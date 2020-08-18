#include <unistd.h> // for rmdir

#include <sys/stat.h> // for mkdir
#include <sys/types.h>

#include <sstream>
#include <fstream>
#include "cgroup.hpp"
#include "err_handling.hpp"

using namespace std;

///////////////////////////////////////////////////////////////////////////
//
//      Implementation of CgroupBase
//
///////////////////////////////////////////////////////////////////////////
//

string CgroupBase::_mem_mnt_point;

void
CgroupBase::GetMountPoint() {
    // TODO: get mount point by reading /proc/self/mountinfo
    if (!_mem_mnt_point.empty()) {
        return;
    }
    _mem_mnt_point = "/sys/fs/cgroup/memory";
}

const string&
CgroupBase::GetMemMountPoint() {
    GetMountPoint();
    return _mem_mnt_point;
}

///////////////////////////////////////////////////////////////////////////
//
//      Implementation of CgroupMem
//
///////////////////////////////////////////////////////////////////////////
//
CgroupMem::CgroupMem(const char* name, const char* limit) :
    CgroupBase(name, CgroupBase::CG_MEMORY), _limit(limit) {
    _path = GetMemMountPoint() + "/" + _name;
    _created = false;
}

bool
CgroupMem::Create() {
    (void)Remove(true);

    if (::mkdir(_path.c_str(), 0755) != 0) {
        ExitOnErr("failed to mkdir: " + _path, errno, -1);
    }

    string limit_file_path = _path + "/memory.limit_in_bytes";

    ofstream limit_file;
    limit_file.open(limit_file_path);

    if (!limit_file) {
        ExitOnErr("failed to open file: " + limit_file_path, errno, -1);
    }

    limit_file << _limit;
    limit_file.close();

    _created = true;
    return true;
}

bool
CgroupMem::Remove(bool force) {
    if (!force && !_created) {
        return true;
    }

    int rc = ::rmdir(_path.c_str());
    if (rc == 0 || errno == ENOENT) {
        _created = false;
    }
    return rc == 0;
}

bool
CgroupMem::Apply(pid_t pid) {
    stringstream ss;
    ss << GetMemMountPoint() << "/" << _name << "/tasks";

    auto task_file_path = ss.str();

    ofstream task_file;
    task_file.open(task_file_path);

    if (!task_file) {
        ExitOnErr("faied to open " + task_file_path, errno, -1);
    }

    task_file << pid;
    task_file.close();

    return true;
}
