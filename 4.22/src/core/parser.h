#pragma once

#include "literature.h"
#include "../utils/logger.h"
#include "../utils/string_utils.h"
#include "../core/config.h"
#include <fstream>
#include <string>
#include <functional>
#include <vector>
#include <chrono>
#include <map>

namespace litmgr {

enum class ParseState {
    NONE,
    IN_ELEMENT,
    IN_AUTHOR,
    IN_TITLE,
    IN_YEAR,
    IN_JOURNAL,
    IN_BOOKTITLE,
    IN_VOLUME,
    IN_PAGES,
    IN_EE,
    IN_URL,
    IN_SCHOOL,
    IN_PUBLISHER
};

class XMLParser {
public:
    using Callback = std::function<void(const Literature&, uint64_t)>;
    using ProgressCallback = std::function<void(uint64_t, double)>;
    
    XMLParser() : currentId_(0), totalParsed_(0), bytesRead_(0), fileSize_(0) {}
    
    bool parse(const std::string& filepath, Callback callback, ProgressCallback progress = nullptr) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("无法打开文件: " + filepath);
            return false;
        }
        
        file.seekg(0, std::ios::end);
        fileSize_ = file.tellg();
        file.seekg(0, std::ios::beg);
        
        LOG_INFO("开始解析文件: " + filepath);
        LOG_INFO("文件大小: " + std::to_string(fileSize_ / (1024 * 1024)) + " MB");
        
        auto startTime = std::chrono::steady_clock::now();
        
        std::string line;
        std::string buffer;
        buffer.reserve(65536);
        
        uint64_t lastProgressBytes = 0;
        
        while (std::getline(file, line)) {
            bytesRead_ += line.size() + 1;
            buffer += line + '\n';
            
            if (buffer.size() >= 32768) {
                processBuffer(buffer, callback);
                buffer.clear();
                buffer.reserve(65536);
                
                if (progress && bytesRead_ - lastProgressBytes >= 10 * 1024 * 1024) {
                    double percent = static_cast<double>(bytesRead_) / fileSize_ * 100.0;
                    progress(totalParsed_, percent);
                    lastProgressBytes = bytesRead_;
                }
            }
        }
        
        if (!buffer.empty()) {
            processBuffer(buffer, callback);
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        
        LOG_INFO("解析完成。总记录数: " + std::to_string(totalParsed_));
        LOG_INFO("耗时: " + std::to_string(duration.count()) + " 秒");
        
        file.close();
        return true;
    }
    
    uint64_t getTotalParsed() const { return totalParsed_; }

private:
    void processBuffer(const std::string& buffer, Callback callback) {
        size_t pos = 0;
        
        while (pos < buffer.size()) {
            if (buffer[pos] == '<') {
                size_t endTag = buffer.find('>', pos);
                if (endTag == std::string::npos) break;
                
                std::string tag = buffer.substr(pos + 1, endTag - pos - 1);
                processTag(tag, buffer, pos, endTag, callback);
                pos = endTag + 1;
            } else {
                size_t contentStart = pos;
                size_t nextTag = buffer.find('<', pos);
                
                if (nextTag == std::string::npos) {
                    if (currentState_ != ParseState::NONE) {
                        std::string content = buffer.substr(contentStart);
                        processContent(content);
                    }
                    break;
                }
                
                if (nextTag > contentStart && currentState_ != ParseState::NONE) {
                    std::string content = buffer.substr(contentStart, nextTag - contentStart);
                    processContent(content);
                }
                
                pos = nextTag;
            }
        }
    }
    
    void processTag(const std::string& tag, const std::string& buffer, size_t& pos, size_t endTag, Callback callback) {
        bool isClosing = !tag.empty() && tag[0] == '/';
        bool isSelfClosing = !tag.empty() && tag.back() == '/';
        
        std::string tagName = tag;
        if (isClosing) {
            tagName = tag.substr(1);
        }
        if (isSelfClosing && !isClosing) {
            tagName = tag.substr(0, tag.size() - 1);
        }
        
        size_t spacePos = tagName.find(' ');
        if (spacePos != std::string::npos) {
            tagName = tagName.substr(0, spacePos);
        }
        
        if (isClosing) {
            handleClosingTag(tagName, callback);
        } else if (!isSelfClosing) {
            handleOpeningTag(tagName, tag);
        }
    }
    
