#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace litmgr {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.open(filename, std::ios::app);
    }
    
    void setLevel(LogLevel level) {
        level_ = level;
    }
    
    void log(LogLevel level, const std::string& message) {
        if (level < level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_s(&tm_buf, &time);
        
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        oss << " [" << levelToString(level) << "] " << message << std::endl;
        
        std::cout << oss.str();
        if (logFile_.is_open()) {
            logFile_ << oss.str();
            logFile_.flush();
        }
    }
    
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warning(const std::string& msg) { log(LogLevel::WARNING, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    
private:
    Logger() : level_(LogLevel::INFO) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    LogLevel level_;
    std::ofstream logFile_;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) litmgr::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) litmgr::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) litmgr::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) litmgr::Logger::getInstance().error(msg)

}
