#include <errno.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysmacros.h> // for makedev()
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

#include "nlohmann/json.hpp"

#include "container.hpp"
#include "fs.hpp"
#include "err_handling.hpp"
#include "utils/clone_exec.hpp"

using namespace std;
using json = nlohmann::json;

//////////////////////////////////////////////////////////////////
//
//      Implementation of ContainerInfo
//
//////////////////////////////////////////////////////////////////
//
const std::string ContainerInfo::_container_base("/var/lib/docker_diy/containers");

ContainerInfo::ContainerInfo(const char* image_name, const char* container_name,
                             const char** cmd_argv, int cmd_argc):
    _image_info(image_name), _virt_net(container_name) {

    setContainerName(container_name);

    // clone argv
    for (int i = 0; i < cmd_argc; i++) {
        _argv.push_back(cmd_argv[i]);
    }
}

ContainerInfo::ContainerInfo(const char* container_name) :
    _name(container_name), _virt_net(container_name) {
    setContainerName(container_name);
}

void
ContainerInfo::setContainerName(const char* container_name) {
    _name = container_name;

    _this_container_path = _container_base + "/" + container_name;
    _upper_dir = _this_container_path + "/upper";
    _work_dir = _this_container_path  + "/workdir";
    _merge_dir = _this_container_path + "/merge";

    _pid_file_path = _this_container_path + "/pid.txt";
    _cmd_file_path = _this_container_path + "/cmd.txt";
}

ContainerInfo::~ContainerInfo() {
    for (auto vol : _volumes) {
        delete vol;
    }
    _volumes.clear();

    for (auto cg : _cgroups) {
        delete cg;
    }
    _cgroups.clear();
}

std::string
ContainerInfo::getContainerPath(const char* name) {
    return _container_base + "/" + name;
}

bool
ContainerInfo::AlreadyExist(const char* name) {
    string path_name = getContainerPath(name);

    auto rc = DirExist(path_name);
    if (rc.second == 0) {
        return rc.first == 0 ? false : true;
    }

    ExitOnErr(string("failed to access ") + path_name, rc.second, -1);
    return false; // just to suppress warning
}

bool
ContainerInfo::isRunning(const char* name) {
    auto root_dir = getContainerPath(name);
    auto pid_file_path = root_dir + "/pid.txt";

    fstream pid_file;
    pid_file.open(pid_file_path);

    if (!pid_file) {
        return false;
    }

    string pid_str;
    if (!getline(pid_file, pid_str)) {
        return false;
    }

    auto proc_path = "/proc/" + pid_str + "/";
    auto rc = DirExist(proc_path);

    // TODO: need to check if the pid the the container proc
    return !rc.second && rc.first;
}

void
ContainerInfo::serialize() {
    json j_info;

    // step 1: write down the cmd as well as args
    {
        json j_argv = json::array();
        //for (int i = 0; i < _argc; i++) { j_argv.push_back(_argv[i]); }
        j_info["argv"] = _argv;
    }

    // step 2: write image info
    j_info["image"] = {{"repo", _image_info.getRepo().c_str()},
                       {"tag", _image_info.getTag().c_str()}};

    // step 3: write volume spec strings
    vector<string> vol_specs;
    for (const auto _iter : _volumes) {
        if (_iter->IsFile()) continue;
        string vol_spec;
        _iter->GetSpecString(vol_spec);
        vol_specs.push_back(vol_spec);
    }
    j_info["volumes"] = vol_specs;

    // TODO: serialize cgroup info

    // finaly, write to file
    ofstream info_file;

    auto info_file_path = _get_info_file_path();
    info_file.open(info_file_path, fstream::trunc);

    if (!info_file) {
        ExitOnErr("failed to open " + info_file_path, errno, -1);
    }

    info_file << j_info;

    info_file.close();
}

std::string
ContainerInfo::_get_info_file_path() {
    return getContainerPath() + "/info.json";
}

