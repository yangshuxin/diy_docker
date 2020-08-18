#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <utility> // for std::pair
#include <sstream>

#include "fs.hpp"

using namespace std;

pair<int, int>
DirExist(const char* path_name) {
    auto rc = make_pair(0, 0);

    struct stat info;
    if (stat(path_name, &info)) {
        if (errno != ENOENT) {
            rc.second = errno;
        }
    } else if (info.st_mode & S_IFDIR) {
        rc.first = 1;
    }

    return rc;
}

bool
MkdirP(const char* dir_path, mode_t mode) {
    std::string current_level = "";
    std::string level;
    std::stringstream ss(dir_path);

    if (dir_path[0] == '/') {
        if (!std::getline(ss, level, '/')) {
            return false;
        }
        current_level = "/";
    }

    for (;std::getline(ss, level, '/'); current_level += "/") {
        current_level += level;

        auto rc = DirExist(current_level);
        if (rc.second != 0) { return false; }
        if (rc.first == 1) { continue; }

        if (::mkdir(current_level.c_str(), 0755) != 0) {
            perror("mkdir");
            return false;
        }
    }
    return true;
}
