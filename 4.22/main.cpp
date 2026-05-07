#include "src/core/config.h"
#include "src/core/parser.h"
#include "src/core/serializer.h"
#include "src/index/index_manager.h"
#include "src/search/searcher.h"
#include "src/utils/logger.h"
#include "src/utils/string_utils.h"

#include <iostream>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <windows.h>

using namespace litmgr;

void setupConsole() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
}

void printBanner() {
    std::cout << R"(
================================================
        科学文献管理系统
        版本 1.0.0
================================================
)" << std::endl;
}

void printMainMenu() {
    std::cout << R"(
==================== 主菜单 ====================
  [1] 按作者搜索 (精准匹配)
  [2] 按作者搜索 (部分匹配)
  [3] 按标题搜索 (精准匹配)
  [4] 按标题关键词搜索
  [5] 按关键词搜索 (AND逻辑)
  [6] 按关键词搜索 (OR逻辑)
  [7] 查看索引统计
  [8] 清除搜索缓存
  [0] 退出系统
================================================
请输入选项: )" << std::flush;
}

void printSingleResult(const SearchResult& result, size_t index, size_t total) {
    std::cout << "\n[" << index << "/" << total << "]\n";
    std::cout << "ID: " << result.id << "\n";
    std::cout << "标题: " << result.title << "\n";
    
    std::cout << "作者: ";
    for (size_t i = 0; i < result.authors.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << result.authors[i];
    }
    std::cout << "\n";
    
    if (!result.venue.empty()) {
        std::cout << "期刊/会议: " << result.venue << "\n";
    }
    if (result.year > 0) {
        std::cout << "年份: " << result.year << "\n";
    }
    if (!result.volume.empty()) {
        std::cout << "卷号: " << result.volume << "\n";
    }
    if (!result.link.empty()) {
        std::cout << "链接: " << result.link << "\n";
    }
    std::cout << std::string(60, '-') << "\n";
}

void printResultsPaginated(const SearchResponse& response, size_t pageSize = 10) {
    if (response.results.empty()) {
        std::cout << "\n未找到匹配的结果。\n";
        return;
    }
    
    std::cout << "\n找到 " << response.total << " 条结果";
    std::cout << "，耗时 " << std::fixed << std::setprecision(3) << response.searchTime << "秒";
    if (response.fromCache) {
        std::cout << " (来自缓存)";
    }
    std::cout << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    size_t totalResults = response.results.size();
    size_t currentIndex = 0;
    
    while (currentIndex < totalResults) {
        size_t endIndex = (std::min)(currentIndex + pageSize, totalResults);
        
        for (size_t i = currentIndex; i < endIndex; ++i) {
            printSingleResult(response.results[i], i + 1, totalResults);
        }
        
        currentIndex = endIndex;
        
        if (currentIndex < totalResults) {
            std::cout << "\n--- 已显示 " << currentIndex << "/" << totalResults << " 条结果 ---\n";
            std::cout << "按回车键继续，输入 q 返回菜单: ";
            
            std::string input;
            std::getline(std::cin, input);
            
            if (!input.empty() && (input[0] == 'q' || input[0] == 'Q')) {
                std::cout << "已中断显示，返回主菜单。\n";
                return;
            }
        }
    }
    
    std::cout << "\n--- 已显示全部 " << totalResults << " 条结果 ---\n";
}

std::string getInput(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void doSearch(Searcher* searcher, SearchType type, const SearchOptions& options) {
    std::string prompt;
    switch (type) {
        case SearchType::AUTHOR:
            prompt = "请输入作者名: ";
            break;
        case SearchType::TITLE:
            prompt = "请输入标题: ";
            break;
        case SearchType::KEYWORDS:
            prompt = "请输入关键词 (用空格分隔): ";
            break;
    }
    
    std::string query = getInput(prompt);
    if (query.empty()) {
        std::cout << "输入不能为空！\n";
        return;
    }
    
    SearchOptions searchOpts = options;
    searchOpts.limit = 1000;
    
    auto response = searcher->search(type, query, searchOpts);
    printResultsPaginated(response, 10);
    
    std::cout << "\n按回车键返回菜单...";
    std::cin.get();
}

void runInteractive(std::unique_ptr<IndexManager>& indexManager, std::unique_ptr<Searcher>& searcher) {
    while (true) {
        printMainMenu();
        
        std::string choice;
        std::getline(std::cin, choice);
        
        if (choice.empty()) continue;
        
        char cmd = choice[0];
        
        switch (cmd) {
            case '1':
                doSearch(searcher.get(), SearchType::AUTHOR, SearchOptions{MatchType::EXACT, LogicType::AND, 100, 0});
                break;
                
            case '2':
                doSearch(searcher.get(), SearchType::AUTHOR, SearchOptions{MatchType::PARTIAL, LogicType::AND, 100, 0});
                break;
                
            case '3':
                doSearch(searcher.get(), SearchType::TITLE, SearchOptions{MatchType::EXACT, LogicType::AND, 100, 0});
                break;
                
            case '4':
                doSearch(searcher.get(), SearchType::TITLE, SearchOptions{MatchType::PARTIAL, LogicType::AND, 100, 0});
                break;
                
            case '5':
                doSearch(searcher.get(), SearchType::KEYWORDS, SearchOptions{MatchType::EXACT, LogicType::AND, 100, 0});
                break;
                
            case '6':
                doSearch(searcher.get(), SearchType::KEYWORDS, SearchOptions{MatchType::EXACT, LogicType::OR, 100, 0});
                break;
                
            case '7':
                std::cout << "\n";
                indexManager->printStats();
                std::cout << "按回车键继续...";
                std::cin.get();
                break;
                
            case '8':
                searcher->clearCache();
                std::cout << "缓存已清除！\n";
                std::cout << "按回车键继续...";
                std::cin.get();
                break;
                
            case '0':
            case 'q':
            case 'Q':
                std::cout << "\n感谢使用，再见！\n";
                return;
                
            default:
                std::cout << "无效选项，请重新输入！\n";
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    setupConsole();
    printBanner();
    
    Logger::getInstance().setLevel(LogLevel::INFO);
    
    // 优先从环境变量读取 XML 路径
    std::string xmlPath = "dblp.xml";
    const char* homeEnv = std::getenv("LITMGMT_HOME");
    if (homeEnv != nullptr && homeEnv[0] != '\0') {
        std::string home = homeEnv;
        while (!home.empty() && (home.back() == '/' || home.back() == '\\')) {
            home.pop_back();
        }
        xmlPath = home + "/dblp.xml";
    }
    
    // 命令行参数可以覆盖
    if (argc > 1) {
        xmlPath = argv[1];
    }
    
    auto indexManager = std::make_unique<IndexManager>();
    
    if (indexManager->indexesExist()) {
        std::cout << "正在加载现有索引...\n";
        if (!indexManager->loadIndexes()) {
            std::cerr << "加载索引失败。请重新构建。\n";
            std::cout << "按回车键退出...";
            std::cin.get();
            return 1;
        }
    } else {
        std::cout << "未找到索引文件。正在从XML文件构建索引...\n";
        std::cout << "XML文件: " << xmlPath << "\n";
        
        if (!indexManager->buildFromXML(xmlPath)) {
            std::cerr << "构建索引失败。\n";
            std::cout << "按回车键退出...";
            std::cin.get();
            return 1;
        }
    }
    
    auto searcher = std::make_unique<Searcher>(indexManager.get());
    
    runInteractive(indexManager, searcher);
    
    return 0;
}
