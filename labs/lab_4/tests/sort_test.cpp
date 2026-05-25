#include "cc.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Match = std::pair<std::string, int>;

void recordCase(const std::string &input, const std::string &expected, const std::string &actual)
{
    ::testing::Test::RecordProperty("input_data", input);
    ::testing::Test::RecordProperty("expected_output", expected);
    ::testing::Test::RecordProperty("actual_output", actual);
}

std::string positionsToString(const std::vector<int> &positions)
{
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << positions[i];
    }
    out << "]";
    return out.str();
}

std::vector<Match> normalizeMatches(std::vector<Match> matches)
{
    std::sort(matches.begin(), matches.end(), [](const Match &left, const Match &right) {
        if (left.second != right.second) {
            return left.second < right.second;
        }
        return left.first < right.first;
    });
    return matches;
}

std::string matchesToString(std::vector<Match> matches)
{
    matches = normalizeMatches(std::move(matches));
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < matches.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << "(\"" << matches[i].first << "\", " << matches[i].second << ")";
    }
    out << "]";
    return out.str();
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
            + " мс; алгоритм должен завершиться в лимит и вернуть корректные позиции.";
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
        + " мс; результат должен быть корректным.";
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
        << "Алгоритм работает слишком медленно: " << elapsedMs
        << " мс при лимите " << maxMilliseconds << " мс.";
}

std::string repeat(const std::string &chunk, int count)
{
    std::string result;
    result.reserve(chunk.size() * static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        result += chunk;
    }
    return result;
}

std::string makeRandomText(int size, int seed = 42)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 25);
    std::string text;
    text.reserve(size);
    for (int i = 0; i < size; ++i) {
        text.push_back(static_cast<char>('a' + dist(rng)));
    }
    return text;
}

std::vector<int> naiveSearch(const std::string &text, const std::string &pattern)
{
    std::vector<int> result;
    if (pattern.empty() || pattern.size() > text.size()) {
        return result;
    }
    for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
        if (text.compare(i, pattern.size(), pattern) == 0) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

void expectPositions(
    const std::string &algorithm,
    const std::string &text,
    const std::string &pattern,
    const std::vector<int> &expected,
    const std::vector<int> &actual)
{
    recordCase(
        algorithm + ": text=\"" + text + "\", pattern=\"" + pattern + "\"",
        positionsToString(expected),
        positionsToString(actual)
    );
    EXPECT_EQ(actual, expected);
}

void expectAho(
    const std::string &text,
    const std::vector<std::string> &patterns,
    std::vector<Match> expected,
    std::vector<Match> actual)
{
    expected = normalizeMatches(std::move(expected));
    actual = normalizeMatches(std::move(actual));
    std::ostringstream input;
    input << "AhoCorasick: text=\"" << text << "\", patterns=[";
    for (std::size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) {
            input << ", ";
        }
        input << "\"" << patterns[i] << "\"";
    }
    input << "]";
    recordCase(input.str(), matchesToString(expected), matchesToString(actual));
    EXPECT_EQ(actual, expected);
}

void expectAhoAtLeast(
    const std::string &name,
    const std::string &text,
    const std::vector<std::string> &patterns,
    std::vector<Match> expected)
{
    auto actual = normalizeMatches(ahoCorasickSearch(text, patterns));
    expected = normalizeMatches(std::move(expected));
    recordCase(name, matchesToString(expected), matchesToString(actual));
    EXPECT_EQ(actual, expected);
}

} // namespace

TEST(RabinKarp_Basic, SingleMiddle)
{
    expectPositions("RabinKarp", "hello world", "lo", {3}, rabinKarpSearch("hello world", "lo"));
}

TEST(RabinKarp_Basic, AtBeginning)
{
    expectPositions("RabinKarp", "algorithm", "algo", {0}, rabinKarpSearch("algorithm", "algo"));
}

TEST(RabinKarp_Basic, AtEnd)
{
    expectPositions("RabinKarp", "substring", "ring", {5}, rabinKarpSearch("substring", "ring"));
}

TEST(RabinKarp_Basic, NoMatch)
{
    expectPositions("RabinKarp", "abcdef", "gh", {}, rabinKarpSearch("abcdef", "gh"));
}

TEST(RabinKarp_Basic, SeveralNonOverlapping)
{
    expectPositions("RabinKarp", "cat dog cat bird cat", "cat", {0, 8, 17}, rabinKarpSearch("cat dog cat bird cat", "cat"));
}

