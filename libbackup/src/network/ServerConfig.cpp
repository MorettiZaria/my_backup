#include "network/ServerConfig.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

ServerConfig::ServerConfig() {}

bool ServerConfig::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Warning: cannot open config file '" << path
                  << "', using defaults." << std::endl;
        return false;
    }

    std::string line;
    std::string section;
    int lineNo = 0;

    while (std::getline(f, line)) {
        lineNo++;
        line = trim(line);

        // 跳过空行和注释
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // 节标题
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        // 键值对
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;  // 跳过无效行
        }

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        // 移除值两侧的引号
        if (val.size() >= 2 && ((val[0] == '"' && val.back() == '"') ||
                                 (val[0] == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        if (section == "server") {
            if (key == "port") {
                try { port_ = static_cast<uint16_t>(std::stoi(val)); }
                catch (...) { std::cerr << "Warning: invalid port at line " << lineNo << std::endl; }
            } else if (key == "storage_path") {
                storagePath_ = val;
            } else if (key == "log_file") {
                logFile_ = val;
            } else if (key == "max_connections") {
                try { maxConnections_ = std::stoi(val); }
                catch (...) { std::cerr << "Warning: invalid max_connections at line " << lineNo << std::endl; }
            }
        }
    }

    return true;
}

bool ServerConfig::writeExample(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: cannot write config to '" << path << "'" << std::endl;
        return false;
    }

    f << "# Backup Server Configuration\n"
      << "# Lines starting with # or ; are comments.\n"
      << "\n"
      << "[server]\n"
      << "# Listening port\n"
      << "port = 8848\n"
      << "\n"
      << "# Storage directory for backups and user database\n"
      << "storage_path = ./server_data\n"
      << "\n"
      << "# Log file path (leave empty for stdout only)\n"
      << "log_file = /var/log/backup-server.log\n"
      << "\n"
      << "# Maximum concurrent connections\n"
      << "max_connections = 100\n";

    return true;
}

// ===== 访问器 =====

uint16_t ServerConfig::port() const { return port_; }
std::string ServerConfig::storagePath() const { return storagePath_; }
std::string ServerConfig::logFile() const { return logFile_; }
int ServerConfig::maxConnections() const { return maxConnections_; }

void ServerConfig::setPort(uint16_t p) { port_ = p; }
void ServerConfig::setStoragePath(const std::string& p) { storagePath_ = p; }
void ServerConfig::setLogFile(const std::string& p) { logFile_ = p; }
void ServerConfig::setMaxConnections(int n) { maxConnections_ = n; }

// ===== 辅助 =====

std::string ServerConfig::trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}
