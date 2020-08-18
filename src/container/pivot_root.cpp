#include <sys/mount.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <sstream>
#include <string>

#include "container.hpp"

using namespace std;

// steal the code from https://lwn.net/Articles/800381/
bool PivotRoot(const char* new_root) {
    const char* put_old = "/old_root";

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == 1) {
        fprintf(stderr, "mount-MS_PRIVATE");
        return false;
    }

    // Ensure that 'new_root' is a mount point.
    // MS_REC is necessary as we have volume-mount-point under new_root/
    //
    if (mount(new_root, new_root, NULL, MS_REC | MS_BIND, NULL) == -1) {
        fprintf(stderr, "mount-MS_BIND");
        return false;
    }

    // Create directory to which old root will be pivoted
    ostringstream ss;
    ss << new_root << put_old;
    string put_old_path = ss.str();
    rmdir(put_old_path.c_str());

    if (mkdir(put_old_path.c_str(), 0777) == -1) {
        perror("mkdir");
        return false;
    }

    // And pivot the root filesystem
    if (syscall(SYS_pivot_root, new_root, put_old_path.c_str()) == -1) {
        perror("pivot_root");
        return false;
    }

    // Switch the current working working directory to "/"
    if (chdir("/") == -1) {
        perror("chdir");
        return false;
    }

    // Unmount old root and remove mount point
    if (umount2(put_old, MNT_DETACH) == -1) {
        perror("umount2");
    }

    if (rmdir(put_old) == -1) {
        perror("rmdir");
    }
    return true;
}