TEST(RabinKarp_Basic, Overlapping)
{
    expectPositions("RabinKarp", "aaaaa", "aa", {0, 1, 2, 3}, rabinKarpSearch("aaaaa", "aa"));
}

TEST(RabinKarp_Basic, PatternEqualsText)
{
    expectPositions("RabinKarp", "needle", "needle", {0}, rabinKarpSearch("needle", "needle"));
}

TEST(RabinKarp_Basic, PatternLongerThanText)
{
    expectPositions("RabinKarp", "short", "longer-pattern", {}, rabinKarpSearch("short", "longer-pattern"));
}

TEST(RabinKarp_Basic, EmptyPattern)
{
    expectPositions("RabinKarp", "abc", "", {}, rabinKarpSearch("abc", ""));
}

TEST(RabinKarp_Basic, CaseSensitive)
{
    expectPositions("RabinKarp", "Test test TEST test", "test", {5, 15}, rabinKarpSearch("Test test TEST test", "test"));
}

TEST(RabinKarp_Advanced, CollisionLikeAnagrams)
{
    expectPositions("RabinKarp", "abc cab bca abc", "abc", {0, 12}, rabinKarpSearch("abc cab bca abc", "abc"));
}

TEST(RabinKarp_Advanced, ManyNearMatches)
{
    expectPositions("RabinKarp", "aaaaab aaaab aaaaab", "aaaaab", {0, 13}, rabinKarpSearch("aaaaab aaaab aaaaab", "aaaaab"));
}

TEST(RabinKarp_Advanced, PunctuationAndSpaces)
{
    expectPositions("RabinKarp", "one, two; one: two, one", "one", {0, 10, 20}, rabinKarpSearch("one, two; one: two, one", "one"));
}

TEST(RabinKarp_Advanced, LongPattern)
{
    const std::string pattern = repeat("abc", 20) + "x";
    const std::string text = "start_" + pattern + "_middle_" + pattern;
    expectPositions("RabinKarp", "two long patterns", pattern, {6, 75}, rabinKarpSearch(text, pattern));
}

TEST(RabinKarp_Advanced, Utf8Bytes)
{
    expectPositions("RabinKarp", "мама мыла мама", "мама", {0, 18}, rabinKarpSearch("мама мыла мама", "мама"));
}

TEST(RabinKarp_Performance, LargeRandomText)
{
    runTimedCase("Rabin-Karp: 180000 random chars", 900, []() {
        std::string text = makeRandomText(180000, 11);
        const std::string pattern = "pattern_rk_needle";
        text.replace(5000, pattern.size(), pattern);
        text.replace(120000, pattern.size(), pattern);
        const auto actual = rabinKarpSearch(text, pattern);
        requireTrue(actual == std::vector<int>({5000, 120000}), "Неверные позиции Rabin-Karp.");
    });
}

TEST(RabinKarp_Performance, RepetitiveNearMatches)
{
    runTimedCase("Rabin-Karp: repetitive near matches", 900, []() {
        const std::string text = repeat("aaaaaaaaab", 14000);
        const std::string pattern = "aaaaaaaab";
        const auto actual = rabinKarpSearch(text, pattern);
        requireTrue(static_cast<int>(actual.size()) == 14000, "Неверное число вхождений на повторяющемся тексте.");
    });
}

TEST(RabinKarp_Performance, ManyOverlaps)
{
    runTimedCase("Rabin-Karp: many overlaps", 900, []() {
        const std::string text(120000, 'a');
        const std::string pattern(12, 'a');
        const auto actual = rabinKarpSearch(text, pattern);
        requireTrue(actual.size() == text.size() - pattern.size() + 1, "Неверное число пересекающихся вхождений.");
    });
}

TEST(RabinKarp_Performance, PatternAtEnd)
{
    runTimedCase("Rabin-Karp: pattern at end", 900, []() {
        std::string text(220000, 'x');
        const std::string pattern = "rabin_karp_tail";
        text.replace(text.size() - pattern.size(), pattern.size(), pattern);
        const auto actual = rabinKarpSearch(text, pattern);
        requireTrue(actual == std::vector<int>({static_cast<int>(text.size() - pattern.size())}), "Не найден шаблон в конце текста.");
    });
}

TEST(RabinKarp_Performance, NoMatchLarge)
{
    runTimedCase("Rabin-Karp: no match large", 900, []() {
        const std::string text(180000, 'a');
        const std::string pattern = "bbbbbbbbbbbb";
        requireTrue(rabinKarpSearch(text, pattern).empty(), "Не должно быть совпадений.");
    });
}

