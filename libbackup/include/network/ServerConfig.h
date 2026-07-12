#ifndef BACKUP_SERVERCONFIG_H
#define BACKUP_SERVERCONFIG_H

#include <string>
#include <cstdint>
#include <map>

/// INI 风格的服务器配置文件解析器
class ServerConfig {
public:
    ServerConfig();

    /// 从文件加载配置，失败抛异常或返回 false
    bool load(const std::string& path);

    /// 写入示例配置文件
    static bool writeExample(const std::string& path);

    // ---- 访问器 ----
    uint16_t port() const;
    std::string storagePath() const;
    std::string logFile() const;
    int maxConnections() const;

    // 修改器（CLI 参数覆盖用）
    void setPort(uint16_t p);
    void setStoragePath(const std::string& p);
    void setLogFile(const std::string& p);
    void setMaxConnections(int n);

private:
    uint16_t port_ = 8848;
    std::string storagePath_ = "./server_data";
    std::string logFile_;  // 空 = 不写文件，仅 stdout
    int maxConnections_ = 100;

    static std::string trim(const std::string& s);
};

#endif // BACKUP_SERVERCONFIG_H
