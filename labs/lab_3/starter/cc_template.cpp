#include "cc.h"

std::size_t hash1(const std::string &key)
{
    // TODO: первая собственная хеш-функция для строк.
    return key.size();
}

std::size_t hash2(const std::string &key)
{
    // TODO: вторая собственная хеш-функция для строк.
    return key.size();
}

std::size_t hash3(const std::string &key)
{
    // TODO: третья собственная хеш-функция для строк.
    return key.size();
}

HashTable::HashTable()
{
    // TODO: подготовить внутреннее хранилище таблицы.
}

void HashTable::insert(const std::string &key, int value)
{
    // TODO: добавить новый ключ или обновить существующий.
}

bool HashTable::contains(const std::string &key) const
{
    // TODO: вернуть true, если ключ есть в таблице.
    return false;
}

int HashTable::get(const std::string &key) const
{
    // TODO: вернуть значение по ключу.
    return 0;
}

bool HashTable::remove(const std::string &key)
{
    // TODO: удалить ключ и вернуть true, если он был найден.
    return false;
}

int HashTable::size() const
{
    // TODO: вернуть количество активных элементов.
    return 0;
}

int HashTable::collisionCount() const
{
    // TODO: вернуть количество коллизий, обнаруженных при вставках.
    return 0;
}