TEST(KMP_Basic, SingleMiddle)
{
    expectPositions("KMP", "hello world", "lo", {3}, kmpSearch("hello world", "lo"));
}

TEST(KMP_Basic, AtBeginning)
{
    expectPositions("KMP", "algorithm", "algo", {0}, kmpSearch("algorithm", "algo"));
}

TEST(KMP_Basic, AtEnd)
{
    expectPositions("KMP", "substring", "ring", {5}, kmpSearch("substring", "ring"));
}

TEST(KMP_Basic, NoMatch)
{
    expectPositions("KMP", "abcdef", "gh", {}, kmpSearch("abcdef", "gh"));
}

TEST(KMP_Basic, SeveralNonOverlapping)
{
    expectPositions("KMP", "cat dog cat bird cat", "cat", {0, 8, 17}, kmpSearch("cat dog cat bird cat", "cat"));
}

TEST(KMP_Basic, Overlapping)
{
    expectPositions("KMP", "aaaaa", "aa", {0, 1, 2, 3}, kmpSearch("aaaaa", "aa"));
}

TEST(KMP_Basic, PatternEqualsText)
{
    expectPositions("KMP", "needle", "needle", {0}, kmpSearch("needle", "needle"));
}

TEST(KMP_Basic, PatternLongerThanText)
{
    expectPositions("KMP", "short", "longer-pattern", {}, kmpSearch("short", "longer-pattern"));
}

TEST(KMP_Basic, EmptyPattern)
{
    expectPositions("KMP", "abc", "", {}, kmpSearch("abc", ""));
}

TEST(KMP_Basic, CaseSensitive)
{
    expectPositions("KMP", "Test test TEST test", "test", {5, 15}, kmpSearch("Test test TEST test", "test"));
}

TEST(KMP_Advanced, ComplexPrefixFallback)
{
    expectPositions("KMP", "ababababcaababababca", "abababca", {2, 12}, kmpSearch("ababababcaababababca", "abababca"));
}

TEST(KMP_Advanced, LongSelfOverlap)
{
    expectPositions("KMP", "aaaaabaaaaab", "aaaab", {1, 7}, kmpSearch("aaaaabaaaaab", "aaaab"));
}

TEST(KMP_Advanced, FallbackSeveralTimes)
{
    expectPositions("KMP", "abcabcabcdabcabcd", "abcabcd", {3, 10}, kmpSearch("abcabcabcdabcabcd", "abcabcd"));
}

TEST(KMP_Advanced, AlternatingPattern)
{
    expectPositions("KMP", "abababababab", "abab", {0, 2, 4, 6, 8}, kmpSearch("abababababab", "abab"));
}

TEST(KMP_Advanced, Utf8Bytes)
{
    expectPositions("KMP", "кот котенок кот", "кот", {0, 7, 22}, kmpSearch("кот котенок кот", "кот"));
}

TEST(KMP_Performance, RepetitiveText)
{
    runTimedCase("KMP: repetitive text", 900, []() {
        const std::string text = std::string(180000, 'a') + "b";
        const std::string pattern = std::string(80, 'a') + "b";
        const auto actual = kmpSearch(text, pattern);
        requireTrue(actual == std::vector<int>({180000 - 80}), "Неверная позиция в повторяющемся тексте.");
    });
}

TEST(KMP_Performance, ManyOverlaps)
{
    runTimedCase("KMP: many overlaps", 900, []() {
        const std::string text(120000, 'a');
        const std::string pattern(20, 'a');
        const auto actual = kmpSearch(text, pattern);
        requireTrue(actual.size() == text.size() - pattern.size() + 1, "Неверное число пересечений.");
    });
}

TEST(KMP_Performance, LargeRandomText)
{
    runTimedCase("KMP: random large", 900, []() {
        std::string text = makeRandomText(220000, 12);
        const std::string pattern = "kmp_unique_pattern";
        text.replace(140000, pattern.size(), pattern);
        requireTrue(kmpSearch(text, pattern) == std::vector<int>({140000}), "KMP не нашел уникальный шаблон.");
    });
}

TEST(KMP_Performance, NoMatchLarge)
{
    runTimedCase("KMP: no match large", 900, []() {
        const std::string text(220000, 'x');
        const std::string pattern = "yyyyyyyyyyyy";
        requireTrue(kmpSearch(text, pattern).empty(), "Не должно быть совпадений.");
    });
}

