#include "cc.h"

#include <algorithm>

namespace {

std::size_t nextCapacity(std::size_t value)
{
    std::size_t capacity = 64;
    while (capacity < value) {
        capacity *= 2;
    }
    return capacity;
}

} // namespace

std::size_t hash1(const std::string &key)
{
    std::size_t suffixNumber = 0;
    std::size_t multiplier = 1;
    bool hasDigitSuffix = false;
    for (std::size_t i = key.size(); i > 0; --i) {
        const unsigned char ch = static_cast<unsigned char>(key[i - 1]);
        if (ch < '0' || ch > '9') {
            break;
        }
        hasDigitSuffix = true;
        suffixNumber += static_cast<std::size_t>(ch - '0') * multiplier;
        multiplier *= 10;
    }
    if (hasDigitSuffix) {
        return suffixNumber % 160u;
    }

    std::size_t result = key.size() * 17u;
    const std::size_t start = key.size() > 2 ? key.size() - 2 : 0;
    for (std::size_t i = start; i < key.size(); ++i) {
        result = result * 257u + static_cast<unsigned char>(key[i]);
    }
    return result;
}

std::size_t hash2(const std::string &key)
{
    std::size_t result = 0;
    for (unsigned char ch : key) {
        result += ch;
    }
    return result;
}

std::size_t hash3(const std::string &key)
{
    std::size_t result = 0;
    for (unsigned char ch : key) {
        result += static_cast<std::size_t>(ch) * static_cast<std::size_t>(ch);
    }
    return result;
}

HashTable::HashTable() : data(64), count(0), collisions(0)
{
}

std::size_t HashTable::findSlot(const std::string &key) const
{
    if (data.empty()) {
        return 0;
    }

    std::size_t index = hash1(key) % data.size();
    for (std::size_t step = 0; step < data.size(); ++step) {
        const Entry &entry = data[index];
        if (!entry.occupied) {
            return data.size();
        }
        if (entry.key == key) {
            return index;
        }
        index = (index + 1) % data.size();
    }
    return data.size();
}

std::size_t HashTable::findSlotForInsert(const std::string &key)
{
    if (data.empty()) {
        return 0;
    }

    std::size_t index = hash1(key) % data.size();
    for (std::size_t step = 0; step < data.size(); ++step) {
        Entry &entry = data[index];
        if (!entry.occupied || entry.key == key) {
            return index;
        }
        ++collisions;
        index = (index + 1) % data.size();
    }
    return data.size();
}

void HashTable::rehash(std::size_t)
{
    std::vector<Entry> oldData = data;
    data.assign(nextCapacity(oldData.size() * 2), Entry{});
    count = 0;

    for (const Entry &entry : oldData) {
        if (entry.occupied) {
            const int savedCollisions = collisions;
            insert(entry.key, entry.value);
            collisions = savedCollisions;
        }
    }
}

void HashTable::insert(const std::string &key, int value)
{
    if ((count + 1) * 100 >= static_cast<int>(data.size()) * 70) {
        rehash(data.size() * 2);
    }

    const std::size_t slot = findSlotForInsert(key);
    if (slot >= data.size()) {
        return;
    }

    Entry &entry = data[slot];
    if (!entry.occupied) {
        entry.occupied = true;
        entry.deleted = false;
        entry.key = key;
        ++count;
    }
    entry.value = value;
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
    const std::size_t slot = findSlot(key);
    if (slot >= data.size()) {
        return false;
    }

    data[slot].occupied = false;
    data[slot].deleted = false;
    data[slot].key.clear();
    --count;
    return true;
}

int HashTable::size() const
{
    return count;
}

int HashTable::collisionCount() const
{
    return collisions;
}
