#include <string>
#include <string.h>
#include "image.hpp"

using namespace std;

const string ImageInfo::_image_base("/var/lib/docker_diy/images");

void
ImageInfo::Set(const char* repo, const char* tag) {
    _repo = repo;
    _tag = tag;
    _path = _image_base + "/" + repo + "/" + tag;
}

void
ImageInfo::Set(const char* repo_tag_combo) {
    const char* p = ::strchr(repo_tag_combo, int(':'));
    if (p) {
        _repo = string(repo_tag_combo, p - repo_tag_combo);
        _tag = ++p;
    } else {
        _repo = repo_tag_combo;
        _tag = "";
    }
    if (_tag.size() == 0) {
        _tag = "latest";
    }
    _path = _image_base + "/" + _repo + "/" + _tag;
}
