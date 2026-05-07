#pragma once

#include <list>
#include <unordered_map>
#include <mutex>

namespace litmgr {

template<typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}
    
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return false;
        }
        
        lruList_.splice(lruList_.begin(), lruList_, it->second.second);
        value = it->second.first;
        return true;
    }
    
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second.first = value;
            lruList_.splice(lruList_.begin(), lruList_, it->second.second);
            return;
        }
        
        if (cache_.size() >= capacity_) {
            auto last = lruList_.back();
            cache_.erase(last);
            lruList_.pop_back();
        }
        
        lruList_.push_front(key);
        cache_[key] = {value, lruList_.begin()};
    }
    
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.find(key) != cache_.end();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        lruList_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
    size_t capacity() const { return capacity_; }
    
    void setCapacity(size_t capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = capacity;
        
        while (cache_.size() > capacity_) {
            auto last = lruList_.back();
            cache_.erase(last);
            lruList_.pop_back();
        }
    }

private:
    size_t capacity_;
    std::list<Key> lruList_;
    std::unordered_map<Key, std::pair<Value, typename std::list<Key>::iterator>> cache_;
    mutable std::mutex mutex_;
};

}
