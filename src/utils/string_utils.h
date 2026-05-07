#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <locale>
#include <sstream>

namespace litmgr {

class StringUtils {
public:
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }
    
    static std::string trim(const std::string& str) {
        size_t start = 0;
        size_t end = str.length();
        
        while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
            ++start;
        }
        
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
            --end;
        }
        
        return str.substr(start, end - start);
    }
    
    static std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::istringstream iss(str);
        std::string token;
        
        while (std::getline(iss, token, delimiter)) {
            if (!token.empty()) {
                tokens.push_back(trim(token));
            }
        }
        
        return tokens;
    }
    
    static std::vector<std::string> extractKeywords(const std::string& title) {
        std::vector<std::string> keywords;
        std::string word;
        std::string lowerTitle = toLower(title);
        
        for (char c : lowerTitle) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-') {
                word += c;
            } else if (!word.empty()) {
                if (word.length() >= 2 && !isStopWord(word)) {
                    keywords.push_back(word);
                }
                word.clear();
            }
        }
        
        if (!word.empty() && word.length() >= 2 && !isStopWord(word)) {
            keywords.push_back(word);
        }
        
        return keywords;
    }
    
    static size_t hashString(const std::string& str) {
        std::hash<std::string> hasher;
        return hasher(toLower(trim(str)));
    }
    
    static bool isStopWord(const std::string& word) {
        static const std::vector<std::string> stopWords = {
            "a", "an", "the", "and", "or", "but", "in", "on", "at", "to", "for",
            "of", "with", "by", "from", "as", "is", "was", "are", "were", "been",
            "be", "have", "has", "had", "do", "does", "did", "will", "would",
            "could", "should", "may", "might", "must", "shall", "can", "need",
            "this", "that", "these", "those", "it", "its", "they", "them",
            "their", "we", "our", "you", "your", "he", "she", "him", "her",
            "his", "i", "me", "my", "not", "no", "yes", "all", "each", "every",
            "both", "few", "more", "most", "other", "some", "such", "only",
            "own", "same", "so", "than", "too", "very", "just", "also", "now",
            "here", "there", "when", "where", "why", "how", "what", "which", "who"
        };
        
        return std::find(stopWords.begin(), stopWords.end(), word) != stopWords.end();
    }
    
    static std::string normalizeAuthorName(const std::string& name) {
        std::string normalized = trim(name);
        std::replace(normalized.begin(), normalized.end(), '\t', ' ');
        
        while (normalized.find("  ") != std::string::npos) {
            size_t pos = normalized.find("  ");
            normalized.replace(pos, 2, " ");
        }
        
        return normalized;
    }
};

}
