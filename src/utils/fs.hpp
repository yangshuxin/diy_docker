#ifndef __FS_HPP__
#define __FS_HPP__

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

bool MkdirP(const char*, mode_t mode);
static inline bool MkdirP(const std::string& d, mode_t mode) { return MkdirP(d.c_str(), mode); }

// On error, the errno is set to 2nd element, and the 1st element
// is ignored. Otherwise, the 2nd element is 0, and 1st element
// take 1/0 value, meaning exist/no-exist.
std::pair<int, int> DirExist(const char*);
static inline std::pair<int, int> DirExist(const std::string& s) {
    return DirExist(s.c_str());
}

#endif
