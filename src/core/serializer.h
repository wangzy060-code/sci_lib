#pragma once

#include "literature.h"
#include <fstream>
#include <cstring>
#include <vector>

namespace litmgr {

class Serializer {
public:
    static bool serialize(std::ofstream& out, const Literature& lit) {
        writeUint64(out, lit.id);
        writeString(out, lit.key);
        writeString(out, lit.type);
        
        uint32_t authorCount = static_cast<uint32_t>(lit.authors.size());
        writeUint32(out, authorCount);
        for (const auto& author : lit.authors) {
            writeString(out, author);
        }
        
        writeString(out, lit.title);
        writeString(out, lit.venue);
        writeUint16(out, lit.year);
        writeString(out, lit.volume);
        writeString(out, lit.pages);
        writeString(out, lit.ee);
        writeString(out, lit.url);
        
        return out.good();
    }
    
    static bool deserialize(std::ifstream& in, Literature& lit) {
        if (!readUint64(in, lit.id)) return false;
        if (!readString(in, lit.key)) return false;
        if (!readString(in, lit.type)) return false;
        
        uint32_t authorCount = 0;
        if (!readUint32(in, authorCount)) return false;
        
        lit.authors.clear();
        lit.authors.reserve(authorCount);
        for (uint32_t i = 0; i < authorCount; ++i) {
            std::string author;
            if (!readString(in, author)) return false;
            lit.authors.push_back(std::move(author));
        }
        
        if (!readString(in, lit.title)) return false;
        if (!readString(in, lit.venue)) return false;
        if (!readUint16(in, lit.year)) return false;
        if (!readString(in, lit.volume)) return false;
        if (!readString(in, lit.pages)) return false;
        if (!readString(in, lit.ee)) return false;
        if (!readString(in, lit.url)) return false;
        
        return in.good() || in.eof();
    }

private:
    static void writeUint64(std::ofstream& out, uint64_t value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    
    static void writeUint32(std::ofstream& out, uint32_t value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    
    static void writeUint16(std::ofstream& out, uint16_t value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    
    static void writeString(std::ofstream& out, const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        writeUint32(out, len);
        if (len > 0) {
            out.write(str.data(), len);
        }
    }
    
    static bool readUint64(std::ifstream& in, uint64_t& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good() || in.eof();
    }
    
    static bool readUint32(std::ifstream& in, uint32_t& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good() || in.eof();
    }
    
    static bool readUint16(std::ifstream& in, uint16_t& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good() || in.eof();
    }
    
    static bool readString(std::ifstream& in, std::string& str) {
        uint32_t len = 0;
        if (!readUint32(in, len)) return false;
        
        if (len == 0) {
            str.clear();
            return true;
        }
        
        str.resize(len);
        in.read(&str[0], len);
        return in.good() || in.eof();
    }
};

class LiteratureDataFile {
public:
    LiteratureDataFile() : totalRecords_(0) {}
    
    bool openForWrite(const std::string& path) {
        outFile_.open(path, std::ios::binary);
        uint64_t header = 0;
        outFile_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        return outFile_.good();
    }
    
    bool openForRead(const std::string& path) {
        inFile_.open(path, std::ios::binary);
        if (!inFile_.good()) return false;
        
        inFile_.read(reinterpret_cast<char*>(&totalRecords_), sizeof(totalRecords_));
        if (!inFile_.good()) return false;
        
        offsets_.clear();
        offsets_.push_back(sizeof(uint64_t));
        
        Literature lit;
        for (uint64_t i = 0; i < totalRecords_; ++i) {
            inFile_.seekg(offsets_[i]);
            if (!Serializer::deserialize(inFile_, lit)) break;
            offsets_.push_back(inFile_.tellg());
        }
        
        return true;
    }
    
    bool append(const Literature& lit) {
        if (!Serializer::serialize(outFile_, lit)) return false;
        totalRecords_++;
        return true;
    }
    
    bool readAt(uint64_t index, Literature& lit) {
        if (index >= totalRecords_) return false;
        if (index >= offsets_.size()) return false;
        
        inFile_.seekg(offsets_[index]);
        return Serializer::deserialize(inFile_, lit);
    }
    
    void closeWrite() {
        if (outFile_.is_open()) {
            outFile_.seekp(0);
            outFile_.write(reinterpret_cast<const char*>(&totalRecords_), sizeof(totalRecords_));
            outFile_.close();
        }
    }
    
    void closeRead() {
        if (inFile_.is_open()) {
            inFile_.close();
        }
    }
    
    uint64_t getTotalRecords() const { return totalRecords_; }

private:
    std::ofstream outFile_;
    std::ifstream inFile_;
    uint64_t totalRecords_;
    std::vector<std::streampos> offsets_;
};

}
