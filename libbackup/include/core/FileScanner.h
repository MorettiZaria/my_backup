#ifndef BACKUP_FILESCANNER_H
#define BACKUP_FILESCANNER_H

#include "FileInfo.h"
#include "filter/IFileFilter.h"
#include <string>
#include <vector>
#include <sys/stat.h>

/**
 * 递归遍历目录树，识别所有文件类型，收集 FileInfo 列表
 * 支持 7 种文件类型：普通文件、目录、符号链接、管道、块设备、字符设备、Unix 域套接字
 */
class FileScanner {
public:
    /// 扫描目录，返回所有文件的信息列表（含普通文件的内容数据）
    std::vector<FileInfo> scan(const std::string& rootPath);

    /// 设置文件筛选器（nullptr 或空 CompositeFilter = 不筛选）
    void setFilter(const IFileFilter* filter);

private:
    const IFileFilter* filter_ = nullptr;

    /// 递归遍历
    void scanRecursive(const std::string& rootPath,
                       const std::string& currentRelPath,
                       std::vector<FileInfo>& result);

    /// 对单个目录项调用 lstat 并填充 FileInfo
    FileInfo processEntry(const std::string& fullPath,
                          const std::string& relPath,
                          const struct stat& st);

    /// 读取普通文件全部内容
    std::vector<uint8_t> readFileContent(const std::string& path, off_t size);

    /// 读取符号链接目标路径
    std::string readSymlinkTarget(const std::string& path);
};

#endif // BACKUP_FILESCANNER_H
