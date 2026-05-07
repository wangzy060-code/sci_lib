#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <cstdint>
#include <cstdlib>

namespace litmgr {

struct Config {
    static constexpr size_t MAX_MEMORY_MB = 2048;
    static constexpr size_t BATCH_SIZE = 1000;
    static constexpr size_t CACHE_SIZE = 10000;
    static constexpr size_t MAX_RESULTS = 100;
    
    // 数据目录，优先从环境变量 LITMGMT_HOME 读取，找不到则用相对路径
    static std::string getDataDir() {
        const char* env = std::getenv("LITMGMT_HOME");
        if (env != nullptr && env[0] != '\0') {
            std::string path = env;
            // 移除末尾的 / 或 \，确保统一
            while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
                path.pop_back();
            }
            return path + "/data";
        }
        return "data";
    }
    
    // 也支持单独设置数据目录的环境变量 LITMGMT_DATA_DIR（优先级更高）
    static std::string resolveDir() {
        const char* env = std::getenv("LITMGMT_DATA_DIR");
        if (env != nullptr && env[0] != '\0') {
            std::string path = env;
            while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
                path.pop_back();
            }
            return path;
        }
        // 回退到 LITMGMT_HOME/data
        const char* home = std::getenv("LITMGMT_HOME");
        if (home != nullptr && home[0] != '\0') {
            std::string path = home;
            while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
                path.pop_back();
            }
            return path + "/data";
        }
        return "data";
    }
    
    static std::string getParsedDataFile()   { return resolveDir() + "/parsed_data.bin"; }
    static std::string getAuthorIndexFile()  { return resolveDir() + "/author_index.idx"; }
    static std::string getTitleIndexFile()   { return resolveDir() + "/title_index.idx"; }
    static std::string getKeywordIndexFile() { return resolveDir() + "/keyword_index.idx"; }
    static std::string getKeywordIndexDir()  { return resolveDir() + "/keyword_index"; }
    static std::string getMetadataFile()     { return resolveDir() + "/metadata.json"; }
};

}