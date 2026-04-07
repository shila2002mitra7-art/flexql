#include "lru_cache.h"

LRUCache::LRUCache(size_t cap) : capacity(cap) {}

bool LRUCache::exists(const std::string& key) {
    return cacheMap.find(key) != cacheMap.end();
}

std::string LRUCache::get(const std::string& key) {
    auto it = cacheMap[key];

    auto node = *it;
    cacheList.erase(it);
    cacheList.push_front(node);

    cacheMap[key] = cacheList.begin();

    return node.second;
}

void LRUCache::put(const std::string& key, const std::string& value) {
    if (exists(key)) {
        cacheList.erase(cacheMap[key]);
    }

    cacheList.push_front({key, value});
    cacheMap[key] = cacheList.begin();

    if (cacheList.size() > capacity) {
        auto last = cacheList.back();
        cacheMap.erase(last.first);
        cacheList.pop_back();
    }
}

void LRUCache::clear() {
    cacheList.clear();
    cacheMap.clear();
}
