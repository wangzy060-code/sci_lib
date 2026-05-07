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

class AuthorIndex {
public:
    AuthorIndex() : totalEntries_(0) {}
    
    void addAuthor(const std::string& author, uint64_t litId) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string normalizedAuthor = StringUtils::toLower(StringUtils::normalizeAuthorName(author));
        index_[normalizedAuthor].push_back(litId);
        totalEntries_++;
    }
    
    std::vector<uint64_t> search(const std::string& author) const {
        std::string normalizedAuthor = StringUtils::toLower(StringUtils::normalizeAuthorName(author));
        
        auto it = index_.find(normalizedAuthor);
        if (it != index_.end()) {
            return it->second;
        }
        return {};
    }
    
    std::vector<std::string> searchPartial(const std::string& partial) const {
        std::string normalizedPartial = StringUtils::toLower(StringUtils::trim(partial));
        std::vector<std::string> results;
        
        for (const auto& [author, ids] : index_) {
            if (author.find(normalizedPartial) != std::string::npos) {
                results.push_back(author);
            }
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
        
        uint64_t authorCount = index_.size();
        out.write(reinterpret_cast<const char*>(&authorCount), sizeof(authorCount));
        
        for (const auto& [author, ids] : index_) {
            uint32_t nameLen = static_cast<uint32_t>(author.size());
            out.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
            out.write(author.data(), nameLen);
            
            uint32_t idCount = static_cast<uint32_t>(ids.size());
            out.write(reinterpret_cast<const char*>(&idCount), sizeof(idCount));
            out.write(reinterpret_cast<const char*>(ids.data()), idCount * sizeof(uint64_t));
        }
        
        out.close();
        LOG_INFO("作者索引已保存: " + std::to_string(authorCount) + " 位作者");
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
        
        uint64_t authorCount = 0;
        in.read(reinterpret_cast<char*>(&authorCount), sizeof(authorCount));
        
        for (uint64_t i = 0; i < authorCount; ++i) {
            uint32_t nameLen = 0;
            in.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            
            std::string author(nameLen, '\0');
            in.read(&author[0], nameLen);
            
            uint32_t idCount = 0;
            in.read(reinterpret_cast<char*>(&idCount), sizeof(idCount));
            
            std::vector<uint64_t> ids(idCount);
            in.read(reinterpret_cast<char*>(ids.data()), idCount * sizeof(uint64_t));
            
            index_[author] = std::move(ids);
            totalEntries_ += idCount;
        }
        
        in.close();
        LOG_INFO("作者索引已加载: " + std::to_string(authorCount) + " 位作者");
        return true;
    }
    
    size_t getAuthorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.size();
    }
    
    size_t getTotalEntries() const { return totalEntries_; }
    
    size_t getMemoryUsage() const {
        size_t usage = sizeof(AuthorIndex);
        for (const auto& [author, ids] : index_) {
            usage += author.size() * sizeof(char);
            usage += ids.size() * sizeof(uint64_t);
        }
        return usage;
    }

private:
    std::unordered_map<std::string, std::vector<uint64_t>> index_;
    size_t totalEntries_;
    mutable std::mutex mutex_;
};

}
