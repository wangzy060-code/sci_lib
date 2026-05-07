#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

namespace litmgr {

struct Literature {
    uint64_t id;
    std::string key;
    std::string type;
    std::vector<std::string> authors;
    std::string title;
    std::string venue;
    uint16_t year;
    std::string volume;
    std::string pages;
    std::string ee;
    std::string url;
    
    Literature() : id(0), year(0) {}
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "ID: " << id << "\n";
        oss << "Type: " << type << "\n";
        oss << "Title: " << title << "\n";
        oss << "Authors: ";
        for (size_t i = 0; i < authors.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << authors[i];
        }
        oss << "\n";
        if (!venue.empty()) oss << "Venue: " << venue << "\n";
        if (year > 0) oss << "Year: " << year << "\n";
        if (!volume.empty()) oss << "Volume: " << volume << "\n";
        if (!pages.empty()) oss << "Pages: " << pages << "\n";
        if (!ee.empty()) oss << "Link: " << ee << "\n";
        return oss.str();
    }
    
    std::string toJson() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"id\": " << id << ",\n";
        oss << "  \"title\": \"" << escapeJson(title) << "\",\n";
        oss << "  \"authors\": [";
        for (size_t i = 0; i < authors.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << escapeJson(authors[i]) << "\"";
        }
        oss << "],\n";
        oss << "  \"venue\": \"" << escapeJson(venue) << "\",\n";
        oss << "  \"year\": " << year << ",\n";
        oss << "  \"volume\": \"" << escapeJson(volume) << "\",\n";
        oss << "  \"link\": \"" << escapeJson(ee) << "\"\n";
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

}
