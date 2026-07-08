#ifndef BACKUP_SERVERSTORAGE_H
#define BACKUP_SERVERSTORAGE_H

#include "core/FileInfo.h"
#include "metadata/MetadataSerializer.h"
#include <string>
#include <vector>
#include <cstdint>

/// 服务器端备份存储：管理每个用户的备份目录和文件
class ServerStorage {
public:
    explicit ServerStorage(const std::string& basePath);

    /// 为指定用户创建新备份，返回 backupId（如 "backup_000001"）
    std::string createBackup(const std::string& username);

    /// 保存备份组件
    bool saveHeader(const std::string& username,
                    const std::string& backupId,
                    const std::vector<uint8_t>& headerBytes);
    bool savePayload(const std::string& username,
                     const std::string& backupId,
                     const std::vector<uint8_t>& payloadBytes);
    bool saveMetadata(const std::string& username,
                      const std::string& backupId,
                      const std::vector<FileInfo>& files);

    /// 读取备份组件
    std::vector<uint8_t> loadHeader(const std::string& username,
                                     const std::string& backupId);
    std::vector<uint8_t> loadPayload(const std::string& username,
                                      const std::string& backupId);
    std::vector<FileInfo> loadMetadata(const std::string& username,
                                        const std::string& backupId);

    /// 列出用户所有备份 ID
    std::vector<std::string> listBackups(const std::string& username);

    /// 获取最新备份 ID（空字符串 = 无备份）
    std::string getLatestBackupId(const std::string& username);

    /// 获取备份时间戳（创建时间）
    time_t getBackupTimestamp(const std::string& username,
                              const std::string& backupId);

    /// 删除指定备份
    bool deleteBackup(const std::string& username, const std::string& backupId);

private:
    std::string basePath_;
    MetadataSerializer serializer_;

    std::string userDir(const std::string& username) const;
    std::string backupDir(const std::string& username, const std::string& backupId) const;
    bool ensureDir(const std::string& path);
    int getNextBackupNumber(const std::string& username);
};

#endif // BACKUP_SERVERSTORAGE_H
