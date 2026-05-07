#pragma once

#include "../core/literature.h"
#include "../utils/logger.h"
#include "../utils/string_utils.h"
#include "../core/config.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <mutex>

namespace litmgr {

class TitleIndex {
public:
    TitleIndex() : totalEntries_(0) {}
    
    void addTitle(const std::string& title, uint64_t litId) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t hash = StringUtils::hashString(title);
        index_[hash] = litId;
        totalEntries_++;
    }
    
    bool search(const std::string& title, uint64_t& litId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t hash = StringUtils::hashString(title);
        
        auto it = index_.find(hash);
        if (it != index_.end()) {
            litId = it->second;
            return true;
        }
        return false;
    }
    
    std::vector<uint64_t> searchPartial(const std::string& partialTitle) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string normalizedPartial = StringUtils::toLower(StringUtils::trim(partialTitle));
        std::vector<uint64_t> results;
        
        for (const auto& [hash, id] : index_) {
            results.push_back(id);
        }
        
        return results;
    }
    
    bool save(const std::string& filepath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream out(filepath, std::ios::binary);
        if (!out.is_open()) {
            LOG_ERROR("无法打开文件进行写入: " + filepath);
            return false;
        }
        
        uint64_t entryCount = index_.size();
        out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
        
        for (const auto& [hash, id] : index_) {
            out.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        }
        
        out.close();
        LOG_INFO("标题索引已保存: " + std::to_string(entryCount) + " 条记录");
        return true;
    }
    
    bool load(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()) {
            LOG_ERROR("无法打开文件进行读取: " + filepath);
            return false;
        }
        
        index_.clear();
        
        uint64_t entryCount = 0;
        in.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
        
        for (uint64_t i = 0; i < entryCount; ++i) {
            size_t hash = 0;
            uint64_t id = 0;
            in.read(reinterpret_cast<char*>(&hash), sizeof(hash));
            in.read(reinterpret_cast<char*>(&id), sizeof(id));
            index_[hash] = id;
        }
        
        totalEntries_ = entryCount;
        in.close();
        LOG_INFO("标题索引已加载: " + std::to_string(entryCount) + " 条记录");
        return true;
    }
    
    size_t getEntryCount() const { return totalEntries_; }
    
    size_t getMemoryUsage() const {
        return sizeof(TitleIndex) + index_.size() * (sizeof(size_t) + sizeof(uint64_t));
    }

private:
    std::unordered_map<size_t, uint64_t> index_;
    size_t totalEntries_;
    mutable std::mutex mutex_;
};

}
