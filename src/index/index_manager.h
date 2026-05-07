#pragma once

#include "author_index.h"
#include "title_index.h"
#include "keyword_index.h"
#include "../core/literature.h"
#include "../core/parser.h"
#include "../core/serializer.h"
#include "../core/config.h"
#include "../utils/logger.h"
#include "../utils/file_utils.h"
#include <memory>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace litmgr {

struct IndexStats {
    size_t totalLiterature;
    size_t authorCount;
    size_t titleCount;
    size_t keywordCount;
    size_t totalPostings;
    size_t memoryUsage;
    double buildTime;
};

class IndexManager {
public:
    IndexManager() 
        : authorIndex_(std::make_unique<AuthorIndex>())
        , titleIndex_(std::make_unique<TitleIndex>())
        , keywordIndex_(std::make_unique<KeywordIndex>())
        , dataFile_(std::make_unique<LiteratureDataFile>())
    {}
    
    bool buildFromXML(const std::string& xmlPath) {
        LOG_INFO("正在从以下文件构建索引: " + xmlPath);
        
        auto startTime = std::chrono::steady_clock::now();
        
        if (!dataFile_->openForWrite(Config::getParsedDataFile())) {
            LOG_ERROR("无法打开数据文件进行写入");
            return false;
        }
        
        auto callback = [this](const Literature& lit, uint64_t count) {
            dataFile_->append(lit);
            
            for (const auto& author : lit.authors) {
                authorIndex_->addAuthor(author, lit.id);
            }
            
            if (!lit.title.empty()) {
                titleIndex_->addTitle(lit.title, lit.id);
                keywordIndex_->addKeywords(lit.title, lit.id);
            }
            
            if (count % 100000 == 0) {
                LOG_INFO("已处理 " + std::to_string(count) + " 条记录...");
            }
        };
        
        auto progressCallback = [](uint64_t count, double percent) {
            std::cout << "\r进度: " << std::fixed << std::setprecision(1) 
                      << percent << "% (" << count << " 条记录)" << std::flush;
        };
        
        XMLParser parser;
        if (!parser.parse(xmlPath, callback, progressCallback)) {
            LOG_ERROR("解析XML文件失败");
            return false;
        }
        
        std::cout << std::endl;
        
        dataFile_->closeWrite();
        
        if (!saveIndexes()) {
            LOG_ERROR("保存索引失败");
            return false;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        
        stats_.totalLiterature = parser.getTotalParsed();
        stats_.authorCount = authorIndex_->getAuthorCount();
        stats_.titleCount = titleIndex_->getEntryCount();
        stats_.keywordCount = keywordIndex_->getKeywordCount();
        stats_.totalPostings = keywordIndex_->getTotalPostings();
        stats_.memoryUsage = getMemoryUsage();
        stats_.buildTime = static_cast<double>(duration.count());
        
        LOG_INFO("索引构建完成，耗时 " + std::to_string(duration.count()) + " 秒");
        printStats();
        
        return true;
    }
    
    bool loadIndexes() {
        LOG_INFO("正在加载索引...");
        
        if (!FileUtils::fileExists(Config::getParsedDataFile())) {
            LOG_WARNING("数据文件不存在");
            return false;
        }
        
        if (!dataFile_->openForRead(Config::getParsedDataFile())) {
            LOG_ERROR("无法打开数据文件进行读取");
            return false;
        }
        
        bool success = true;
        
        if (FileUtils::fileExists(Config::getAuthorIndexFile())) {
            if (!authorIndex_->load(Config::getAuthorIndexFile())) {
                LOG_ERROR("加载作者索引失败");
                success = false;
            }
        }
        
        if (FileUtils::fileExists(Config::getTitleIndexFile())) {
            if (!titleIndex_->load(Config::getTitleIndexFile())) {
                LOG_ERROR("加载标题索引失败");
                success = false;
            }
        }
        
        if (FileUtils::fileExists(Config::getKeywordIndexFile())) {
            if (!keywordIndex_->load(Config::getKeywordIndexFile())) {
                LOG_ERROR("加载关键词索引失败");
                success = false;
            }
        }
        
        if (success) {
            stats_.totalLiterature = dataFile_->getTotalRecords();
            stats_.authorCount = authorIndex_->getAuthorCount();
            stats_.titleCount = titleIndex_->getEntryCount();
            stats_.keywordCount = keywordIndex_->getKeywordCount();
            stats_.totalPostings = keywordIndex_->getTotalPostings();
            stats_.memoryUsage = getMemoryUsage();
            
            LOG_INFO("索引加载成功");
            printStats();
        }
        
        return success;
    }
    
    bool saveIndexes() {
        LOG_INFO("正在保存索引...");
        
        FileUtils::createDirectory(Config::getDataDir());
        
        bool success = true;
        
        if (!authorIndex_->save(Config::getAuthorIndexFile())) {
            LOG_ERROR("保存作者索引失败");
            success = false;
        }
        
        if (!titleIndex_->save(Config::getTitleIndexFile())) {
            LOG_ERROR("保存标题索引失败");
            success = false;
        }
        
        if (!keywordIndex_->save(Config::getKeywordIndexFile())) {
            LOG_ERROR("保存关键词索引失败");
            success = false;
        }
        
        if (success) {
            LOG_INFO("索引保存成功");
        }
        
        return success;
    }
    
    bool indexesExist() const {
        return FileUtils::fileExists(Config::getParsedDataFile()) &&
               FileUtils::fileExists(Config::getAuthorIndexFile()) &&
               FileUtils::fileExists(Config::getTitleIndexFile()) &&
               FileUtils::fileExists(Config::getKeywordIndexFile());
    }
    
    AuthorIndex* getAuthorIndex() { return authorIndex_.get(); }
    TitleIndex* getTitleIndex() { return titleIndex_.get(); }
    KeywordIndex* getKeywordIndex() { return keywordIndex_.get(); }
    LiteratureDataFile* getDataFile() { return dataFile_.get(); }
    
    const IndexStats& getStats() const { return stats_; }
    
    size_t getMemoryUsage() const {
        return authorIndex_->getMemoryUsage() +
               titleIndex_->getMemoryUsage() +
               keywordIndex_->getMemoryUsage();
    }
    
    void printStats() const {
        std::cout << "\n=== 索引统计信息 ===\n";
        std::cout << "文献总数: " << stats_.totalLiterature << "\n";
        std::cout << "作者数量: " << stats_.authorCount << "\n";
        std::cout << "标题数量: " << stats_.titleCount << "\n";
        std::cout << "关键词数量: " << stats_.keywordCount << "\n";
        std::cout << "倒排记录数: " << stats_.totalPostings << "\n";
        std::cout << "内存占用: " << (stats_.memoryUsage / (1024 * 1024)) << " MB\n";
        std::cout << "====================\n\n";
    }

private:
    std::unique_ptr<AuthorIndex> authorIndex_;
    std::unique_ptr<TitleIndex> titleIndex_;
    std::unique_ptr<KeywordIndex> keywordIndex_;
    std::unique_ptr<LiteratureDataFile> dataFile_;
    IndexStats stats_;
};

}
