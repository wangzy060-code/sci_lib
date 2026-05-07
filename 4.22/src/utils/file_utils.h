#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace litmgr {

class FileUtils {
public:
    static bool fileExists(const std::string& path) {
        return std::filesystem::exists(path);
    }
    
    static size_t getFileSize(const std::string& path) {
        if (!fileExists(path)) return 0;
        return std::filesystem::file_size(path);
    }
    
    static bool createDirectory(const std::string& path) {
        return std::filesystem::create_directories(path);
    }
    
    static bool removeFile(const std::string& path) {
        return std::filesystem::remove(path);
    }
    
    static std::string getAbsolutePath(const std::string& path) {
        return std::filesystem::absolute(path).string();
    }
};

class MemoryMappedFile {
public:
    MemoryMappedFile() : data_(nullptr), size_(0)
#ifdef _WIN32
        , fileHandle_(INVALID_HANDLE_VALUE), mapHandle_(nullptr)
#else
        , fd_(-1)
#endif
    {}
    
    ~MemoryMappedFile() {
        close();
    }
    
    bool open(const std::string& path, bool readOnly = true) {
        close();
        
#ifdef _WIN32
        DWORD access = readOnly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
        DWORD protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
        
        fileHandle_ = CreateFileA(path.c_str(), access, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (fileHandle_ == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(fileHandle_, &fileSize)) {
            CloseHandle(fileHandle_);
            fileHandle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        size_ = static_cast<size_t>(fileSize.QuadPart);
        
        mapHandle_ = CreateFileMappingA(fileHandle_, nullptr, protect, 0, 0, nullptr);
        if (mapHandle_ == nullptr) {
            CloseHandle(fileHandle_);
            fileHandle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        
        DWORD mapAccess = readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
        data_ = static_cast<uint8_t*>(MapViewOfFile(mapHandle_, mapAccess, 0, 0, 0));
        
        if (data_ == nullptr) {
            CloseHandle(mapHandle_);
            CloseHandle(fileHandle_);
            mapHandle_ = nullptr;
            fileHandle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        
        return true;
#else
        fd_ = ::open(path.c_str(), readOnly ? O_RDONLY : O_RDWR);
        if (fd_ == -1) {
            return false;
        }
        
        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        size_ = sb.st_size;
        
        int prot = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
        data_ = static_cast<uint8_t*>(mmap(nullptr, size_, prot, MAP_SHARED, fd_, 0));
        
        if (data_ == MAP_FAILED) {
            ::close(fd_);
            fd_ = -1;
            data_ = nullptr;
            return false;
        }
        
        return true;
#endif
    }
    
    void close() {
        if (data_) {
#ifdef _WIN32
            UnmapViewOfFile(data_);
#else
            munmap(data_, size_);
#endif
            data_ = nullptr;
        }
        
#ifdef _WIN32
        if (mapHandle_) {
            CloseHandle(mapHandle_);
            mapHandle_ = nullptr;
        }
        if (fileHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle_);
            fileHandle_ = INVALID_HANDLE_VALUE;
        }
#else
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
#endif
        size_ = 0;
    }
    
    uint8_t* data() { return data_; }
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    bool isOpen() const { return data_ != nullptr; }

private:
    uint8_t* data_;
    size_t size_;
    
#ifdef _WIN32
    HANDLE fileHandle_;
    HANDLE mapHandle_;
#else
    int fd_;
#endif
};

}
