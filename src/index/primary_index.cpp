#include "primary_index.h"

void PrimaryIndex::insert(const std::string& key, size_t rowPos) {
    indexMap[key] = rowPos;
}

bool PrimaryIndex::exists(const std::string& key) const {
    return indexMap.find(key) != indexMap.end();
}

size_t PrimaryIndex::get(const std::string& key) const {
    return indexMap.at(key);
}

void PrimaryIndex::reserve(size_t capacity) {
    indexMap.reserve(capacity);
}

void PrimaryIndex::clear() {
    indexMap.clear();
}
