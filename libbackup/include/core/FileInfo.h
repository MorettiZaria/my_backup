#ifndef BACKUP_FILEINFO_H
#define BACKUP_FILEINFO_H

#include <cstdint>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

/**
 * 存储单个文件的全部信息（数据 + 元数据）
 * 支持 7 种文件类型：普通文件、目录、符号链接、管道、块设备、字符设备、套接字
 */
class FileInfo {
public:
    std::string  relativePath;    // 相对于备份根目录的路径
    mode_t       fileType;        // S_IFREG / S_IFDIR / S_IFLNK / S_IFIFO / S_IFBLK / S_IFCHR / S_IFSOCK
    mode_t       permissions;     // 权限位 (0777 等)
    uid_t        owner;           // 属主 UID
    gid_t        group;           // 属组 GID
    off_t        fileSize;        // 文件大小（普通文件）；特殊文件为 0
    time_t       atime;           // 最后访问时间
    time_t       mtime;           // 最后修改时间
    time_t       ctime;           // 状态变更时间
    std::string  symlinkTarget;   // 符号链接目标路径（仅 S_IFLNK）
    dev_t        deviceId;        // 设备号（仅 S_IFBLK / S_IFCHR）
    std::vector<uint8_t> content; // 文件数据（仅普通文件，备份时读入）

    FileInfo();

    /// 从 struct stat 填充元数据字段
    void fromStat(const struct stat& st, const std::string& relPath);

    /// 序列化：FileInfo → 字节流（用于存入备份文件头部）
    std::vector<uint8_t> serialize() const;

    /// 反序列化：字节流 → FileInfo。offset 会被推进到读取结束位置
    static FileInfo deserialize(const uint8_t* data, size_t& offset);

    /// 是否为普通文件
    bool isRegular() const;
    /// 是否为目录
    bool isDirectory() const;
    /// 是否为符号链接
    bool isSymlink() const;
    /// 是否为命名管道（FIFO）
    bool isFifo() const;
    /// 是否为块设备
    bool isBlockDevice() const;
    /// 是否为字符设备
    bool isCharDevice() const;
    /// 是否为 Unix 域套接字
    bool isSocket() const;
    /// 是否为设备文件（块设备或字符设备）
    bool isDevice() const;
};

#endif // BACKUP_FILEINFO_H
