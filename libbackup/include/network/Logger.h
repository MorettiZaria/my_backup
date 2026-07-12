#ifndef BACKUP_LOGGER_H
#define BACKUP_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

/// 简单日志类：支持 info/warn/error，输出到文件和/或 stdout
/// fork() 后子进程共享同一个日志文件（追加模式 + 互斥锁）
class Logger {
public:
    /// 获取单例
    static Logger& instance();

    /// 初始化日志（设置文件路径和是否输出到 stdout）
    void init(const std::string& logFile, bool alsoStdout = false);

    /// 是否已初始化
    bool isInitialized() const;

    /// 写日志
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(const std::string& level, const std::string& msg);

    std::ofstream file_;
    bool alsoStdout_ = true;  // 默认输出到 stdout（向后兼容，显式调用 init 后可关闭）
    std::mutex mtx_;
    bool initialized_ = false;
};

#endif // BACKUP_LOGGER_H
