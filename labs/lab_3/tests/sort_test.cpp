#include "cc.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

void recordCase(const std::string &input, const std::string &expected, const std::string &actual)
{
    ::testing::Test::RecordProperty("input_data", input);
    ::testing::Test::RecordProperty("expected_output", expected);
    ::testing::Test::RecordProperty("actual_output", actual);
}

void requireTrue(bool condition, const std::string &message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void runTimedCase(const std::string &name, long long maxMilliseconds, const std::function<void()> &body)
{
    auto done = std::make_shared<std::promise<void>>();
    std::future<void> future = done->get_future();
    auto error = std::make_shared<std::string>();

    const auto started = std::chrono::steady_clock::now();
    std::thread worker([body, done, error]() {
        try {
            body();
        } catch (const std::exception &ex) {
            *error = ex.what();
        } catch (...) {
            *error = "Неизвестное исключение.";
        }
        done->set_value();
    });
    worker.detach();

    if (future.wait_for(std::chrono::milliseconds(maxMilliseconds)) != std::future_status::ready) {
        const std::string expected = "Лимит времени: " + std::to_string(maxMilliseconds)
            + " мс; операции должны завершиться в лимит и вернуть корректный результат.";
        const std::string actual = "Превышен лимит времени: проверка не завершилась за "
            + std::to_string(maxMilliseconds) + " мс.";
        recordCase(name, expected, actual);
        ::testing::Test::RecordProperty("duration_ms", std::to_string(maxMilliseconds));
        ADD_FAILURE() << "Runtime error: " << actual;
        return;
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
    ::testing::Test::RecordProperty("duration_ms", std::to_string(elapsedMs));

    const std::string expected = "Лимит времени: " + std::to_string(maxMilliseconds)
        + " мс; результат операций должен быть корректным.";
    if (!error->empty()) {
        recordCase(name, expected, "Исключение во время выполнения: " + *error);
        FAIL() << *error;
        return;
    }

    const bool hadCorrectnessFailure = ::testing::Test::HasFailure();
    const std::string actual = hadCorrectnessFailure
        ? "Выполнено за " + std::to_string(elapsedMs)
            + " мс, но проверка результата провалена. Подробности ниже."
        : (elapsedMs > maxMilliseconds
            ? "Выполнено за " + std::to_string(elapsedMs) + " мс; лимит "
                + std::to_string(maxMilliseconds) + " мс превышен."
            : "Выполнено за " + std::to_string(elapsedMs) + " мс; лимит не превышен.");
    recordCase(name, expected, actual);

    EXPECT_LE(elapsedMs, maxMilliseconds)
        << "Реализация работает слишком медленно: " << elapsedMs
        << " мс при лимите " << maxMilliseconds << " мс.";
}

std::string keyNumber(const std::string &prefix, int value)
{
    std::ostringstream out;
    out << prefix << std::setw(6) << std::setfill('0') << value;
    return out.str();
}

std::vector<std::string> makeRandomStrings(int count, int seed = 42)
{
    std::vector<std::string> keys;
    keys.reserve(count);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> lenDist(8, 24);
    std::uniform_int_distribution<int> charDist(0, 61);

    for (int i = 0; i < count; ++i) {
        const int length = lenDist(rng);
        std::string key;
        key.reserve(length + 8);
        for (int j = 0; j < length; ++j) {
            const int value = charDist(rng);
            if (value < 10) {
                key.push_back(static_cast<char>('0' + value));
            } else if (value < 36) {
                key.push_back(static_cast<char>('a' + value - 10));
            } else {
                key.push_back(static_cast<char>('A' + value - 36));
            }
        }
        key += "_" + std::to_string(i);
        keys.push_back(key);
    }
    return keys;
}

std::vector<std::string> makeOrderedKeys(int count)
{
    std::vector<std::string> keys;
    keys.reserve(count);
    for (int i = 0; i < count; ++i) {
        keys.push_back(keyNumber("user_", i));
    }
    return keys;
}

std::vector<std::string> makeRealNames()
{
    const std::vector<std::string> firstNames = {
        "anna", "ivan", "petr", "maria", "sergey", "daria", "alexey", "olga",
        "nikita", "elena", "vladislav", "kirill", "sofia", "roman", "polina"
    };
    const std::vector<std::string> lastNames = {
        "ivanov", "petrov", "sidorov", "smirnov", "volkov", "orlov", "fedorov",
        "morozov", "novikov", "egorov", "lebedev", "kozlov", "sokolov"
    };

    std::vector<std::string> names;
    for (const auto &first : firstNames) {
        for (const auto &last : lastNames) {
            names.push_back(first + "." + last);
        }
    }
    return names;
}

std::vector<std::string> makeAnagrams()
{
    std::vector<std::string> result;
    const std::vector<std::string> bases = {"abcdef", "student", "hasher", "listen", "binary"};
    for (const auto &base : bases) {
        std::string value = base;
        std::sort(value.begin(), value.end());
        int produced = 0;
        do {
            result.push_back(value);
            ++produced;
        } while (produced < 60 && std::next_permutation(value.begin(), value.end()));
    }
    return result;
}

std::vector<std::string> makeSimilarTextKeys(int count)
{
    std::vector<std::string> keys;
    keys.reserve(count);
    for (int i = 0; i < count; ++i) {
        keys.push_back("hash-table-collision-analysis-chapter-" + std::to_string(i));
    }
    return keys;
}

struct DistributionStats {
    int occupied = 0;
    int collisions = 0;
    int maxBucket = 0;
};

DistributionStats distributionFor(
    const std::vector<std::string> &keys,
    int bucketCount,
    const std::function<std::size_t(const std::string &)> &hashFunction)
{
    std::vector<int> buckets(bucketCount, 0);
    for (const auto &key : keys) {
        buckets[hashFunction(key) % static_cast<std::size_t>(bucketCount)] += 1;
    }

    DistributionStats stats;
    for (int bucket : buckets) {
        if (bucket > 0) {
            stats.occupied += 1;
            stats.collisions += bucket - 1;
            stats.maxBucket = std::max(stats.maxBucket, bucket);
        }
    }
    return stats;
}

DistributionStats bestDistribution(const std::vector<std::string> &keys, int bucketCount)
{
    const std::vector<DistributionStats> stats = {
        distributionFor(keys, bucketCount, hash1),
        distributionFor(keys, bucketCount, hash2),
        distributionFor(keys, bucketCount, hash3)
    };

    return *std::min_element(stats.begin(), stats.end(), [](const auto &left, const auto &right) {
        if (left.maxBucket != right.maxBucket) {
            return left.maxBucket < right.maxBucket;
        }
        return left.collisions < right.collisions;
    });
}

int distinctBuckets(
    const std::vector<std::string> &keys,
    int bucketCount,
    const std::function<std::size_t(const std::string &)> &hashFunction)
{
    std::unordered_set<std::size_t> buckets;
    for (const auto &key : keys) {
        buckets.insert(hashFunction(key) % static_cast<std::size_t>(bucketCount));
    }
    return static_cast<int>(buckets.size());
}

std::string distributionText(const DistributionStats &stats)
{
    return "occupied=" + std::to_string(stats.occupied)
        + "; collisions=" + std::to_string(stats.collisions)
        + "; maxBucket=" + std::to_string(stats.maxBucket);
}

std::vector<std::string> findHash1CollisionKeys(int count, int modulo)
{
    std::vector<std::vector<std::string>> buckets(modulo);
    for (int i = 0; i < 100000; ++i) {
        std::string key = "collision_key_" + std::to_string(i);
        buckets[hash1(key) % static_cast<std::size_t>(modulo)].push_back(key);
        if (static_cast<int>(buckets[hash1(key) % static_cast<std::size_t>(modulo)].size()) >= count) {
            return buckets[hash1(key) % static_cast<std::size_t>(modulo)];
        }
    }
    return {};
}

} // namespace

TEST(HashFunctions_Basic, SameInputIsStable)
{
    const std::string key = "student_42";
    recordCase(key, "Все три функции возвращают одинаковый результат при повторном вызове.", "hash1/hash2/hash3 вызваны дважды.");
    EXPECT_EQ(hash1(key), hash1(key));
    EXPECT_EQ(hash2(key), hash2(key));
    EXPECT_EQ(hash3(key), hash3(key));
}

TEST(HashFunctions_Basic, EmptyStringDoesNotCrash)
{
    recordCase("empty string", "Функции обрабатывают пустую строку без исключения.", "hash1=" + std::to_string(hash1("")) + "; hash2=" + std::to_string(hash2("")) + "; hash3=" + std::to_string(hash3("")));
    SUCCEED();
}

TEST(HashFunctions_Basic, CaseCanAffectHash)
{
    const bool changed = hash1("hash") != hash1("HASH") || hash2("hash") != hash2("HASH") || hash3("hash") != hash3("HASH");
    recordCase("hash / HASH", "Хотя бы одна функция различает регистр.", changed ? "Регистр влияет." : "Все функции игнорируют регистр.");
    EXPECT_TRUE(changed);
}

TEST(HashFunctions_Basic, OrderCanAffectHash)
{
    const bool changed = hash1("abc") != hash1("cba") || hash2("abc") != hash2("cba") || hash3("abc") != hash3("cba");
    recordCase("abc / cba", "Хотя бы одна функция учитывает порядок символов.", changed ? "Порядок влияет." : "Порядок не влияет.");
    EXPECT_TRUE(changed);
}

TEST(HashFunctions_Basic, LastCharacterCanAffectHash)
{
    const bool changed = hash1("prefix_a") != hash1("prefix_b") || hash2("prefix_a") != hash2("prefix_b") || hash3("prefix_a") != hash3("prefix_b");
    recordCase("prefix_a / prefix_b", "Хотя бы одна функция учитывает последний символ.", changed ? "Последний символ влияет." : "Последний символ не влияет.");
    EXPECT_TRUE(changed);
}

TEST(HashFunctions_Basic, DigitsCanAffectHash)
{
    const bool changed = hash1("user1") != hash1("user2") || hash2("user1") != hash2("user2") || hash3("user1") != hash3("user2");
    recordCase("user1 / user2", "Хотя бы одна функция учитывает цифры.", changed ? "Цифры влияют." : "Цифры не влияют.");
    EXPECT_TRUE(changed);
}

TEST(HashFunctions_Basic, SpacesCanAffectHash)
{
    const bool changed = hash1("hash table") != hash1("hashtable") || hash2("hash table") != hash2("hashtable") || hash3("hash table") != hash3("hashtable");
    recordCase("hash table / hashtable", "Хотя бы одна функция учитывает пробелы.", changed ? "Пробелы влияют." : "Пробелы не влияют.");
    EXPECT_TRUE(changed);
}

TEST(HashFunctions_Basic, LongStringIsStable)
{
    const std::string key(5000, 'x');
    recordCase("5000 символов x", "Хеш длинной строки стабилен.", "hash1=" + std::to_string(hash1(key)));
    EXPECT_EQ(hash1(key), hash1(key));
    EXPECT_EQ(hash2(key), hash2(key));
    EXPECT_EQ(hash3(key), hash3(key));
}

TEST(HashFunctions_Basic, Utf8BytesAreAccepted)
{
    const std::string key = "ключ_данных";
    recordCase(key, "UTF-8 строка не ломает хеш-функции.", "hash1=" + std::to_string(hash1(key)));
    EXPECT_EQ(hash1(key), hash1(key));
    EXPECT_EQ(hash2(key), hash2(key));
    EXPECT_EQ(hash3(key), hash3(key));
}

TEST(HashFunctions_Basic, AtLeastTwoFunctionsAreDifferent)
{
    const std::string key = "hash_quality_probe";
    const int distinct = static_cast<int>(std::unordered_set<std::size_t>{hash1(key), hash2(key), hash3(key)}.size());
    recordCase(key, "Минимум две функции дают разные значения.", "distinct=" + std::to_string(distinct));
    EXPECT_GE(distinct, 2);
}

TEST(HashFunctions_Advanced, TwoFunctionsUseCharacterOrder)
{
    int changed = 0;
    changed += hash1("abcdef") != hash1("fedcba") ? 1 : 0;
    changed += hash2("abcdef") != hash2("fedcba") ? 1 : 0;
    changed += hash3("abcdef") != hash3("fedcba") ? 1 : 0;
    recordCase("abcdef / fedcba", "Минимум две функции чувствительны к порядку.", "changed=" + std::to_string(changed));
    EXPECT_GE(changed, 2);
}

TEST(HashFunctions_Advanced, TwoFunctionsUseSuffix)
{
    int changed = 0;
    changed += hash1("same_prefix_A") != hash1("same_prefix_B") ? 1 : 0;
    changed += hash2("same_prefix_A") != hash2("same_prefix_B") ? 1 : 0;
    changed += hash3("same_prefix_A") != hash3("same_prefix_B") ? 1 : 0;
    recordCase("same_prefix_A / same_prefix_B", "Минимум две функции учитывают хвост строки.", "changed=" + std::to_string(changed));
    EXPECT_GE(changed, 2);
}

TEST(HashFunctions_Advanced, NullByteInsideString)
{
    const std::string withNull("abc\0def", 7);
    const std::string withoutNull = "abcdef";
    int changed = 0;
    changed += hash1(withNull) != hash1(withoutNull) ? 1 : 0;
    changed += hash2(withNull) != hash2(withoutNull) ? 1 : 0;
    changed += hash3(withNull) != hash3(withoutNull) ? 1 : 0;
    recordCase("abc\\0def / abcdef", "Минимум две функции учитывают нулевой байт внутри строки.", "changed=" + std::to_string(changed));
    EXPECT_GE(changed, 2);
}

TEST(HashFunctions_Advanced, SimilarLongStringsDoNotCollapse)
{
    const std::string a = std::string(1000, 'a') + "x";
    const std::string b = std::string(1000, 'a') + "y";
    int changed = 0;
    changed += hash1(a) != hash1(b) ? 1 : 0;
    changed += hash2(a) != hash2(b) ? 1 : 0;
    changed += hash3(a) != hash3(b) ? 1 : 0;
    recordCase("1000 одинаковых символов + разный хвост", "Минимум две функции различают похожие длинные строки.", "changed=" + std::to_string(changed));
    EXPECT_GE(changed, 2);
}

TEST(HashFunctions_Advanced, FunctionsAreNotJustLength)
{
    const std::vector<std::string> keys = {"aa", "bb", "cc", "dd", "ee", "ff"};
    int goodFunctions = 0;
    goodFunctions += distinctBuckets(keys, 101, hash1) >= 4 ? 1 : 0;
    goodFunctions += distinctBuckets(keys, 101, hash2) >= 4 ? 1 : 0;
    goodFunctions += distinctBuckets(keys, 101, hash3) >= 4 ? 1 : 0;
    recordCase("aa, bb, cc, dd, ee, ff", "Минимум две функции различают строки одинаковой длины.", "goodFunctions=" + std::to_string(goodFunctions));
    EXPECT_GE(goodFunctions, 2);
}

TEST(HashFunctions_Performance, Hash1ManyStrings)
{
    runTimedCase("hash1 для 200000 строк", 1200, []() {
        const auto keys = makeRandomStrings(200000, 11);
        std::size_t checksum = 0;
        for (const auto &key : keys) {
            checksum ^= hash1(key);
        }
        requireTrue(checksum != 0, "hash1 не должен сводить все значения к нулю.");
    });
}

TEST(HashFunctions_Performance, Hash2ManyStrings)
{
    runTimedCase("hash2 для 200000 строк", 1200, []() {
        const auto keys = makeRandomStrings(200000, 12);
        std::size_t checksum = 0;
        for (const auto &key : keys) {
            checksum ^= hash2(key);
        }
        requireTrue(checksum != 0, "hash2 не должен сводить все значения к нулю.");
    });
}

TEST(HashFunctions_Performance, Hash3ManyStrings)
{
    runTimedCase("hash3 для 200000 строк", 1200, []() {
        const auto keys = makeRandomStrings(200000, 13);
        std::size_t checksum = 0;
        for (const auto &key : keys) {
            checksum ^= hash3(key);
        }
        requireTrue(checksum != 0, "hash3 не должен сводить все значения к нулю.");
    });
}

TEST(HashFunctions_Performance, RepeatedLongText)
{
    runTimedCase("хеширование длинного текста 1200 раз", 1600, []() {
        std::string text = std::string(4000, 'a') + "hash-tail-0000";
        std::size_t checksum = 0;
        for (int i = 0; i < 1200; ++i) {
            text[text.size() - 1] = static_cast<char>('0' + (i % 10));
            text[text.size() - 2] = static_cast<char>('0' + ((i / 10) % 10));
            text[text.size() - 3] = static_cast<char>('0' + ((i / 100) % 10));
            checksum += hash1(text);
            checksum += hash2(text);
            checksum += hash3(text);
        }
        requireTrue(checksum != 0, "Хеши длинных строк не должны быть нулевыми.");
    });
}

TEST(HashFunctions_Performance, OrderedKeysHashFast)
{
    runTimedCase("хеширование 250000 упорядоченных ключей", 1600, []() {
        std::size_t checksum = 0;
        for (int i = 0; i < 250000; ++i) {
            const std::string key = keyNumber("ordered_", i);
            checksum ^= hash1(key) + hash2(key) + hash3(key);
        }
        requireTrue(checksum != 0, "Контрольная сумма хешей не должна быть нулевой.");
    });
}

TEST(HashDistribution_Basic, Hash1UsesDifferentBuckets)
{
    const auto keys = makeRandomStrings(80, 1);
    const int distinct = distinctBuckets(keys, 127, hash1);
    recordCase("80 случайных строк, 127 бакетов", "hash1 занимает минимум 45 бакетов.", "occupied=" + std::to_string(distinct));
    EXPECT_GE(distinct, 45);
}

TEST(HashDistribution_Basic, Hash2UsesDifferentBuckets)
{
    const auto keys = makeRandomStrings(80, 2);
    const int distinct = distinctBuckets(keys, 127, hash2);
    recordCase("80 случайных строк, 127 бакетов", "hash2 занимает минимум 45 бакетов.", "occupied=" + std::to_string(distinct));
    EXPECT_GE(distinct, 45);
}

TEST(HashDistribution_Basic, Hash3UsesDifferentBuckets)
{
    const auto keys = makeRandomStrings(80, 3);
    const int distinct = distinctBuckets(keys, 127, hash3);
    recordCase("80 случайных строк, 127 бакетов", "hash3 занимает минимум 45 бакетов.", "occupied=" + std::to_string(distinct));
    EXPECT_GE(distinct, 45);
}

TEST(HashDistribution_Basic, BestFunctionHandlesShortWords)
{
    const std::vector<std::string> keys = {"one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"};
    const auto stats = bestDistribution(keys, 31);
    recordCase("короткие слова", "Лучший вариант: maxBucket <= 3.", distributionText(stats));
    EXPECT_LE(stats.maxBucket, 3);
}

TEST(HashDistribution_Basic, BestFunctionHandlesDifferentLengths)
{
    std::vector<std::string> keys;
    for (int i = 1; i <= 60; ++i) {
        keys.push_back(std::string(i, static_cast<char>('a' + i % 20)));
    }
    const auto stats = bestDistribution(keys, 127);
    recordCase("строки разной длины", "Лучший вариант: occupied >= 45.", distributionText(stats));
    EXPECT_GE(stats.occupied, 45);
}

TEST(HashDistribution_Basic, BestFunctionHandlesNumbersInKeys)
{
    const auto keys = makeOrderedKeys(120);
    const auto stats = bestDistribution(keys, 127);
    recordCase("user_000000..user_000119", "Лучший вариант: occupied >= 70.", distributionText(stats));
    EXPECT_GE(stats.occupied, 70);
}

TEST(HashDistribution_Basic, BestFunctionHandlesNames)
{
    const auto names = makeRealNames();
    const auto stats = bestDistribution(names, 257);
    recordCase("195 имен", "Лучший вариант: maxBucket <= 5.", distributionText(stats));
    EXPECT_LE(stats.maxBucket, 5);
}

TEST(HashDistribution_Basic, BestFunctionHandlesTextLikeKeys)
{
    const auto keys = makeSimilarTextKeys(150);
    const auto stats = bestDistribution(keys, 257);
    recordCase("похожие текстовые ключи", "Лучший вариант: occupied >= 90.", distributionText(stats));
    EXPECT_GE(stats.occupied, 90);
}

TEST(HashDistribution_Basic, BestFunctionHandlesCaseVariants)
{
    std::vector<std::string> keys;
    for (char c = 'a'; c <= 'z'; ++c) {
        keys.push_back(std::string("case_") + c);
        keys.push_back(std::string("case_") + static_cast<char>(std::toupper(c)));
    }
    const auto stats = bestDistribution(keys, 101);
    recordCase("case_a..case_Z", "Лучший вариант: occupied >= 35.", distributionText(stats));
    EXPECT_GE(stats.occupied, 35);
}

TEST(HashDistribution_Basic, BestFunctionKeepsCollisionCountReasonable)
{
    const auto keys = makeRandomStrings(200, 4);
    const auto stats = bestDistribution(keys, 257);
    recordCase("200 случайных строк, 257 бакетов", "Лучший вариант: collisions <= 80.", distributionText(stats));
    EXPECT_LE(stats.collisions, 80);
}

TEST(HashDistribution_Advanced, OrderedKeysDoNotCollapse)
{
    const auto keys = makeOrderedKeys(2000);
    const auto stats = bestDistribution(keys, 1009);
    recordCase("2000 упорядоченных ключей", "Лучший вариант: occupied >= 750 и maxBucket <= 8.", distributionText(stats));
    EXPECT_GE(stats.occupied, 750);
    EXPECT_LE(stats.maxBucket, 8);
}

TEST(HashDistribution_Advanced, AnagramsDoNotCollapse)
{
    const auto keys = makeAnagrams();
    const auto stats = bestDistribution(keys, 1009);
    recordCase("набор анаграмм", "Лучший вариант: occupied >= 120 и maxBucket <= 6.", distributionText(stats));
    EXPECT_GE(stats.occupied, 120);
    EXPECT_LE(stats.maxBucket, 6);
}

TEST(HashDistribution_Advanced, SimilarPrefixesDoNotCollapse)
{
    const auto keys = makeSimilarTextKeys(1500);
    const auto stats = bestDistribution(keys, 1009);
    recordCase("1500 ключей с одинаковым префиксом", "Лучший вариант: occupied >= 700 и maxBucket <= 8.", distributionText(stats));
    EXPECT_GE(stats.occupied, 700);
    EXPECT_LE(stats.maxBucket, 8);
}

TEST(HashDistribution_Advanced, RealNamesRemainBalanced)
{
    auto names = makeRealNames();
    for (int i = 0; i < 8; ++i) {
        const auto more = makeRealNames();
        names.insert(names.end(), more.begin(), more.end());
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        names[i] += "_" + std::to_string(i);
    }
    const auto stats = bestDistribution(names, 2003);
    recordCase("расширенный набор имен", "Лучший вариант: maxBucket <= 6.", distributionText(stats));
    EXPECT_LE(stats.maxBucket, 6);
}

TEST(HashDistribution_Advanced, AtLeastTwoFunctionsAreUsable)
{
    const auto keys = makeOrderedKeys(1000);
    int usable = 0;
    for (const auto &hashFunction : {std::function<std::size_t(const std::string &)>(hash1), std::function<std::size_t(const std::string &)>(hash2), std::function<std::size_t(const std::string &)>(hash3)}) {
        const auto stats = distributionFor(keys, 1009, hashFunction);
        if (stats.occupied >= 500 && stats.maxBucket <= 10) {
            usable += 1;
        }
    }
    recordCase("1000 ordered keys", "Минимум две из трех функций имеют приемлемое распределение.", "usable=" + std::to_string(usable));
    EXPECT_GE(usable, 2);
}

TEST(HashDistribution_Performance, RandomLargeDistribution)
{
    runTimedCase("распределение 60000 случайных строк", 900, []() {
        const auto keys = makeRandomStrings(60000, 21);
        const auto stats = bestDistribution(keys, 32749);
        requireTrue(stats.maxBucket <= 10, "Слишком большой максимальный бакет: " + std::to_string(stats.maxBucket));
        requireTrue(stats.occupied >= 25000, "Слишком мало занятых бакетов: " + std::to_string(stats.occupied));
    });
}

TEST(HashDistribution_Performance, OrderedLargeDistribution)
{
    runTimedCase("распределение 60000 ordered keys", 900, []() {
        const auto keys = makeOrderedKeys(60000);
        const auto stats = bestDistribution(keys, 32749);
        requireTrue(stats.maxBucket <= 10, "Слишком большой максимальный бакет: " + std::to_string(stats.maxBucket));
        requireTrue(stats.occupied >= 25000, "Слишком мало занятых бакетов: " + std::to_string(stats.occupied));
    });
}

TEST(HashDistribution_Performance, SimilarLargeDistribution)
{
    runTimedCase("распределение 50000 похожих строк", 900, []() {
        const auto keys = makeSimilarTextKeys(50000);
        const auto stats = bestDistribution(keys, 32749);
        requireTrue(stats.maxBucket <= 12, "Слишком большой максимальный бакет: " + std::to_string(stats.maxBucket));
        requireTrue(stats.occupied >= 23000, "Слишком мало занятых бакетов: " + std::to_string(stats.occupied));
    });
}

TEST(HashDistribution_Performance, NamesLargeDistribution)
{
    runTimedCase("распределение большого набора имен", 900, []() {
        std::vector<std::string> names;
        for (int i = 0; i < 300; ++i) {
            auto part = makeRealNames();
            for (auto &name : part) {
                name += "_" + std::to_string(i);
            }
            names.insert(names.end(), part.begin(), part.end());
        }
        const auto stats = bestDistribution(names, 32749);
        requireTrue(stats.maxBucket <= 12, "Слишком большой максимальный бакет: " + std::to_string(stats.maxBucket));
        requireTrue(stats.occupied >= 22000, "Слишком мало занятых бакетов: " + std::to_string(stats.occupied));
    });
}

TEST(HashDistribution_Performance, AllFunctionsLargeEnough)
{
    runTimedCase("качество всех функций на 20000 ключей", 900, []() {
        const auto keys = makeOrderedKeys(20000);
        int usable = 0;
        for (const auto &hashFunction : {std::function<std::size_t(const std::string &)>(hash1), std::function<std::size_t(const std::string &)>(hash2), std::function<std::size_t(const std::string &)>(hash3)}) {
            const auto stats = distributionFor(keys, 20011, hashFunction);
            if (stats.occupied >= 9000 && stats.maxBucket <= 8) {
                usable += 1;
            }
        }
        requireTrue(usable >= 2, "Минимум две функции должны оставаться приемлемыми на большом наборе.");
    });
}

TEST(HashTable_Basic, StartsEmpty)
{
    HashTable table;
    recordCase("новая таблица", "size=0; contains=false", "size=" + std::to_string(table.size()) + "; contains=" + std::to_string(table.contains("a")));
    EXPECT_EQ(table.size(), 0);
    EXPECT_FALSE(table.contains("a"));
}

TEST(HashTable_Basic, InsertAndContains)
{
    HashTable table;
    table.insert("alpha", 10);
    recordCase("insert alpha=10", "contains(alpha)=true; size=1", "contains=" + std::to_string(table.contains("alpha")) + "; size=" + std::to_string(table.size()));
    EXPECT_TRUE(table.contains("alpha"));
    EXPECT_EQ(table.size(), 1);
}

TEST(HashTable_Basic, InsertAndGet)
{
    HashTable table;
    table.insert("alpha", 10);
    recordCase("insert alpha=10; get alpha", "10", std::to_string(table.get("alpha")));
    EXPECT_EQ(table.get("alpha"), 10);
}

TEST(HashTable_Basic, MultipleElements)
{
    HashTable table;
    table.insert("a", 1);
    table.insert("b", 2);
    table.insert("c", 3);
    recordCase("insert a,b,c", "size=3; get b=2", "size=" + std::to_string(table.size()) + "; get b=" + std::to_string(table.get("b")));
    EXPECT_EQ(table.size(), 3);
    EXPECT_EQ(table.get("b"), 2);
}

TEST(HashTable_Basic, UpdateExistingKey)
{
    HashTable table;
    table.insert("same", 1);
    table.insert("same", 7);
    recordCase("insert same=1; insert same=7", "size=1; get same=7", "size=" + std::to_string(table.size()) + "; get same=" + std::to_string(table.get("same")));
    EXPECT_EQ(table.size(), 1);
    EXPECT_EQ(table.get("same"), 7);
}

TEST(HashTable_Basic, RemoveExistingKey)
{
    HashTable table;
    table.insert("gone", 5);
    const bool removed = table.remove("gone");
    recordCase("insert gone; remove gone", "removed=true; contains=false; size=0", "removed=" + std::to_string(removed) + "; contains=" + std::to_string(table.contains("gone")) + "; size=" + std::to_string(table.size()));
    EXPECT_TRUE(removed);
    EXPECT_FALSE(table.contains("gone"));
    EXPECT_EQ(table.size(), 0);
}

TEST(HashTable_Basic, RemoveMissingKey)
{
    HashTable table;
    table.insert("a", 1);
    const bool removed = table.remove("missing");
    recordCase("insert a; remove missing", "removed=false; size=1", "removed=" + std::to_string(removed) + "; size=" + std::to_string(table.size()));
    EXPECT_FALSE(removed);
    EXPECT_EQ(table.size(), 1);
}

TEST(HashTable_Basic, ReinsertAfterRemove)
{
    HashTable table;
    table.insert("key", 1);
    table.remove("key");
    table.insert("key", 2);
    recordCase("insert key=1; remove; insert key=2", "contains=true; get=2; size=1", "contains=" + std::to_string(table.contains("key")) + "; get=" + std::to_string(table.get("key")) + "; size=" + std::to_string(table.size()));
    EXPECT_TRUE(table.contains("key"));
    EXPECT_EQ(table.get("key"), 2);
    EXPECT_EQ(table.size(), 1);
}

TEST(HashTable_Basic, EmptyStringKey)
{
    HashTable table;
    table.insert("", 42);
    recordCase("insert empty key=42", "contains=true; get=42", "contains=" + std::to_string(table.contains("")) + "; get=" + std::to_string(table.get("")));
    EXPECT_TRUE(table.contains(""));
    EXPECT_EQ(table.get(""), 42);
}

TEST(HashTable_Basic, CollisionCounterIsNonNegative)
{
    HashTable table;
    table.insert("a", 1);
    table.insert("b", 2);
    recordCase("insert a,b", "collisionCount >= 0", "collisionCount=" + std::to_string(table.collisionCount()));
    EXPECT_GE(table.collisionCount(), 0);
}

TEST(HashTable_Advanced, GrowsBeyondInitialCapacity)
{
    HashTable table;
    for (int i = 0; i < 150; ++i) {
        table.insert(keyNumber("key_", i), i);
    }
    recordCase("insert key_000000..key_000149", "size=150; все значения доступны", "size=" + std::to_string(table.size()));
    ASSERT_EQ(table.size(), 150);
    for (int i = 0; i < 150; ++i) {
        EXPECT_EQ(table.get(keyNumber("key_", i)), i);
    }
}

TEST(HashTable_Advanced, RemoveKeepsProbeChain)
{
    const auto keys = findHash1CollisionKeys(4, 64);
    ASSERT_GE(keys.size(), 4u);
    HashTable table;
    for (int i = 0; i < 4; ++i) {
        table.insert(keys[i], i + 1);
    }
    table.remove(keys[1]);
    recordCase("4 ключа с одинаковым hash1 % 64; удалить второй", "последующие ключи остаются доступными", "contains third=" + std::to_string(table.contains(keys[2])) + "; contains fourth=" + std::to_string(table.contains(keys[3])));
    EXPECT_FALSE(table.contains(keys[1]));
    EXPECT_TRUE(table.contains(keys[2]));
    EXPECT_TRUE(table.contains(keys[3]));
    EXPECT_EQ(table.get(keys[2]), 3);
}

TEST(HashTable_Advanced, RehashPreservesValues)
{
    HashTable table;
    for (int i = 0; i < 500; ++i) {
        table.insert(keyNumber("rehash_", i), i * 3);
    }
    recordCase("500 вставок", "каждое седьмое значение сохранено после роста таблицы", "size=" + std::to_string(table.size()));
    ASSERT_EQ(table.size(), 500);
    for (int i = 0; i < 500; i += 7) {
        EXPECT_EQ(table.get(keyNumber("rehash_", i)), i * 3);
    }
}

TEST(HashTable_Advanced, ManyDuplicateUpdatesDoNotGrowSize)
{
    HashTable table;
    for (int i = 0; i < 200; ++i) {
        table.insert("duplicate", i);
    }
    recordCase("200 обновлений одного ключа", "size=1; get duplicate=199", "size=" + std::to_string(table.size()) + "; get=" + std::to_string(table.get("duplicate")));
    EXPECT_EQ(table.size(), 1);
    EXPECT_EQ(table.get("duplicate"), 199);
}

TEST(HashTable_Advanced, RemoveManyAndInsertAgain)
{
    HashTable table;
    for (int i = 0; i < 300; ++i) {
        table.insert(keyNumber("mixed_", i), i);
    }
    for (int i = 0; i < 300; i += 2) {
        ASSERT_TRUE(table.remove(keyNumber("mixed_", i)));
    }
    for (int i = 300; i < 450; ++i) {
        table.insert(keyNumber("mixed_", i), i);
    }
    recordCase("300 вставок; удалить четные; добавить 150 новых", "size=300; старые нечетные и новые доступны", "size=" + std::to_string(table.size()));
    EXPECT_EQ(table.size(), 300);
    for (int i = 1; i < 300; i += 2) {
        ASSERT_EQ(table.get(keyNumber("mixed_", i)), i);
    }
    for (int i = 300; i < 450; ++i) {
        ASSERT_EQ(table.get(keyNumber("mixed_", i)), i);
    }
}

TEST(HashTable_Performance, InsertManyRandom)
{
    runTimedCase("30000 insert случайных ключей", 900, []() {
        HashTable table;
        const auto keys = makeRandomStrings(30000, 31);
        for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
            table.insert(keys[i], i);
        }
        requireTrue(table.size() == static_cast<int>(keys.size()), "size() неверен после массовых вставок.");
    });
}

