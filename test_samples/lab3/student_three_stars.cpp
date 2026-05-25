#include "cc.h"

#include <algorithm>

namespace {

constexpr double kMaxLoadFactor = 0.70;

std::size_t nextPowerOfTwo(std::size_t value)
{
    std::size_t result = 16;
    while (result < value) {
        result *= 2;
    }
    return result;
}

} // namespace

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
    std::size_t result = 0x9e3779b97f4a7c15ull;
    for (unsigned char ch : key) {
        result ^= static_cast<std::size_t>(ch) + 0x9e3779b97f4a7c15ull + (result << 6) + (result >> 2);
    }
    return result;
}

HashTable::HashTable() : data(128), count(0), collisions(0)
{
}

std::size_t HashTable::findSlot(const std::string &key) const
{
    if (data.empty()) {
        return 0;
    }

    std::size_t index = hash1(key) & (data.size() - 1);
    for (std::size_t step = 0; step < data.size(); ++step) {
        const Entry &entry = data[index];
        if (!entry.occupied && !entry.deleted) {
            return data.size();
        }
        if (entry.occupied && entry.key == key) {
            return index;
        }
        index = (index + 1) & (data.size() - 1);
    }
    return data.size();
}

std::size_t HashTable::findSlotForInsert(const std::string &key)
{
    std::size_t firstDeleted = data.size();
    std::size_t index = hash1(key) & (data.size() - 1);

    for (std::size_t step = 0; step < data.size(); ++step) {
        Entry &entry = data[index];
        if (entry.occupied && entry.key == key) {
            return index;
        }
        if (!entry.occupied) {
            if (entry.deleted) {
                if (firstDeleted == data.size()) {
                    firstDeleted = index;
                }
            } else {
                return firstDeleted != data.size() ? firstDeleted : index;
            }
        } else {
            ++collisions;
        }
        index = (index + 1) & (data.size() - 1);
    }

    return firstDeleted;
}

void HashTable::rehash(std::size_t newCapacity)
{
    std::vector<Entry> oldData = data;
    data.assign(nextPowerOfTwo(newCapacity), Entry{});
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
    if (data.empty()) {
        data.assign(128, Entry{});
    }
    if ((static_cast<double>(count + 1) / static_cast<double>(data.size())) > kMaxLoadFactor) {
        rehash(data.size() * 2);
    }

    std::size_t slot = findSlotForInsert(key);
    if (slot >= data.size()) {
        rehash(data.size() * 2);
        slot = findSlotForInsert(key);
    }

    Entry &entry = data[slot];
    if (!entry.occupied) {
        entry.key = key;
        entry.occupied = true;
        entry.deleted = false;
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

    data[slot].key.clear();
    data[slot].value = 0;
    data[slot].occupied = false;
    data[slot].deleted = true;
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
