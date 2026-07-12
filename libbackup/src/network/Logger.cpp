#include "network/Logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::init(const std::string& logFile, bool alsoStdout) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_.is_open()) {
        file_.close();
    }
    file_.open(logFile, std::ios::app);
    alsoStdout_ = alsoStdout;
    initialized_ = true;
}

bool Logger::isInitialized() const {
    return initialized_;
}

void Logger::info(const std::string& msg) {
    log("INFO", msg);
}

void Logger::warn(const std::string& msg) {
    log("WARN", msg);
}

void Logger::error(const std::string& msg) {
    log("ERROR", msg);
}

void Logger::log(const std::string& level, const std::string& msg) {
    // 生成时间戳
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_now{};
    localtime_r(&time_t_now, &tm_now);

    std::ostringstream oss;
    oss << "["
        << std::setfill('0') << std::setw(4) << (tm_now.tm_year + 1900) << "-"
        << std::setfill('0') << std::setw(2) << (tm_now.tm_mon + 1) << "-"
        << std::setfill('0') << std::setw(2) << tm_now.tm_mday << " "
        << std::setfill('0') << std::setw(2) << tm_now.tm_hour << ":"
        << std::setfill('0') << std::setw(2) << tm_now.tm_min << ":"
        << std::setfill('0') << std::setw(2) << tm_now.tm_sec << "."
        << std::setfill('0') << std::setw(3) << ms.count()
        << "] [" << level << "] " << msg;

    std::string line = oss.str();

    std::lock_guard<std::mutex> lock(mtx_);
    if (file_.is_open()) {
        file_ << line << std::endl;
    }
    if (alsoStdout_ || !file_.is_open()) {
        // 未配置文件时写入 stdout（向后兼容），或显式要求 stdout
        std::cout << line << std::endl;
    }
}
