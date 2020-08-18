#ifndef __VOLUME_HPP__
#define __VOLUME_HPP__

#include <string>

class Volume {
public:
    Volume(const char* vol_spec, const char* merged_root, bool is_file=false);
    Volume(const std::string& vol_spec,
           const std::string& merged_root, bool is_file=false) :
        Volume(vol_spec.c_str(), merged_root.c_str(), is_file) {}

    bool CreateMountPoint(const char* overlay2_upperdir, bool exit_if_failed=true);

    bool Mount();
    bool Umount();

    static bool IsValidSpec(const char* vol_spec);
    static bool IsValidSpec(const std::string& vol_spec) {
        return IsValidSpec(vol_spec.c_str());
    }

    // return src:dest:option string
    void GetSpecString(std::string& result) const;

    const std::string& GetSrcDir() const { return _src; }
    const std::string& GetDestDirInContainer() const { return _dest; }

    bool IsFile() const { return _is_file; }

private:
    static bool _decompose(const char* volume_spec, std::string& src,
                           std::string& dest, std::string& mnt_opt);

    static int _cvtMountOption(const std::string& opt);
    bool _CreateMountPointHelper(const char* overlay2_upperdir);

private:
    enum {
        VOL_RD_ONLY = 1,
    };

    std::string _src;
    std::string _dest;
    std::string _merged_root;

    int _mount_option;
    bool _is_file;
};

#endif // __VOLUME_HPP__