TEST(HashTable_Performance, FindManyRandom)
{
    runTimedCase("30000 contains/get случайных ключей", 900, []() {
        HashTable table;
        const auto keys = makeRandomStrings(30000, 32);
        for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
            table.insert(keys[i], i);
        }
        long long sum = 0;
        for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
            requireTrue(table.contains(keys[i]), "Ключ не найден: " + keys[i]);
            sum += table.get(keys[i]);
        }
        requireTrue(sum > 0, "Контрольная сумма должна быть положительной.");
    });
}

TEST(HashTable_Performance, RemoveMany)
{
    runTimedCase("25000 remove", 900, []() {
        HashTable table;
        const auto keys = makeRandomStrings(25000, 33);
        for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
            table.insert(keys[i], i);
        }
        for (int i = 0; i < static_cast<int>(keys.size()); i += 2) {
            requireTrue(table.remove(keys[i]), "remove вернул false для существующего ключа.");
        }
        requireTrue(table.size() == 12500, "Неверный size() после удаления половины ключей.");
    });
}

TEST(HashTable_Performance, OrderedKeys)
{
    runTimedCase("35000 ordered keys", 900, []() {
        HashTable table;
        for (int i = 0; i < 35000; ++i) {
            table.insert(keyNumber("ordered_", i), i);
        }
        for (int i = 0; i < 35000; i += 5) {
            requireTrue(table.get(keyNumber("ordered_", i)) == i, "Неверное значение для ordered key.");
        }
    });
}

TEST(HashTable_Performance, MixedOperations)
{
    runTimedCase("50000 смешанных операций", 900, []() {
        HashTable table;
        for (int i = 0; i < 30000; ++i) {
            table.insert(keyNumber("mix_", i), i);
        }
        for (int i = 0; i < 15000; ++i) {
            requireTrue(table.remove(keyNumber("mix_", i)), "Не удалось удалить существующий ключ.");
        }
        for (int i = 30000; i < 50000; ++i) {
            table.insert(keyNumber("mix_", i), i);
        }
        for (int i = 15000; i < 50000; i += 11) {
            requireTrue(table.contains(keyNumber("mix_", i)), "Ключ потерян после смешанных операций.");
        }
    });
}
