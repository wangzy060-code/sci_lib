#pragma once

#include "../core/literature.h"
#include "../index/index_manager.h"
#include "../cache/lru_cache.h"
#include "../utils/logger.h"
#include "../utils/string_utils.h"
#include "../core/config.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace litmgr {

enum class SearchType {
    AUTHOR,
    TITLE,
    KEYWORDS
};

enum class MatchType {
    EXACT,
    PARTIAL
};

enum class LogicType {
    AND,
    OR
};

struct SearchOptions {
    MatchType matchType = MatchType::EXACT;
    LogicType logicType = LogicType::AND;
    size_t limit = 100;
    size_t offset = 0;
};

struct SearchResult {
    uint64_t id;
    std::string title;
    std::vector<std::string> authors;
    std::string venue;
    uint16_t year;
    std::string volume;
    std::string link;
    
    static SearchResult fromLiterature(const Literature& lit) {
        SearchResult result;
        result.id = lit.id;
        result.title = lit.title;
        result.authors = lit.authors;
        result.venue = lit.venue;
        result.year = lit.year;
        result.volume = lit.volume;
        result.link = lit.ee;
        return result;
    }
};

struct SearchResponse {
    size_t total;
    std::vector<SearchResult> results;
    double searchTime;
    bool fromCache;
    
    std::string toJson() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"total\": " << total << ",\n";
        oss << "  \"search_time\": \"" << std::fixed << std::setprecision(3) << searchTime << "s\",\n";
        oss << "  \"from_cache\": " << (fromCache ? "true" : "false") << ",\n";
        oss << "  \"results\": [\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            oss << "    {\n";
            oss << "      \"id\": " << r.id << ",\n";
            oss << "      \"title\": \"" << escapeJson(r.title) << "\",\n";
            oss << "      \"authors\": [";
            for (size_t j = 0; j < r.authors.size(); ++j) {
                if (j > 0) oss << ", ";
                oss << "\"" << escapeJson(r.authors[j]) << "\"";
            }
            oss << "],\n";
            oss << "      \"venue\": \"" << escapeJson(r.venue) << "\",\n";
            oss << "      \"year\": " << r.year << ",\n";
            oss << "      \"volume\": \"" << escapeJson(r.volume) << "\",\n";
            oss << "      \"link\": \"" << escapeJson(r.link) << "\"\n";
            oss << "    }";
            if (i < results.size() - 1) oss << ",";
            oss << "\n";
        }
        
        oss << "  ]\n";
        oss << "}";
        return oss.str();
    }

private:
    static std::string escapeJson(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

class Searcher {
public:
    explicit Searcher(IndexManager* indexManager)
        : indexManager_(indexManager)
        , authorCache_(std::make_unique<LRUCache<std::string, std::vector<uint64_t>>>(Config::CACHE_SIZE))
        , keywordCache_(std::make_unique<LRUCache<std::string, std::vector<uint64_t>>>(Config::CACHE_SIZE))
    {}
    
    SearchResponse search(SearchType type, const std::string& query, const SearchOptions& options = SearchOptions()) {
        auto startTime = std::chrono::steady_clock::now();
        SearchResponse response;
        response.fromCache = false;
        
        std::vector<uint64_t> ids;
        
        switch (type) {
            case SearchType::AUTHOR:
                ids = searchAuthorImpl(query, options, response.fromCache);
                break;
            case SearchType::TITLE:
                ids = searchTitleImpl(query, options);
                break;
            case SearchType::KEYWORDS:
                ids = searchKeywordsImpl(query, options, response.fromCache);
                break;
        }
        
        response.total = ids.size();
        
        size_t start = (std::min)(options.offset, ids.size());
        size_t end = (std::min)(options.offset + options.limit, ids.size());
        
        uint64_t totalRecords = indexManager_->getDataFile()->getTotalRecords();
        
        for (size_t i = start; i < end; ++i) {
            Literature lit;
            uint64_t id = ids[i];
            if (id > 0 && id <= totalRecords) {
                if (indexManager_->getDataFile()->readAt(id - 1, lit)) {
                    response.results.push_back(SearchResult::fromLiterature(lit));
                }
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        response.searchTime = duration.count() / 1000000.0;
        
        return response;
    }
    
    SearchResponse searchAuthor(const std::string& author, const SearchOptions& options = SearchOptions()) {
        return search(SearchType::AUTHOR, author, options);
    }
    
    SearchResponse searchTitle(const std::string& title, const SearchOptions& options = SearchOptions()) {
        return search(SearchType::TITLE, title, options);
    }
    
    SearchResponse searchKeywords(const std::string& keywords, const SearchOptions& options = SearchOptions()) {
        return search(SearchType::KEYWORDS, keywords, options);
    }
    
    void clearCache() {
        authorCache_->clear();
        keywordCache_->clear();
    }
    
    size_t getCacheSize() const {
        return authorCache_->size() + keywordCache_->size();
    }

private:
    std::vector<uint64_t> searchAuthorImpl(const std::string& query, const SearchOptions& options, bool& fromCache) {
        std::string cacheKey = "author:" + query;
        std::vector<uint64_t> ids;
        
        if (authorCache_->get(cacheKey, ids)) {
            fromCache = true;
            return ids;
        }
        
        if (options.matchType == MatchType::EXACT) {
            ids = indexManager_->getAuthorIndex()->search(query);
        } else {
            auto matchingAuthors = indexManager_->getAuthorIndex()->searchPartial(query);
            for (const auto& author : matchingAuthors) {
                auto authorIds = indexManager_->getAuthorIndex()->search(author);
                ids.insert(ids.end(), authorIds.begin(), authorIds.end());
            }
        }
        
        authorCache_->put(cacheKey, ids);
        fromCache = false;
        return ids;
    }
    
    std::vector<uint64_t> searchTitleImpl(const std::string& query, const SearchOptions& options) {
        std::vector<uint64_t> ids;
        
        if (options.matchType == MatchType::EXACT) {
            uint64_t id;
            if (indexManager_->getTitleIndex()->search(query, id)) {
                ids.push_back(id);
            }
        } else {
            ids = indexManager_->getTitleIndex()->searchPartial(query);
        }
        
        return ids;
    }
    
    std::vector<uint64_t> searchKeywordsImpl(const std::string& query, const SearchOptions& options, bool& fromCache) {
        auto keywords = StringUtils::split(query, ' ');
        
        if (keywords.empty()) return {};
        
        std::string cacheKey = "keywords:" + query + ":" + 
                               (options.logicType == LogicType::AND ? "AND" : "OR");
        std::vector<uint64_t> ids;
        
        if (keywordCache_->get(cacheKey, ids)) {
            fromCache = true;
            return ids;
        }
        
        if (options.logicType == LogicType::AND) {
            ids = indexManager_->getKeywordIndex()->searchAnd(keywords);
        } else {
            ids = indexManager_->getKeywordIndex()->searchOr(keywords);
        }
        
        keywordCache_->put(cacheKey, ids);
        fromCache = false;
        return ids;
    }
    
    IndexManager* indexManager_;
    std::unique_ptr<LRUCache<std::string, std::vector<uint64_t>>> authorCache_;
    std::unique_ptr<LRUCache<std::string, std::vector<uint64_t>>> keywordCache_;
};

}
