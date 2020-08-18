#include <sys/mount.h>
#include <sstream>
#include <fstream>

#include "volume.hpp"
#include "err_handling.hpp"
#include "fs.hpp"

using namespace std;

Volume::Volume(const char* vol_spec, const char* merged_root, bool is_file):
    _merged_root(merged_root), _mount_option(0), _is_file(is_file)
{
    ASSERT0(IsValidSpec(vol_spec));

    string field;
    string mount_options;

    ASSERT0(IsValidSpec(vol_spec));
    (void)_decompose(vol_spec, _src, _dest, mount_options);
    _mount_option = _cvtMountOption(mount_options);
}

bool
Volume::_decompose(const char* volume_spec, std::string& src,
                   std::string& dest, std::string& mount_options) {
    src = "";
    dest = "";
    mount_options = "";

    std::stringstream ss(volume_spec);
    string field;

    for (int field_idx = 0; std::getline(ss, field, ':'); field_idx++) {
        switch (field_idx) {
        case 0: src = field; break;
        case 1: dest = field; break;
        case 2: mount_options = field; break;
        default:
            return false;
        }
    }
    return true;
}

int
Volume::_cvtMountOption(const std::string& opt) {
    if (opt.empty()) { return 0; }
    if (opt.compare("ro") == 0) { return (int)VOL_RD_ONLY; }
    return -1;
}

bool
Volume::IsValidSpec(const char* vol_spec) {
    string src, dest, opts;

    if (!_decompose(vol_spec, src, dest, opts)) {
        return false;
    }

    if (src.empty() || dest.empty()) {
        return false;
    }

    return _cvtMountOption(opts) >= 0;
}

void
Volume::GetSpecString(string& result) const {
    stringstream ss;

    ss << _src << ":" << _dest;
    if (_mount_option & VOL_RD_ONLY) {
        ss << ":ro";
    }

    result = ss.str();
}


// create mount-point dir in the overlay2's upper layer
bool
Volume::_CreateMountPointHelper(const char* overlay2_upperdir) {
    ostringstream ss;
    ss << overlay2_upperdir << "/" << _dest;

    string full_path = ss.str();
    string full_path_dir(full_path);

    if (_is_file) {
        auto pos = full_path.find_last_of('/');
        if (pos == string::npos) {
            return false;
        }
        full_path_dir = full_path.substr(0, pos);
    }

    auto rc = DirExist(full_path_dir);
    if (rc.second != 0) {
        // something wrong occured
        return false;
    }

    if (!rc.first) {
        // the dir does not exist
        if (!MkdirP(full_path_dir, 0755)) {
            return false;
        }
    }

    if (_is_file) {
        // create empty file
        ofstream empty_f;
        empty_f.open(full_path);
        if (!empty_f) {
            return false;
        }
        empty_f.close();
    }

    return true;
}

bool
Volume::CreateMountPoint(const char* overlay2_upperdir, bool exit_if_failed) {
    bool succ = _CreateMountPointHelper(overlay2_upperdir);
    if (succ) {
        return true;
    }

    ExitOnErr("CreateMountPoint failed:", errno, -1);
}

bool
Volume::Mount() {
    int mount_flags = MS_BIND;
    if (_mount_option & VOL_RD_ONLY) {
        mount_flags |= MS_RDONLY;
    }

    ostringstream ss;
    ss << _merged_root << "/" << _dest;
    auto dest_path = ss.str();

    int rc = ::mount(_src.c_str(), dest_path.c_str(), 0, mount_flags, 0);
    return rc == 0;
}

bool
Volume::Umount() {
    ostringstream ss;
    ss << _merged_root << "/" << _dest;

    return umount(ss.str().c_str()) == 0;
}
