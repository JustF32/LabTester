#pragma once

#include <cstddef>
#include <string>
#include <vector>

std::size_t hash1(const std::string &key);
std::size_t hash2(const std::string &key);
std::size_t hash3(const std::string &key);

class HashTable {
public:
    HashTable();

    void insert(const std::string &key, int value);
    bool contains(const std::string &key) const;
    int get(const std::string &key) const;
    bool remove(const std::string &key);
    int size() const;
    int collisionCount() const;

private:
    struct Entry {
        std::string key;
        int value = 0;
        bool occupied = false;
        bool deleted = false;
    };

    std::vector<Entry> data;
    int count = 0;
    int collisions = 0;

    std::size_t findSlot(const std::string &key) const;
    std::size_t findSlotForInsert(const std::string &key);
    void rehash(std::size_t newCapacity);
};