bool
ContainerInfo::deserialize() {
    ASSERT0(!_this_container_path.empty());

    ifstream info_file(_get_info_file_path());

    json j_info;
    info_file >> j_info;

    if (!j_info.contains("argv") || !j_info.contains("image")) {
        cerr << "WTF!" << endl;
        return false;
    }

    // restore argv
    {
        auto j_argv = j_info["argv"];

        _argv.clear();
        int argc = j_argv.size();
        for (int i = 0; i < argc; i++) {
            _argv.push_back(j_argv[i]);
        }
    }

    // restore image info
    {
        json image_info = j_info["image"];
        _image_info.Set(image_info["repo"].get<string>(),
                        image_info["tag"].get<string>());
    }

    // restore volume info
    ASSERT0(!_merge_dir.empty());
    if (j_info.contains("volumes")) {
        auto vols = j_info["volumes"];

        int vol_cnt = int(vols.size());
        for (int i = 0; i < vol_cnt; i++) {
            _volumes.push_back(new Volume(vols[i], _merge_dir));
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////
//
//      Implementation of ContainerInstance
//
//////////////////////////////////////////////////////////////////
//
ContainerInstance* ContainerInstance::_inst = 0;

ContainerInstance::ContainerInstance(const char* image_name,
    const char* container_name, const char** cmd_argv, int cmd_argc) :
    ContainerInfo(image_name, container_name, cmd_argv, cmd_argc) {
    ASSERT(!_inst, "multiple ContainerInstances");
    _root_mounted = false;
    _ready_to_run = false;
    _add_files_to_be_mounted();

    _inst = this;
}

ContainerInstance::ContainerInstance(const char* container_name)
    : ContainerInfo(container_name) {
    ASSERT(!_inst, "multiple ContainerInstances");
    _root_mounted = false;
    _ready_to_run = false;
    _add_files_to_be_mounted();

    _inst = this;
}

void
ContainerInstance::exitIfNameInvalid(const string& container_name) {
    if (container_name.size() >= 10) {
        // the veth name is "veth{0|1}_<container_name", and size of
        // device name should be under 16 (i.e. IF_NAMESIZE) byte.
        cerr << "container name should have no more than 9 chars\n";
        exit(1);
    }

    // container name is used to compose some directories, that is why.
    for (auto c : container_name) {
        if (((c >= 'a') && (c <= 'z')) ||
            ((c >= 'A') && (c <= 'Z')) ||
            ((c >= '1') && (c <= '9')) ||
            (c == '_')) {
            continue;
        }
        cerr << "container name should not contain char other than [1-9a-zA-Z_]\n";
        exit(1);
    }
}

void
ContainerInstance::_add_files_to_be_mounted() {
    const char* md = _merge_dir.c_str();

    string resolv_file;

    {
        string systemd_resolv = "/run/systemd/resolve/resolv.conf";
        struct stat buffer;
        resolv_file = (stat (systemd_resolv.c_str(), &buffer) == 0) ?
                       systemd_resolv : "/etc/resolv.conf";
    }

    auto v1 = new Volume(resolv_file + ":/etc/resolv.conf", md,
                         true /* is regular file*/);
    auto v2 = new Volume("/etc/hostname:/etc/hostname", md, true);
    auto v3 = new Volume("/etc/hosts:/etc/hosts", md, true);

    _volumes.push_back(v1);
    _volumes.push_back(v2);
    _volumes.push_back(v3);
}

void
ContainerInstance::Cleanup() {
    UmountVolumes();

    if (_root_mounted) {
        UmountRoot();
    }
}

bool
ContainerInstance::UnionMountRoot() {
    if (_root_mounted) return true;

    ostringstream ss;
    ss << "lowerdir=" << getImageDir() << ","
       << "upperdir=" << getUpperDir() << ","
       << "workdir=" << getWorkDir();

    // cout << getMergeDir() << endl;
    // cout << ss.str() << endl;

    int rc = ::mount(0, getMergeDir().c_str(), "overlay", 0, ss.str().c_str());
    if (rc != 0) {
        ExitOnErr("Overlay union mount root", errno, -1);
        return false;
    }

    _root_mounted = true;
    return true;
}

bool
ContainerInstance::UmountRoot(bool force) {
    if (!_root_mounted && !force) {
        return true;
    }

    _root_mounted = false;
    return ::umount(getMergeDir().c_str()) == 0;
}

bool
ContainerInstance::InitDirsAndFiles() {
    // step 1: create overlay2 dirs
    const char* dirs[] = {
        getUpperDir().c_str(),
        getWorkDir().c_str(),
        getMergeDir().c_str()
    };

    for (int i = 0; i < int(sizeof(dirs)/sizeof(dirs[0])); i++) {
        if (!MkdirP(dirs[i], 0755)) {
            cerr << "failed to create dir: " << dirs[i] << endl;
            return false;
        }
    }

    // step 2: write down pid file
    {
        ofstream pid_file;
        pid_file.open(_pid_file_path, fstream::trunc);

        if (!pid_file) {
            ExitOnErr("failed to open file: " + _pid_file_path,
                      errno, -1);
        }

        pid_file << getpid();
        pid_file.close();
    }

    // step 3: create volume mount point in the upper layer
    CreateVolumeMountPoints();

    // step 4: serialize other info into a file
    serialize();
    return true;
}


static int
run_cmd_in_docker(void* arg) {
    char* const* argv = (char* const*)arg;

    const ContainerInstance* inst = ContainerInstance::GetInstance();
    const int* fd = inst->GetPipeFd();

    // step1: receive instruction from parent process
    {
        close(fd[1]); // child process is read-only
        int buf_sz = 512;
        char* buf = new char[buf_sz];
        if (!buf) {
            return -1;
        }

        int count = read(fd[0], buf, buf_sz - 1);
        if (count < 0) { return -1; }

        if (!::strncmp(buf, "go", 2)) {
            // proceed
        } else if (!::strncmp(buf, "die", 3)) {
            return -1;
        } else {
            // unknown command
            return -1;
        }
    }

    const char* merge_root = inst->getMergeDir().c_str();

    // step 2: pivot root
    if (!PivotRoot(merge_root)) {
        return -1;
    }

    int rc;
#if 0
    // make /proc private
    rc = mount(0, "/proc", 0, MS_REC | MS_PRIVATE, 0);
    if (rc != 0) {
        perror("private mount");
        return 0;
    }
#endif

#if 1
    rc = mount("proc", "/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, 0);
    if (rc != 0) {
        perror("mount proc/");
        return 0;
    }
#endif

    rc = mount("tmpfs", "/dev", "tmpfs", MS_NOEXEC | MS_NOSUID, 0);
    if (rc != 0) {
        perror("mount tmpfs");
        return 0;
    }

    // create some devices, I don't know if this is the right way to be.
    if (mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3))) {
        perror("mknod /dev/null");
    }

    if (mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5))) {
        perror("mknod /dev/zero");
    }

    // step 3: exec the cmd
    const char* default_cmd_argv[] = {"/bin/sh", 0};
    const char** cmd_argv = argv ? (const char**)argv : default_cmd_argv;

    return execv(cmd_argv[0], (char* const*)cmd_argv);
}

