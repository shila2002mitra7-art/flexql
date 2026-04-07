#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <string>
#include <unordered_map>
#include <list>

class LRUCache {
public:
    LRUCache(size_t capacity = 100);

    bool exists(const std::string& key);
    std::string get(const std::string& key);
    void put(const std::string& key, const std::string& value);
    void clear();

private:
    size_t capacity;

    std::list<std::pair<std::string, std::string>> cacheList;

    std::unordered_map<
        std::string,
        std::list<std::pair<std::string, std::string>>::iterator
    > cacheMap;
};

#endif
