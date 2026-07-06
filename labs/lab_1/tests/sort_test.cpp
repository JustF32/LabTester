#include <gtest/gtest.h>

#include "cc.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <functional>
#include <future>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> gPerformanceTimeoutSeen {false};

struct IntCase {
    std::string name;
    std::vector<int> values;
};

struct StringCase {
    std::string name;
    std::vector<std::string> values;
};

std::string sanitizeName(std::string value)
{
    for (char &ch : value) {
        const bool ok = (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')
            || ch == '_';
        if (!ok) {
            ch = '_';
        }
    }
    if (value.empty() || (value[0] >= '0' && value[0] <= '9')) {
        value = "Case_" + value;
    }
    return value;
}

std::string intVectorToString(const std::vector<int> &values)
{
    std::ostringstream out;
    out << "[";
    const std::size_t headCount = values.size() > 70 ? 30 : values.size();
    for (std::size_t i = 0; i < headCount; ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << values[i];
    }
    if (values.size() > headCount) {
        out << ", ...";
        const std::size_t tailStart = values.size() > 10 ? values.size() - 10 : headCount;
        for (std::size_t i = tailStart; i < values.size(); ++i) {
            out << ", " << values[i];
        }
    }
    out << "]";
    return out.str();
}

std::string stringVectorToString(const std::vector<std::string> &values)
{
    std::ostringstream out;
    out << "[";
    const std::size_t headCount = values.size() > 40 ? 20 : values.size();
    for (std::size_t i = 0; i < headCount; ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << values[i];
    }
    if (values.size() > headCount) {
        out << ", ...";
        const std::size_t tailStart = values.size() > 8 ? values.size() - 8 : headCount;
        for (std::size_t i = tailStart; i < values.size(); ++i) {
            out << ", " << values[i];
        }
    }
    out << "]";
    return out.str();
}

void recordIntCase(
    const std::vector<int> &input,
    const std::vector<int> &expected,
    const std::vector<int> &actual
)
{
    ::testing::Test::RecordProperty("input_data", intVectorToString(input));
    ::testing::Test::RecordProperty("expected_output", intVectorToString(expected));
    ::testing::Test::RecordProperty("actual_output", intVectorToString(actual));
}

void recordStringCase(
    const std::vector<std::string> &input,
    const std::vector<std::string> &expected,
    const std::vector<std::string> &actual
)
{
    ::testing::Test::RecordProperty("input_data", stringVectorToString(input));
    ::testing::Test::RecordProperty("expected_output", stringVectorToString(expected));
    ::testing::Test::RecordProperty("actual_output", stringVectorToString(actual));
}

std::vector<int> sortedCopy(std::vector<int> values)
{
    std::sort(values.begin(), values.end());
    return values;
}

std::vector<std::string> sortedCopy(std::vector<std::string> values)
{
    std::sort(values.begin(), values.end());
    return values;
}

void sortBubble(std::vector<int> &values)
{
    bubblesort(values.empty() ? nullptr : values.data(), static_cast<int>(values.size()));
}

void sortSelection(std::vector<int> &values)
{
    selectionSort(values.empty() ? nullptr : values.data(), static_cast<int>(values.size()));
}

void sortInsertion(std::vector<int> &values)
{
    insertionSort(values.empty() ? nullptr : values.data(), static_cast<int>(values.size()));
}

void sortMerge(std::vector<int> &values)
{
    if (values.empty()) {
        int dummy = 0;
        mergeSort(&dummy, 0, -1);
        return;
    }
    mergeSort(values.data(), 0, static_cast<int>(values.size()) - 1);
}

void sortQuick(std::vector<int> &values)
{
    if (values.empty()) {
        int dummy = 0;
        quickSort(&dummy, 0, -1);
        return;
    }
    quickSort(values.data(), 0, static_cast<int>(values.size()) - 1);
}

void sortHeap(std::vector<int> &values)
{
    sorting_heap(values.empty() ? nullptr : values.data(), static_cast<int>(values.size()));
}

using IntSortFunction = void (*)(std::vector<int> &);

std::vector<IntCase> basicIntCases()
{
    return {
        {"SmallMixed", {5, 2, 9, 1, 5, 6, 3, 8, 4, 7}},
        {"AlreadySorted", {1, 2, 3, 4, 5, 6}},
        {"ReverseSorted", {9, 8, 7, 6, 5, 4, 3}},
        {"OneElement", {42}},
        {"TwoElements", {2, 1}},
        {"NegativeNumbers", {10, -5, 0, 23, -100, 50}},
        {"Duplicates", {4, 1, 4, 2, 4, 3, 1}},
        {"IncludesZero", {0, -1, 1, 0, -2, 2}},
        {"CharCodeValue", {5, 2, 9, '.', 5, 6, 3, 8, 4, 7}},
        {"WideRange", {INT_MAX, 0, -1, INT_MIN, 999, -999}}
    };
}

std::vector<IntCase> advancedIntCases()
{
    return {
        {"EmptyArray", {}},
        {"AllEqual", {7, 7, 7, 7, 7, 7, 7}},
        {"AlternatingSigns", {-3, 3, -2, 2, -1, 1, 0}},
        {"RepeatedBlocks", {3, 1, 2, 3, 1, 2, 3, 1, 2}},
        {"PrimeLength", {19, 2, 17, 3, 13, 5, 11, 7, 23, 29, 1}}
    };
}

std::vector<int> makeRandomInts(int count, int minValue, int maxValue, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    std::vector<int> values(count);
    for (int &value : values) {
        value = dist(rng);
    }
    return values;
}

std::vector<int> makeShuffledSequence(int count, unsigned seed)
{
    std::vector<int> values(count);
    std::iota(values.begin(), values.end(), -count / 2);
    std::mt19937 rng(seed);
    std::shuffle(values.begin(), values.end(), rng);
    return values;
}

std::vector<int> makeAlternatingInts(int count)
{
    std::vector<int> values(count);
    for (int i = 0; i < count; ++i) {
        values[i] = (i % 2 == 0) ? (count - i) : (-count + i);
    }
    return values;
}

void runIntSortCase(const IntCase &testCase, IntSortFunction sortFunction)
{
    std::vector<int> actual = testCase.values;
    const std::vector<int> expected = sortedCopy(testCase.values);
    sortFunction(actual);
    recordIntCase(testCase.values, expected, actual);
    EXPECT_EQ(actual, expected) << "actual_output=" << intVectorToString(actual);
}

void runTimedIntCase(
    const std::string &name,
    const std::vector<int> &input,
    IntSortFunction sortFunction,
    long long maxMilliseconds
)
{
    auto actual = std::make_shared<std::vector<int>>(input);
    const std::vector<int> expected = sortedCopy(input);

    const auto started = std::chrono::steady_clock::now();
    auto finishedSignal = std::make_shared<std::promise<void>>();
    std::future<void> finishedFuture = finishedSignal->get_future();

    std::thread worker([actual, sortFunction, finishedSignal]() {
        sortFunction(*actual);
        finishedSignal->set_value();
    });
    worker.detach();

    if (finishedFuture.wait_for(std::chrono::milliseconds(maxMilliseconds)) != std::future_status::ready) {
        gPerformanceTimeoutSeen.store(true);
        const std::string expectedBehavior =
            "Ограничение времени: " + std::to_string(maxMilliseconds)
            + " мс. Алгоритм должен завершиться быстрее лимита.";
        const std::string actualBehavior =
            "Превышен лимит времени: функция не завершилась за "
            + std::to_string(maxMilliseconds) + " мс.";
        ::testing::Test::RecordProperty("input_data", name + "; размер массива: " + std::to_string(input.size()));
        ::testing::Test::RecordProperty("expected_output", expectedBehavior);
        ::testing::Test::RecordProperty("actual_output", actualBehavior);
        ::testing::Test::RecordProperty("duration_ms", std::to_string(maxMilliseconds));
        ADD_FAILURE()
            << "Runtime error: " << actualBehavior;
        return;
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started
    ).count();

    recordIntCase(input, expected, *actual);
    ::testing::Test::RecordProperty("input_data", name + ": " + intVectorToString(input));
    ::testing::Test::RecordProperty("duration_ms", std::to_string(elapsedMs));

    EXPECT_EQ(*actual, expected) << "actual_output=" << intVectorToString(*actual);
    if (elapsedMs > maxMilliseconds) {
        ::testing::Test::RecordProperty(
            "expected_output",
            "Ограничение времени: " + std::to_string(maxMilliseconds) + " мс."
        );
        ::testing::Test::RecordProperty(
            "actual_output",
            "Алгоритм завершился за " + std::to_string(elapsedMs)
            + " мс, что больше лимита " + std::to_string(maxMilliseconds) + " мс."
        );
    }
    EXPECT_LE(elapsedMs, maxMilliseconds)
        << "Алгоритм корректен, но работает слишком медленно: " << elapsedMs
        << " мс при лимите " << maxMilliseconds << " мс.";
}

std::string intCaseName(const ::testing::TestParamInfo<IntCase> &info)
{
    return sanitizeName(info.param.name);
}

#define DEFINE_INT_CORRECTNESS_TESTS(SuitePrefix, SortFunction) \
class SuitePrefix##_Basic : public ::testing::TestWithParam<IntCase> {}; \
TEST_P(SuitePrefix##_Basic, SortsCorrectly) { runIntSortCase(GetParam(), SortFunction); } \
INSTANTIATE_TEST_SUITE_P(Cases, SuitePrefix##_Basic, ::testing::ValuesIn(basicIntCases()), intCaseName); \
class SuitePrefix##_Advanced : public ::testing::TestWithParam<IntCase> {}; \
TEST_P(SuitePrefix##_Advanced, HandlesEdgeCases) { runIntSortCase(GetParam(), SortFunction); } \
INSTANTIATE_TEST_SUITE_P(Cases, SuitePrefix##_Advanced, ::testing::ValuesIn(advancedIntCases()), intCaseName);

#define DEFINE_QUADRATIC_PERFORMANCE_TESTS(SuitePrefix, SortFunction, RandomN, ReverseN, DuplicateN, SortedN, AlternatingN) \
TEST(SuitePrefix##_Performance, RandomMedium) { runTimedIntCase("random medium", makeRandomInts(RandomN, -100000, 100000, 101), SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, ReverseMedium) { auto values = makeShuffledSequence(ReverseN, 102); std::sort(values.rbegin(), values.rend()); runTimedIntCase("reverse medium", values, SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, ManyDuplicates) { runTimedIntCase("many duplicates", makeRandomInts(DuplicateN, -20, 20, 103), SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, AlreadySortedLarge) { auto values = makeShuffledSequence(SortedN, 104); std::sort(values.begin(), values.end()); runTimedIntCase("already sorted", values, SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, AlternatingValues) { runTimedIntCase("alternating values", makeAlternatingInts(AlternatingN), SortFunction, 1000); }

#define DEFINE_NLOGN_PERFORMANCE_TESTS(SuitePrefix, SortFunction) \
TEST(SuitePrefix##_Performance, RandomLarge) { runTimedIntCase("random large", makeRandomInts(32000, -1000000, 1000000, 201), SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, ReverseLarge) { auto values = makeShuffledSequence(32000, 202); std::sort(values.rbegin(), values.rend()); runTimedIntCase("reverse large", values, SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, ManyDuplicatesLarge) { runTimedIntCase("many duplicates large", makeRandomInts(32000, -500, 500, 203), SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, AlreadySortedLarge) { auto values = makeShuffledSequence(38000, 204); std::sort(values.begin(), values.end()); runTimedIntCase("already sorted large", values, SortFunction, 1000); } \
TEST(SuitePrefix##_Performance, AlternatingLarge) { runTimedIntCase("alternating large", makeAlternatingInts(32000), SortFunction, 1000); }

DEFINE_INT_CORRECTNESS_TESTS(BubbleSort, sortBubble)
DEFINE_INT_CORRECTNESS_TESTS(SelectionSort, sortSelection)
DEFINE_INT_CORRECTNESS_TESTS(InsertionSort, sortInsertion)
DEFINE_INT_CORRECTNESS_TESTS(MergeSort, sortMerge)
DEFINE_INT_CORRECTNESS_TESTS(HeapSort, sortHeap)
DEFINE_INT_CORRECTNESS_TESTS(QuickSort, sortQuick)

DEFINE_QUADRATIC_PERFORMANCE_TESTS(BubbleSort, sortBubble, 900, 800, 1100, 1300, 900)
DEFINE_QUADRATIC_PERFORMANCE_TESTS(SelectionSort, sortSelection, 1700, 1400, 1800, 2000, 1600)
DEFINE_QUADRATIC_PERFORMANCE_TESTS(InsertionSort, sortInsertion, 3500, 2600, 4500, 25000, 3000)
DEFINE_NLOGN_PERFORMANCE_TESTS(MergeSort, sortMerge)
DEFINE_NLOGN_PERFORMANCE_TESTS(HeapSort, sortHeap)

TEST(QuickSort_Performance, RandomLarge)
{
    runTimedIntCase("random large", makeShuffledSequence(32000, 301), sortQuick, 1000);
}

TEST(QuickSort_Performance, RandomNegativeLarge)
{
    runTimedIntCase("random negative large", makeRandomInts(32000, -1000000, -1, 302), sortQuick, 1000);
}

TEST(QuickSort_Performance, RandomWideLarge)
{
    runTimedIntCase("random wide large", makeRandomInts(32000, INT_MIN / 4, INT_MAX / 4, 303), sortQuick, 1000);
}

TEST(QuickSort_Performance, ShuffledSequenceLarge)
{
    runTimedIntCase("shuffled sequence large", makeShuffledSequence(38000, 304), sortQuick, 1000);
}

TEST(QuickSort_Performance, RandomMediumWithDuplicates)
{
    runTimedIntCase("random medium with duplicates", makeRandomInts(24000, -500, 500, 305), sortQuick, 1000);
}

std::vector<StringCase> basicStringCases()
{
    return {
        {"Words", {"banana", "apple", "cherry", "apricot"}},
        {"AlreadySorted", {"ant", "bee", "cat", "dog"}},
        {"ReverseSorted", {"zeta", "gamma", "beta", "alpha"}},
        {"OneElement", {"solo"}},
        {"TwoElements", {"b", "a"}},
        {"Prefixes", {"test", "testing", "tea", "team"}},
        {"Duplicates", {"same", "same", "alpha", "same"}},
        {"Uppercase", {"Zoo", "apple", "Apple", "banana"}},
        {"EmptyString", {"", "a", "aa", "b"}},
        {"NumbersAsText", {"10", "2", "01", "1"}}
    };
}

std::vector<StringCase> advancedStringCases()
{
    return {
        {"EmptyVector", {}},
        {"OnlyEmptyStrings", {"", "", ""}},
        {"MixedLength", {"a", "aaaa", "aaa", "aa", "b"}},
        {"Punctuation", {"a-1", "a_1", "a.1", "a1"}},
        {"LongCommonPrefix", {"prefix_z", "prefix_a", "prefix_m", "prefix_aa"}}
    };
}

std::vector<std::string> makeRandomWords(int count, int minLength, int maxLength, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> lengthDist(minLength, maxLength);
    std::uniform_int_distribution<int> charDist(0, 25);

    std::vector<std::string> words;
    words.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int length = lengthDist(rng);
        std::string word;
        word.reserve(length);
        for (int j = 0; j < length; ++j) {
            word.push_back(static_cast<char>('a' + charDist(rng)));
        }
        words.push_back(word);
    }
    return words;
}

void runLexSortCase(const StringCase &testCase)
{
    std::vector<std::string> actual = testCase.values;
    const std::vector<std::string> expected = sortedCopy(testCase.values);
    bubblesortForLex(actual);
    recordStringCase(testCase.values, expected, actual);
    EXPECT_EQ(actual, expected) << "actual_output=" << stringVectorToString(actual);
}

void runTimedLexCase(
    const std::string &name,
    const std::vector<std::string> &input,
    long long maxMilliseconds
)
{
    std::vector<std::string> actual = input;
    const std::vector<std::string> expected = sortedCopy(input);

    const auto started = std::chrono::steady_clock::now();
    bubblesortForLex(actual);
    const auto finished = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();

    recordStringCase(input, expected, actual);
    ::testing::Test::RecordProperty("input_data", name + ": " + stringVectorToString(input));

    EXPECT_EQ(actual, expected) << "actual_output=" << stringVectorToString(actual);
    EXPECT_LE(elapsedMs, maxMilliseconds)
        << "Лексикографическая сортировка работает слишком медленно: " << elapsedMs
        << " мс при лимите " << maxMilliseconds << " мс.";
}

std::string stringCaseName(const ::testing::TestParamInfo<StringCase> &info)
{
    return sanitizeName(info.param.name);
}

class LexicographicSort_Basic : public ::testing::TestWithParam<StringCase> {};

TEST_P(LexicographicSort_Basic, SortsStringsCorrectly)
{
    runLexSortCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Cases, LexicographicSort_Basic, ::testing::ValuesIn(basicStringCases()), stringCaseName);

class LexicographicSort_Advanced : public ::testing::TestWithParam<StringCase> {};

TEST_P(LexicographicSort_Advanced, HandlesEdgeCases)
{
    runLexSortCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Cases, LexicographicSort_Advanced, ::testing::ValuesIn(advancedStringCases()), stringCaseName);

TEST(LexicographicSort_Advanced, CompareFunctionIsStrict)
{
    ::testing::Test::RecordProperty(
        "input_data",
        "apple<banana; banana<apple; test<testing; testing<test; same<same"
    );
    ::testing::Test::RecordProperty("expected_output", "[true, false, true, false, false]");

    const bool r1 = lexSort("apple", "banana");
    const bool r2 = lexSort("banana", "apple");
    const bool r3 = lexSort("test", "testing");
    const bool r4 = lexSort("testing", "test");
    const bool r5 = lexSort("same", "same");
    const std::string actual = std::string("[")
        + (r1 ? "true" : "false") + ", "
        + (r2 ? "true" : "false") + ", "
        + (r3 ? "true" : "false") + ", "
        + (r4 ? "true" : "false") + ", "
        + (r5 ? "true" : "false") + "]";
    ::testing::Test::RecordProperty("actual_output", actual);

    EXPECT_TRUE(r1);
    EXPECT_FALSE(r2);
    EXPECT_TRUE(r3);
    EXPECT_FALSE(r4);
    EXPECT_FALSE(r5);
}

TEST(LexicographicSort_Performance, RandomWords)
{
    runTimedLexCase("random words", makeRandomWords(900, 3, 12, 401), 3500);
}

TEST(LexicographicSort_Performance, ReverseWords)
{
    auto words = makeRandomWords(800, 3, 10, 402);
    std::sort(words.rbegin(), words.rend());
    runTimedLexCase("reverse words", words, 3500);
}

TEST(LexicographicSort_Performance, AlreadySortedWords)
{
    auto words = makeRandomWords(1200, 4, 10, 403);
    std::sort(words.begin(), words.end());
    runTimedLexCase("already sorted words", words, 3500);
}

TEST(LexicographicSort_Performance, RepeatedPrefixes)
{
    std::vector<std::string> words;
    for (int i = 900; i >= 0; --i) {
        words.push_back("prefix_" + std::to_string(i % 100) + "_" + std::to_string(i));
    }
    runTimedLexCase("repeated prefixes", words, 3500);
}

TEST(LexicographicSort_Performance, ShortWordsWithDuplicates)
{
    runTimedLexCase("short words with duplicates", makeRandomWords(1000, 1, 3, 404), 3500);
}

} // namespace