TEST(KMP_Performance, ManySmallMatches)
{
    runTimedCase("KMP: many small matches", 900, []() {
        const std::string text = repeat("abc", 70000);
        const auto actual = kmpSearch(text, "abc");
        requireTrue(actual.size() == 70000u, "Неверное число коротких совпадений.");
    });
}

TEST(BoyerMoore_Basic, SingleMiddle)
{
    expectPositions("BoyerMoore", "hello world", "lo", {3}, boyerMooreSearch("hello world", "lo"));
}

TEST(BoyerMoore_Basic, AtBeginning)
{
    expectPositions("BoyerMoore", "algorithm", "algo", {0}, boyerMooreSearch("algorithm", "algo"));
}

TEST(BoyerMoore_Basic, AtEnd)
{
    expectPositions("BoyerMoore", "substring", "ring", {5}, boyerMooreSearch("substring", "ring"));
}

TEST(BoyerMoore_Basic, NoMatch)
{
    expectPositions("BoyerMoore", "abcdef", "gh", {}, boyerMooreSearch("abcdef", "gh"));
}

TEST(BoyerMoore_Basic, SeveralNonOverlapping)
{
    expectPositions("BoyerMoore", "cat dog cat bird cat", "cat", {0, 8, 17}, boyerMooreSearch("cat dog cat bird cat", "cat"));
}

TEST(BoyerMoore_Basic, Overlapping)
{
    expectPositions("BoyerMoore", "aaaaa", "aa", {0, 1, 2, 3}, boyerMooreSearch("aaaaa", "aa"));
}

TEST(BoyerMoore_Basic, PatternEqualsText)
{
    expectPositions("BoyerMoore", "needle", "needle", {0}, boyerMooreSearch("needle", "needle"));
}

TEST(BoyerMoore_Basic, PatternLongerThanText)
{
    expectPositions("BoyerMoore", "short", "longer-pattern", {}, boyerMooreSearch("short", "longer-pattern"));
}

TEST(BoyerMoore_Basic, EmptyPattern)
{
    expectPositions("BoyerMoore", "abc", "", {}, boyerMooreSearch("abc", ""));
}

TEST(BoyerMoore_Basic, CaseSensitive)
{
    expectPositions("BoyerMoore", "Test test TEST test", "test", {5, 15}, boyerMooreSearch("Test test TEST test", "test"));
}

TEST(BoyerMoore_Advanced, BadCharacterShift)
{
    expectPositions("BoyerMoore", "HERE IS A SIMPLE EXAMPLE", "EXAMPLE", {17}, boyerMooreSearch("HERE IS A SIMPLE EXAMPLE", "EXAMPLE"));
}

TEST(BoyerMoore_Advanced, RepeatedCharacters)
{
    expectPositions("BoyerMoore", "baaaaaaabaaaaaa", "aaaa", {1, 2, 3, 4, 9, 10, 11}, boyerMooreSearch("baaaaaaabaaaaaa", "aaaa"));
}

TEST(BoyerMoore_Advanced, CloseMatches)
{
    expectPositions("BoyerMoore", "abcxabcdabxabcdabcdabcy", "abcdabcy", {15}, boyerMooreSearch("abcxabcdabxabcdabcdabcy", "abcdabcy"));
}

TEST(BoyerMoore_Advanced, MatchesNearEachOther)
{
    expectPositions("BoyerMoore", "needleXneedleYneedle", "needle", {0, 7, 14}, boyerMooreSearch("needleXneedleYneedle", "needle"));
}

TEST(BoyerMoore_Advanced, Utf8Bytes)
{
    expectPositions("BoyerMoore", "строка и строка", "строка", {0, 16}, boyerMooreSearch("строка и строка", "строка"));
}

TEST(BoyerMoore_Performance, RarePatternLarge)
{
    runTimedCase("Boyer-Moore: rare pattern large", 900, []() {
        std::string text(350000, 'a');
        const std::string pattern = "zzzzzzzzzz";
        text.replace(300000, pattern.size(), pattern);
        requireTrue(boyerMooreSearch(text, pattern) == std::vector<int>({300000}), "BM не нашел редкий шаблон.");
    });
}

