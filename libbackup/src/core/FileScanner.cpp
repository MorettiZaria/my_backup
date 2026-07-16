#include "core/FileScanner.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

std::vector<FileInfo> FileScanner::scan(const std::string& rootPath) {
    std::vector<FileInfo> result;
    scanRecursive(rootPath, "", result);
    return result;
}

void FileScanner::setFilter(const IFileFilter* filter) {
    filter_ = filter;
}

void FileScanner::scanRecursive(const std::string& rootPath,
                                const std::string& currentRelPath,
                                std::vector<FileInfo>& result) {
    std::string fullPath = rootPath;
    if (!currentRelPath.empty()) {
        fullPath += "/" + currentRelPath;
    }

    DIR* dir = opendir(fullPath.c_str());
    if (!dir) {
        std::cerr << "Warning: cannot open directory: " << fullPath << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 . 和 ..
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string entryRelPath = currentRelPath.empty()
            ? entry->d_name
            : currentRelPath + "/" + entry->d_name;
        std::string entryFullPath = rootPath + "/" + entryRelPath;

        struct stat st;
        if (lstat(entryFullPath.c_str(), &st) != 0) {
            std::cerr << "Warning: cannot stat: " << entryFullPath << std::endl;
            continue;
        }

        // 筛选：在被筛选掉的文件上跳过 processEntry（避免读取内容）
        // 目录即使被筛选器拒绝，仍需递归进入以查找匹配的子文件
        bool isDir = S_ISDIR(st.st_mode);
        bool filteredOut = false;
        if (filter_ && !isDir) {
            FileInfo prelim;
            prelim.fromStat(st, entryRelPath);
            if (!filter_->matches(prelim)) {
                continue;
            }
        } else if (filter_ && isDir) {
            FileInfo prelim;
            prelim.fromStat(st, entryRelPath);
            filteredOut = !filter_->matches(prelim);
        }

        if (!filteredOut) {
            FileInfo info = processEntry(entryFullPath, entryRelPath, st);
            result.push_back(info);
        }

        // 如果是目录，始终递归（即使目录本身被筛掉，子文件可能命中）
        if (isDir) {
            scanRecursive(rootPath, entryRelPath, result);
        }
    }

    closedir(dir);
}

FileInfo FileScanner::processEntry(const std::string& fullPath,
                                   const std::string& relPath,
                                   const struct stat& st) {
    FileInfo info;
    info.fromStat(st, relPath);

    mode_t type = st.st_mode & S_IFMT;
    switch (type) {
    case S_IFREG:
        // 普通文件：读取内容
        info.content = readFileContent(fullPath, st.st_size);
        break;

    case S_IFLNK:
        // 符号链接：读取目标路径
        info.symlinkTarget = readSymlinkTarget(fullPath);
        break;

    case S_IFBLK:
    case S_IFCHR:
        // 块设备/字符设备：记录设备号
        info.deviceId = st.st_rdev;
        break;

    case S_IFDIR:
    case S_IFIFO:
    case S_IFSOCK:
        // 目录、管道、套接字：只需元数据，无需额外数据
        break;

    default:
        std::cerr << "Warning: unknown file type for: " << fullPath << std::endl;
        break;
    }

    return info;
}

std::vector<uint8_t> FileScanner::readFileContent(const std::string& path, off_t size) {
    std::vector<uint8_t> content;
    if (size <= 0) return content;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Warning: cannot open file: " << path << std::endl;
        return content;
    }

    content.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(content.data()), size);
    return content;
}

std::string FileScanner::readSymlinkTarget(const std::string& path) {
    char buf[4096];
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (len < 0) {
        return "";
    }
    buf[len] = '\0';
    return std::string(buf);
}