    void handleOpeningTag(const std::string& tagName, const std::string& fullTag) {
        if (isPublicationElement(tagName)) {
            currentLit_ = Literature();
            currentLit_.id = ++currentId_;
            currentLit_.type = tagName;
            currentLit_.key = extractAttribute(fullTag, "key");
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_ELEMENT) {
            if (tagName == "author" || tagName == "editor") {
                currentState_ = ParseState::IN_AUTHOR;
            } else if (tagName == "title") {
                currentState_ = ParseState::IN_TITLE;
            } else if (tagName == "year") {
                currentState_ = ParseState::IN_YEAR;
            } else if (tagName == "journal") {
                currentState_ = ParseState::IN_JOURNAL;
            } else if (tagName == "booktitle") {
                currentState_ = ParseState::IN_BOOKTITLE;
            } else if (tagName == "volume") {
                currentState_ = ParseState::IN_VOLUME;
            } else if (tagName == "pages") {
                currentState_ = ParseState::IN_PAGES;
            } else if (tagName == "ee") {
                currentState_ = ParseState::IN_EE;
            } else if (tagName == "url") {
                currentState_ = ParseState::IN_URL;
            }
        }
    }
    
    void handleClosingTag(const std::string& tagName, Callback callback) {
        if (isPublicationElement(tagName)) {
            if (!currentLit_.title.empty()) {
                callback(currentLit_, totalParsed_);
                totalParsed_++;
            }
            currentState_ = ParseState::NONE;
        } else if (currentState_ == ParseState::IN_AUTHOR) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_TITLE) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_YEAR) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_JOURNAL || currentState_ == ParseState::IN_BOOKTITLE) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_VOLUME) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_PAGES) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_EE) {
            currentState_ = ParseState::IN_ELEMENT;
        } else if (currentState_ == ParseState::IN_URL) {
            currentState_ = ParseState::IN_ELEMENT;
        }
    }
    
    void processContent(const std::string& content) {
        std::string trimmed = StringUtils::trim(content);
        if (trimmed.empty()) return;
        
        trimmed = decodeHTMLEntities(trimmed);
        
        switch (currentState_) {
            case ParseState::IN_AUTHOR:
                currentLit_.authors.push_back(StringUtils::normalizeAuthorName(trimmed));
                break;
            case ParseState::IN_TITLE:
                currentLit_.title += trimmed;
                break;
            case ParseState::IN_YEAR:
                try {
                    currentLit_.year = static_cast<uint16_t>(std::stoi(trimmed));
                } catch (...) {}
                break;
            case ParseState::IN_JOURNAL:
                currentLit_.venue += trimmed;
                break;
            case ParseState::IN_BOOKTITLE:
                currentLit_.venue += trimmed;
                break;
            case ParseState::IN_VOLUME:
                currentLit_.volume += trimmed;
                break;
            case ParseState::IN_PAGES:
                currentLit_.pages += trimmed;
                break;
            case ParseState::IN_EE:
                currentLit_.ee += trimmed;
                break;
            case ParseState::IN_URL:
                currentLit_.url += trimmed;
                break;
            default:
                break;
        }
    }
    
    bool isPublicationElement(const std::string& tagName) {
        static const std::vector<std::string> pubTypes = {
            "article", "inproceedings", "proceedings", "book", "incollection",
            "phdthesis", "mastersthesis", "www", "person", "data"
        };
        
        for (const auto& type : pubTypes) {
            if (tagName == type) return true;
        }
        return false;
    }
    
    std::string extractAttribute(const std::string& tag, const std::string& attr) {
        std::string search = attr + "=\"";
        size_t pos = tag.find(search);
        if (pos == std::string::npos) return "";
        
        pos += search.size();
        size_t endPos = tag.find('"', pos);
        if (endPos == std::string::npos) return "";
        
        return tag.substr(pos, endPos - pos);
    }
    
    std::string decodeHTMLEntities(const std::string& str) {
        std::string result = str;
        
        static const std::map<std::string, std::string> entities = {
            {"&amp;", "&"},
            {"&lt;", "<"},
            {"&gt;", ">"},
            {"&quot;", "\""},
            {"&apos;", "'"},
            {"&auml;", "a"},
            {"&ouml;", "o"},
            {"&uuml;", "u"},
            {"&Auml;", "A"},
            {"&Ouml;", "O"},
            {"&Uuml;", "U"},
            {"&szlig;", "ss"},
            {"&nbsp;", " "}
        };
        
        for (const auto& [entity, replacement] : entities) {
            size_t pos = 0;
            while ((pos = result.find(entity, pos)) != std::string::npos) {
                result.replace(pos, entity.length(), replacement);
                pos += replacement.length();
            }
        }
        
        return result;
    }
    
    Literature currentLit_;
    ParseState currentState_ = ParseState::NONE;
    uint64_t currentId_ = 0;
    uint64_t totalParsed_ = 0;
    uint64_t bytesRead_ = 0;
    uint64_t fileSize_ = 0;
};

}
