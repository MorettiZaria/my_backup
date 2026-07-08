#include "network/ServerStorage.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>

ServerStorage::ServerStorage(const std::string& basePath) : basePath_(basePath) {
    ensureDir(basePath_);
}

// ===== 路径辅助 =====

std::string ServerStorage::userDir(const std::string& username) const {
    return basePath_ + "/" + username;
}

std::string ServerStorage::backupDir(const std::string& username,
                                      const std::string& backupId) const {
    return userDir(username) + "/" + backupId;
}

bool ServerStorage::ensureDir(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    // 递归创建父目录
    size_t pos = path.rfind('/');
    if (pos != std::string::npos && pos > 0) {
        ensureDir(path.substr(0, pos));
    }
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

int ServerStorage::getNextBackupNumber(const std::string& username) {
    auto backups = listBackups(username);
    int maxNum = 0;
    for (const auto& id : backups) {
        // 格式: "backup_XXXXXX"
        if (id.size() > 7 && id.substr(0, 7) == "backup_") {
            try {
                int num = std::stoi(id.substr(7));
                maxNum = std::max(maxNum, num);
            } catch (...) {}
        }
    }
    return maxNum + 1;
}

// ===== 创建备份 =====

std::string ServerStorage::createBackup(const std::string& username) {
    ensureDir(userDir(username));
    int num = getNextBackupNumber(username);
    std::ostringstream oss;
    oss << "backup_" << std::setw(6) << std::setfill('0') << num;
    std::string backupId = oss.str();
    ensureDir(backupDir(username, backupId));
    return backupId;
}

// ===== 保存 =====

bool ServerStorage::saveHeader(const std::string& username,
                                const std::string& backupId,
                                const std::vector<uint8_t>& headerBytes) {
    std::string path = backupDir(username, backupId) + "/header.bin";
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(headerBytes.data()),
              static_cast<std::streamsize>(headerBytes.size()));
    return out.good();
}

bool ServerStorage::savePayload(const std::string& username,
                                 const std::string& backupId,
                                 const std::vector<uint8_t>& payloadBytes) {
    std::string path = backupDir(username, backupId) + "/payload.bin";
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(payloadBytes.data()),
              static_cast<std::streamsize>(payloadBytes.size()));
    return out.good();
}

bool ServerStorage::saveMetadata(const std::string& username,
                                  const std::string& backupId,
                                  const std::vector<FileInfo>& files) {
    std::string path = backupDir(username, backupId) + "/metadata.bin";
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    auto data = serializer_.serialize(files);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return out.good();
}

// ===== 加载 =====

std::vector<uint8_t> ServerStorage::loadHeader(const std::string& username,
                                                const std::string& backupId) {
    std::string path = backupDir(username, backupId) + "/header.bin";
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    size_t size = static_cast<size_t>(in.tellg());
    in.seekg(0);
    std::vector<uint8_t> data(size);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

std::vector<uint8_t> ServerStorage::loadPayload(const std::string& username,
                                                 const std::string& backupId) {
    std::string path = backupDir(username, backupId) + "/payload.bin";
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    size_t size = static_cast<size_t>(in.tellg());
    in.seekg(0);
    std::vector<uint8_t> data(size);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

std::vector<FileInfo> ServerStorage::loadMetadata(const std::string& username,
                                                   const std::string& backupId) {
    std::string path = backupDir(username, backupId) + "/metadata.bin";
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    size_t size = static_cast<size_t>(in.tellg());
    in.seekg(0);
    std::vector<uint8_t> data(size);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return serializer_.deserialize(data);
}

// ===== 列表 =====

std::vector<std::string> ServerStorage::listBackups(const std::string& username) {
    std::vector<std::string> result;
    std::string udir = userDir(username);
    DIR* dir = opendir(udir.c_str());
    if (!dir) return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        // 检查是否为目录且包含 header.bin
        std::string headerPath = udir + "/" + name + "/header.bin";
        struct stat st;
        if (stat(headerPath.c_str(), &st) == 0) {
            result.push_back(name);
        }
    }
    closedir(dir);

    // 按名称排序（backup_000001 < backup_000002 < ...）
    std::sort(result.begin(), result.end());
    return result;
}

std::string ServerStorage::getLatestBackupId(const std::string& username) {
    auto backups = listBackups(username);
    if (backups.empty()) return "";
    return backups.back();  // 排序后最后一个是最新的
}

time_t ServerStorage::getBackupTimestamp(const std::string& username,
                                          const std::string& backupId) {
    std::string path = backupDir(username, backupId) + "/header.bin";
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

bool ServerStorage::deleteBackup(const std::string& username,
                                  const std::string& backupId) {
    std::string bdir = backupDir(username, backupId);
    // 删除内部文件 + 目录
    std::string files[] = {"header.bin", "payload.bin", "metadata.bin"};
    for (const auto& f : files) {
        std::string fp = bdir + "/" + f;
        unlink(fp.c_str());
    }
    return rmdir(bdir.c_str()) == 0;
}
