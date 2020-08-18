#ifndef __CONTAINER_HPP__
#define __CONTAINER_HPP__

#include <string>
#include <vector>

#include "image.hpp"
#include "volume.hpp"
#include "cgroup.hpp"
#include "virtual_net.hpp"

class ContainerInfo {
public:
    ContainerInfo(const char* image_name, const char* container_name,
                  const char** cmd_argv, int cmd_argc);

    ContainerInfo(const char* container_name);
    ~ContainerInfo();

    // about overlay2 fs
    const std::string& getImageDir() const { return _image_info.getPath(); }
    const std::string& getUpperDir() const { return _upper_dir; }
    const std::string& getWorkDir() const { return _work_dir; }
    const std::string& getMergeDir() const { return _merge_dir; }

    const std::string& getName() const { return _name; }
    const std::string& getContainerPath() const { return _this_container_path; }

    static std::string getContainerPath(const char* name);
    static bool AlreadyExist(const char* name);
    static bool isRunning(const char* name);

    void AddCgroup(CgroupBase* cg) { _cgroups.push_back(cg); }

    void serialize(); // serialize into info.json file
    bool deserialize(); // populate fields from info.json file

protected:
    std::string _get_info_file_path();
    void setContainerName(const char* container_name);

protected:
    static const std::string _container_base;

    std::string _name;
    std::string _this_container_path;

    // about overlay2
    std::string _upper_dir,
                _work_dir,
                _merge_dir;

    ImageInfo _image_info;

    std::string _pid_file_path,
                _cmd_file_path;

    std::vector<std::string> _argv;
    std::vector<Volume*> _volumes;
    std::vector<CgroupBase*> _cgroups;
    VirtualNetwork _virt_net;
};

class ContainerInstance : public ContainerInfo {
public:
    ContainerInstance(const char* image_name, const char* container_name,
                      const char** cmd_argv, int cmd_argc);
    ContainerInstance(const char* container_name);
    ~ContainerInstance();

    bool UnionMountRoot();
    bool UmountRoot(bool force=false);
    void Cleanup();

    bool InitDirsAndFiles();
    void setReadyToRun() { _ready_to_run = true; }

    // about volume
    bool AddVolume(const char* volume_spec);
    bool AddVolume(const std::string& vs) { return AddVolume(vs.c_str()); }
    void MountVolumes();
    bool UmountVolumes();
    void CreateVolumeMountPoints();

    // run the container
    bool run();

    static const ContainerInstance* GetInstance() { return _inst; }

    const int* GetPipeFd() const { return _pipe_fd; }

    static void exitIfNameInvalid(const std::string&);
private:
    bool _CreateCGroups();
    bool _ApplyCgroups(int pid);

    int _clone_exec(int clone_flags, const char* argv,
                    int (*child_fn)(void*));

    bool _init_before_childproc_start(int child_pid);

    void _add_files_to_be_mounted();

private:
    static ContainerInstance* _inst;

    bool _root_mounted;
    bool _ready_to_run;

    int _pipe_fd[2];
};

bool PivotRoot(const char* new_root);

#endif //__CONTAINER_HPP__