//////////////////////////////////////////////////////////////////////////
//
//          Functions about volume
//
//////////////////////////////////////////////////////////////////////////
//
bool
ContainerInstance::AddVolume(const char* volume_spec) {
    if (!Volume::IsValidSpec(volume_spec)) {
        return false;
    }

    ASSERT0(!_merge_dir.empty());
    _volumes.push_back(new Volume(volume_spec, _merge_dir.c_str()));
    return true;
}

bool
ContainerInstance::UmountVolumes() {
    bool succ = true;
    for (auto vol : _volumes) {
        if (!vol->Umount()) {
            succ = false;
        }
    }
    return succ;
}

void
ContainerInstance::MountVolumes() {
    for (auto vol : _volumes) {
        if (!vol->Mount()) {
            ostringstream ss;
            ss << "failed to mount volume " << vol->GetSrcDir() << "->"
               << vol->GetDestDirInContainer();
            ExitOnErr(ss.str(), errno, -1);
        }
    }
}

void
ContainerInstance::CreateVolumeMountPoints() {
    ASSERT0(!_upper_dir.empty());

    for (auto vol : _volumes) {
        bool succ = vol->CreateMountPoint(_upper_dir.c_str(),
                                          true/*exit_if_failed*/);
        ASSERT0(succ);
    }
}

bool
ContainerInstance::_init_before_childproc_start(int child_pid) {
    // apply cgroup
    if (!_ApplyCgroups(child_pid)) {
        return false;
    }

    if (!_virt_net.Bringup(child_pid)) {
        return false;
    }

    return true;
}

#define CHILD_STACK_SZ (1024 * 1024)

int
ContainerInstance::_clone_exec(int clone_flags, const char* argv,
                               int (*child_fn)(void*)) {
    ASSERT0(child_fn);
    if (pipe(_pipe_fd)) {
        ExitOnErr("pipe", errno, -1);
    }

    shared_ptr<char> stack(new char[CHILD_STACK_SZ], default_delete<char[]>());
    int rc = clone(child_fn,
                   stack.get() + CHILD_STACK_SZ, clone_flags,
                   (void*)argv);
    if (rc == -1) {
        ExitOnErr("clone", errno, -1);
        return -1;
    }

    bool succ = _init_before_childproc_start(rc);

    // parent process is write-only, close the read-fd
    close(_pipe_fd[0]);

    // inform child process to move forward
    if (succ) {
        string cmd = "go";
        write(_pipe_fd[1], cmd.c_str(), cmd.size());
    } else {
        string cmd = "die";
        write(_pipe_fd[1], cmd.c_str(), cmd.size());
    }

    int exit_status;
    int w = waitpid(rc, &exit_status, 0);
    if (w == -1) {
        perror("waitpid");
    }
    cout << "child finish with exist status: " << exit_status << endl;

    // todo return 0 if child process finish successfully
    return -1;
}

bool
ContainerInstance::_CreateCGroups() {
    for (auto cg : _cgroups) {
        if (!cg->Create()) {
            return false;
        }
    }
    return true;
}

bool
ContainerInstance::_ApplyCgroups(int pid) {
    for (auto cg : _cgroups) {
        if (!cg->Apply((pid_t)pid)) {
            return false;
        }
    }
    return true;
}

bool
ContainerInstance::run() {
    if (!UnionMountRoot()) {
        return false;
    }

    MountVolumes();
    _CreateCGroups();

    // fork child process and run the specified command
    int rc;

    vector<string> default_argv;
    default_argv.push_back("/bin/sh");

    vector<string>* argv = _argv.empty() ? &default_argv: &_argv;

    const char** child_argv = new const char*[argv->size() + 2];

    for (int i = 0; i < (int)argv->size(); i++) {
        child_argv[i] = (*argv)[i].c_str();
    }
    child_argv[argv->size()] = 0;

    int flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS |
                CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD;
    rc = _clone_exec(flags, (const char*)(void*)child_argv, run_cmd_in_docker);
    delete[] child_argv;

    return rc == 0 ? true : false;
}

ContainerInstance::~ContainerInstance() {
    Cleanup();
    _inst = 0;
}
