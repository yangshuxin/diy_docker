#ifndef __CGROUP_HPP__
#define __CGROUP_HPP__

#include <sys/types.h> // for pid_t
#include <string>

class CgroupBase {
public:
    typedef enum {
        CG_CPU = 1,
        CG_CPUSET = 2,
        CG_MEMORY = 3
    } CGTy;

    CgroupBase(const char* name, CgroupBase::CGTy ty) : _ty(ty), _name(name) {}
    virtual ~CgroupBase() {}

    virtual bool Create() = 0;
    virtual bool Remove(bool force=false) = 0;
    virtual bool Apply(pid_t pid) = 0;

    const std::string& GetName() { return _name; }

    // get cgroup hierachy mount point
    static void GetMountPoint();
    static const std::string& GetMemMountPoint();

protected:
    CGTy _ty;
    std::string _name;

    static std::string _mem_mnt_point;
};

class CgroupMem : public CgroupBase {
public:
    CgroupMem(const char* name, const char* limit);
    virtual ~CgroupMem() { Remove(); }

    bool Create();
    bool Apply(pid_t pid);
    bool Remove(bool force=false);

private:
    std::string _path;
    std::string _limit;
    bool _created;
};

#endif
