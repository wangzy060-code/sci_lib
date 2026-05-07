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
#include <algorithm>

namespace litmgr {

class KeywordIndex {
public:
    KeywordIndex() : totalKeywords_(0), totalPostings_(0) {}
    
    void addKeywords(const std::string& title, uint64_t litId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto keywords = StringUtils::extractKeywords(title);
        for (const auto& keyword : keywords) {
            auto& postings = index_[keyword];
            if (postings.empty() || postings.back() != litId) {
                postings.push_back(litId);
                totalPostings_++;
            }
        }
        totalKeywords_ = index_.size();
    }
    
    std::vector<uint64_t> searchSingle(const std::string& keyword) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string normalizedKeyword = StringUtils::toLower(StringUtils::trim(keyword));
        auto it = index_.find(normalizedKeyword);
        
        if (it != index_.end()) {
            return it->second;
        }
        return {};
    }
    
    std::vector<uint64_t> searchAnd(const std::vector<std::string>& keywords) const {
        if (keywords.empty()) return {};
        if (keywords.size() == 1) return searchSingle(keywords[0]);
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::vector<uint64_t>> postingLists;
        for (const auto& keyword : keywords) {
            std::string normalizedKeyword = StringUtils::toLower(StringUtils::trim(keyword));
            auto it = index_.find(normalizedKeyword);
            if (it == index_.end()) {
                return {};
            }
            postingLists.push_back(it->second);
        }
        
        std::sort(postingLists.begin(), postingLists.end(),
            [](const auto& a, const auto& b) { return a.size() < b.size(); });
        
        std::vector<uint64_t> result = postingLists[0];
        
        for (size_t i = 1; i < postingLists.size() && !result.empty(); ++i) {
            result = intersect(result, postingLists[i]);
        }
        
        return result;
    }
    
    std::vector<uint64_t> searchOr(const std::vector<std::string>& keywords) const {
        if (keywords.empty()) return {};
        if (keywords.size() == 1) return searchSingle(keywords[0]);
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint64_t> result;
        std::unordered_map<uint64_t, bool> seen;
        
        for (const auto& keyword : keywords) {
            std::string normalizedKeyword = StringUtils::toLower(StringUtils::trim(keyword));
            auto it = index_.find(normalizedKeyword);
            
            if (it != index_.end()) {
                for (uint64_t id : it->second) {
                    if (!seen[id]) {
                        seen[id] = true;
                        result.push_back(id);
                    }
                }
            }
        }
        
        return result;
    }
    
    bool save(const std::string& filepath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream out(filepath, std::ios::binary);
        if (!out.is_open()) {
            LOG_ERROR("无法打开文件进行写入: " + filepath);
            return false;
        }
        
        uint64_t keywordCount = index_.size();
        out.write(reinterpret_cast<const char*>(&keywordCount), sizeof(keywordCount));
        
        for (const auto& [keyword, ids] : index_) {
            uint32_t keywordLen = static_cast<uint32_t>(keyword.size());
            out.write(reinterpret_cast<const char*>(&keywordLen), sizeof(keywordLen));
            out.write(keyword.data(), keywordLen);
            
            uint32_t idCount = static_cast<uint32_t>(ids.size());
            out.write(reinterpret_cast<const char*>(&idCount), sizeof(idCount));
            out.write(reinterpret_cast<const char*>(ids.data()), idCount * sizeof(uint64_t));
        }
        
        out.close();
        LOG_INFO("关键词索引已保存: " + std::to_string(keywordCount) + " 个关键词");
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
        
        uint64_t keywordCount = 0;
        in.read(reinterpret_cast<char*>(&keywordCount), sizeof(keywordCount));
        
        for (uint64_t i = 0; i < keywordCount; ++i) {
            uint32_t keywordLen = 0;
            in.read(reinterpret_cast<char*>(&keywordLen), sizeof(keywordLen));
            
            std::string keyword(keywordLen, '\0');
            in.read(&keyword[0], keywordLen);
            
            uint32_t idCount = 0;
            in.read(reinterpret_cast<char*>(&idCount), sizeof(idCount));
            
            std::vector<uint64_t> ids(idCount);
            in.read(reinterpret_cast<char*>(ids.data()), idCount * sizeof(uint64_t));
            
            index_[keyword] = std::move(ids);
            totalPostings_ += idCount;
        }
        
        totalKeywords_ = index_.size();
        in.close();
        LOG_INFO("关键词索引已加载: " + std::to_string(keywordCount) + " 个关键词");
        return true;
    }
    
    size_t getKeywordCount() const { return totalKeywords_; }
    size_t getTotalPostings() const { return totalPostings_; }
    
    size_t getMemoryUsage() const {
        size_t usage = sizeof(KeywordIndex);
        for (const auto& [keyword, ids] : index_) {
            usage += keyword.size() * sizeof(char);
            usage += ids.size() * sizeof(uint64_t);
        }
        return usage;
    }

private:
    std::vector<uint64_t> intersect(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) const {
        std::vector<uint64_t> result;
        size_t i = 0, j = 0;
        
        while (i < a.size() && j < b.size()) {
            if (a[i] < b[j]) {
                ++i;
            } else if (a[i] > b[j]) {
                ++j;
            } else {
                result.push_back(a[i]);
                ++i;
                ++j;
            }
        }
        
        return result;
    }
    
    std::unordered_map<std::string, std::vector<uint64_t>> index_;
    size_t totalKeywords_;
    size_t totalPostings_;
    mutable std::mutex mutex_;
};

}
