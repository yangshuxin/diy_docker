#ifndef __IMAGE_HPP__
#define __IMAGE_HPP__

#include <string>

class ImageInfo {
public:
    ImageInfo(const char* repo, const char* tag) { Set(repo, tag); }
    ImageInfo(const char* repo_tag_combo) { Set(repo_tag_combo); }

    ImageInfo() {};
    ~ImageInfo() {}

    static const std::string& getImageBase() { return _image_base; }

    const std::string& getRepo() const { return _repo; }
    const std::string& getTag() const { return _tag; }
    // return the dir of this specific image
    const std::string& getPath() const { return _path; }

    void Set(const char* repo, const char* tag);
    void Set(const std::string& repo, const std::string& tag) {
        Set(repo.c_str(), tag.c_str());
    }
    void Set(const char* repo_tag_combo);
    void Set(const std::string& repo_tag_combo) {
        Set(repo_tag_combo.c_str());
    }

private:
    std::string _repo;
    std::string _tag;
    std::string _path;

    // parent/base dir for all images
    static const std::string _image_base;
};

#endif //__IMAGE_HPP__