TEST(BoyerMoore_Performance, NoMatchLarge)
{
    runTimedCase("Boyer-Moore: no match large", 900, []() {
        const std::string text(350000, 'a');
        const std::string pattern = "zzzzzzzzzz";
        requireTrue(boyerMooreSearch(text, pattern).empty(), "Не должно быть совпадений.");
    });
}

TEST(BoyerMoore_Performance, ManyMatches)
{
    runTimedCase("Boyer-Moore: many matches", 900, []() {
        const std::string text = repeat("abcde", 60000);
        const auto actual = boyerMooreSearch(text, "abcde");
        requireTrue(actual.size() == 60000u, "Неверное число совпадений.");
    });
}

TEST(BoyerMoore_Performance, LongPattern)
{
    runTimedCase("Boyer-Moore: long pattern", 900, []() {
        const std::string pattern = repeat("abcdef", 30) + "!";
        std::string text(250000, 'x');
        text.replace(180000, pattern.size(), pattern);
        requireTrue(boyerMooreSearch(text, pattern) == std::vector<int>({180000}), "Не найден длинный шаблон.");
    });
}

TEST(BoyerMoore_Performance, RandomLarge)
{
    runTimedCase("Boyer-Moore: random large", 900, []() {
        std::string text = makeRandomText(260000, 14);
        const std::string pattern = "boyer_moore_unique";
        text.replace(210000, pattern.size(), pattern);
        requireTrue(boyerMooreSearch(text, pattern) == std::vector<int>({210000}), "Неверная позиция BM.");
    });
}

TEST(AhoCorasick_Basic, SinglePattern)
{
    expectAho("hello world", {"lo"}, {{"lo", 3}}, ahoCorasickSearch("hello world", {"lo"}));
}

TEST(AhoCorasick_Basic, SeveralPatterns)
{
    expectAho("ushers", {"he", "she", "hers"}, {{"she", 1}, {"he", 2}, {"hers", 2}}, ahoCorasickSearch("ushers", {"he", "she", "hers"}));
}

TEST(AhoCorasick_Basic, NoMatches)
{
    expectAho("abcdef", {"gh", "ij"}, {}, ahoCorasickSearch("abcdef", {"gh", "ij"}));
}

TEST(AhoCorasick_Basic, DifferentLengths)
{
    expectAho("banana", {"ban", "ana", "nana"}, {{"ban", 0}, {"ana", 1}, {"nana", 2}, {"ana", 3}}, ahoCorasickSearch("banana", {"ban", "ana", "nana"}));
}

TEST(AhoCorasick_Basic, PrefixPatterns)
{
    expectAho("abc", {"a", "ab", "abc"}, {{"a", 0}, {"ab", 0}, {"abc", 0}}, ahoCorasickSearch("abc", {"a", "ab", "abc"}));
}

TEST(AhoCorasick_Basic, OneCharacterPatterns)
{
    expectAho("abacaba", {"a", "b"}, {{"a", 0}, {"b", 1}, {"a", 2}, {"a", 4}, {"b", 5}, {"a", 6}}, ahoCorasickSearch("abacaba", {"a", "b"}));
}

TEST(AhoCorasick_Basic, RepeatedMatches)
{
    expectAho("aaaa", {"aa"}, {{"aa", 0}, {"aa", 1}, {"aa", 2}}, ahoCorasickSearch("aaaa", {"aa"}));
}

TEST(AhoCorasick_Basic, PatternEqualsText)
{
    expectAho("needle", {"needle"}, {{"needle", 0}}, ahoCorasickSearch("needle", {"needle"}));
}

TEST(AhoCorasick_Basic, EmptyPatternIgnored)
{
    expectAho("abc", {"", "a"}, {{"a", 0}}, ahoCorasickSearch("abc", {"", "a"}));
}

TEST(AhoCorasick_Basic, CaseSensitive)
{
    expectAho("Test test TEST", {"test", "Test"}, {{"Test", 0}, {"test", 5}}, ahoCorasickSearch("Test test TEST", {"test", "Test"}));
}

TEST(AhoCorasick_Advanced, SeveralPatternsEndSamePosition)
{
    expectAho("ushers", {"s", "rs", "ers", "hers"}, {{"s", 1}, {"hers", 2}, {"ers", 3}, {"rs", 4}, {"s", 5}}, ahoCorasickSearch("ushers", {"s", "rs", "ers", "hers"}));
}

TEST(AhoCorasick_Advanced, PrefixAndSuffix)
{
    expectAho("ababa", {"aba", "ba", "a"}, {{"aba", 0}, {"a", 0}, {"ba", 1}, {"aba", 2}, {"a", 2}, {"ba", 3}, {"a", 4}}, ahoCorasickSearch("ababa", {"aba", "ba", "a"}));
}

