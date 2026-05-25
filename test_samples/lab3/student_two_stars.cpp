#include "cc.h"

#include <algorithm>

std::size_t hash1(const std::string &key)
{
    std::size_t result = 1469598103934665603ull;
    for (unsigned char ch : key) {
        result ^= ch;
        result *= 1099511628211ull;
    }
    return result;
}

std::size_t hash2(const std::string &key)
{
    std::size_t result = 0;
    for (unsigned char ch : key) {
        result = result * 131u + ch;
    }
    return result;
}

std::size_t hash3(const std::string &key)
{
    std::size_t result = 5381u;
    for (unsigned char ch : key) {
        result = ((result << 5) + result) ^ ch;
    }
    return result;
}

HashTable::HashTable() : data(), count(0), collisions(0)
{
}

std::size_t HashTable::findSlot(const std::string &key) const
{
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i].occupied && data[i].key == key) {
            return i;
        }
    }
    return data.size();
}

std::size_t HashTable::findSlotForInsert(const std::string &key)
{
    const std::size_t existing = findSlot(key);
    if (existing < data.size()) {
        return existing;
    }
    return data.size();
}

void HashTable::rehash(std::size_t)
{
}

void HashTable::insert(const std::string &key, int value)
{
    const std::size_t existing = findSlot(key);
    if (existing < data.size()) {
        data[existing].value = value;
        return;
    }

    Entry entry;
    entry.key = key;
    entry.value = value;
    entry.occupied = true;
    entry.deleted = false;
    data.push_back(entry);
    ++count;

    int sameBucketBefore = 0;
    const std::size_t bucket = hash1(key) % 1024u;
    for (std::size_t i = 0; i + 1 < data.size(); ++i) {
        if (data[i].occupied && hash1(data[i].key) % 1024u == bucket) {
            ++sameBucketBefore;
        }
    }
    collisions += sameBucketBefore;
}

bool HashTable::contains(const std::string &key) const
{
    return findSlot(key) < data.size();
}

int HashTable::get(const std::string &key) const
{
    const std::size_t slot = findSlot(key);
    if (slot >= data.size()) {
        return 0;
    }
    return data[slot].value;
}

bool HashTable::remove(const std::string &key)
{
    for (auto it = data.begin(); it != data.end(); ++it) {
        if (it->occupied && it->key == key) {
            data.erase(it);
            --count;
            return true;
        }
    }
    return false;
}

int HashTable::size() const
{
    return count;
}

int HashTable::collisionCount() const
{
    return collisions;
}
