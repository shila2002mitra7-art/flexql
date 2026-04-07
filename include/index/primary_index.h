#ifndef PRIMARY_INDEX_H
#define PRIMARY_INDEX_H

#include <string>
#include <unordered_map>

class PrimaryIndex {
public:
    void insert(const std::string& key, size_t rowPos);
    bool exists(const std::string& key) const;
    size_t get(const std::string& key) const;
    void clear();

private:
    std::unordered_map<std::string, size_t> indexMap;
};

#endif