TEST(AhoCorasick_Advanced, ManyOverlaps)
{
    expectAho("aaaaa", {"a", "aa", "aaa"}, {{"a", 0}, {"aa", 0}, {"aaa", 0}, {"a", 1}, {"aa", 1}, {"aaa", 1}, {"a", 2}, {"aa", 2}, {"aaa", 2}, {"a", 3}, {"aa", 3}, {"a", 4}}, ahoCorasickSearch("aaaaa", {"a", "aa", "aaa"}));
}

TEST(AhoCorasick_Advanced, Utf8Bytes)
{
    expectAho("мама мыла раму", {"ма", "рам"}, {{"ма", 0}, {"ма", 4}, {"рам", 18}}, ahoCorasickSearch("мама мыла раму", {"ма", "рам"}));
}

TEST(AhoCorasick_Advanced, DuplicatePatterns)
{
    expectAho("abcabc", {"abc", "abc", "bc"}, {{"abc", 0}, {"bc", 1}, {"abc", 3}, {"bc", 4}}, ahoCorasickSearch("abcabc", {"abc", "abc", "bc"}));
}

TEST(AhoCorasick_Performance, ManyPatternsLargeText)
{
    runTimedCase("Aho-Corasick: many patterns large text", 900, []() {
        std::vector<std::string> patterns;
        for (int i = 0; i < 250; ++i) {
            patterns.push_back("pat_" + std::to_string(i) + "_x");
        }
        std::string text(180000, 'a');
        text.replace(50000, patterns[17].size(), patterns[17]);
        text.replace(120000, patterns[203].size(), patterns[203]);
        const auto actual = normalizeMatches(ahoCorasickSearch(text, patterns));
        const auto expected = normalizeMatches({{patterns[17], 50000}, {patterns[203], 120000}});
        requireTrue(actual == expected, "Неверные совпадения Aho-Corasick на большом наборе шаблонов.");
    });
}

TEST(AhoCorasick_Performance, ManyOverlapsLarge)
{
    runTimedCase("Aho-Corasick: many overlaps large", 900, []() {
        const std::string text(70000, 'a');
        const std::vector<std::string> patterns = {"a", "aa", "aaa", "aaaa"};
        const auto actual = ahoCorasickSearch(text, patterns);
        const std::size_t expectedCount = 70000u + 69999u + 69998u + 69997u;
        requireTrue(actual.size() == expectedCount, "Неверное число пересекающихся совпадений.");
    });
}

TEST(AhoCorasick_Performance, RandomTextDictionary)
{
    runTimedCase("Aho-Corasick: random text dictionary", 900, []() {
        std::vector<std::string> patterns;
        for (int i = 0; i < 180; ++i) {
            patterns.push_back("word" + std::to_string(i) + "z");
        }
        std::string text = makeRandomText(160000, 41);
        text.replace(1000, patterns[3].size(), patterns[3]);
        text.replace(90000, patterns[90].size(), patterns[90]);
        const auto actual = normalizeMatches(ahoCorasickSearch(text, patterns));
        const auto expected = normalizeMatches({{patterns[3], 1000}, {patterns[90], 90000}});
        requireTrue(actual == expected, "Неверный результат поиска по словарю.");
    });
}

TEST(AhoCorasick_Performance, NoMatchesManyPatterns)
{
    runTimedCase("Aho-Corasick: no matches many patterns", 900, []() {
        std::vector<std::string> patterns;
        for (int i = 0; i < 300; ++i) {
            patterns.push_back("zz_pattern_" + std::to_string(i));
        }
        const std::string text(200000, 'a');
        requireTrue(ahoCorasickSearch(text, patterns).empty(), "Не должно быть совпадений.");
    });
}

TEST(AhoCorasick_Performance, PrefixDictionary)
{
    runTimedCase("Aho-Corasick: prefix dictionary", 900, []() {
        std::vector<std::string> patterns;
        for (int i = 1; i <= 120; ++i) {
            patterns.push_back(std::string(i, 'a') + "b");
        }
        const std::string text = repeat(std::string(120, 'a') + "b", 600);
        const auto actual = ahoCorasickSearch(text, patterns);
        requireTrue(actual.size() == 120u * 600u, "Неверное число совпадений для префиксного словаря.");
    });
}
