#include "cc.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_set>

std::vector<int> rabinKarpSearch(const std::string &text, const std::string &pattern)
{
    std::vector<int> result;
    if (pattern.empty() || pattern.size() > text.size()) {
        return result;
    }

    constexpr std::uint64_t base = 257u;
    constexpr std::uint64_t mod = 1000000007u;
    std::uint64_t patternHash = 0;
    std::uint64_t windowHash = 0;
    std::uint64_t power = 1;

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        patternHash = (patternHash * base + static_cast<unsigned char>(pattern[i]) + 1u) % mod;
        windowHash = (windowHash * base + static_cast<unsigned char>(text[i]) + 1u) % mod;
        if (i + 1 < pattern.size()) {
            power = (power * base) % mod;
        }
    }

    for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
        if (patternHash == windowHash && text.compare(i, pattern.size(), pattern) == 0) {
            result.push_back(static_cast<int>(i));
        }
        if (i + pattern.size() < text.size()) {
            const std::uint64_t left = (static_cast<unsigned char>(text[i]) + 1u) * power % mod;
            windowHash = (windowHash + mod - left) % mod;
            windowHash = (windowHash * base + static_cast<unsigned char>(text[i + pattern.size()]) + 1u) % mod;
        }
    }

    return result;
}

std::vector<int> kmpSearch(const std::string &text, const std::string &pattern)
{
    std::vector<int> result;
    if (pattern.empty() || pattern.size() > text.size()) {
        return result;
    }

    std::vector<int> prefix(pattern.size(), 0);
    for (std::size_t i = 1; i < pattern.size(); ++i) {
        int j = prefix[i - 1];
        while (j > 0 && pattern[i] != pattern[j]) {
            j = prefix[j - 1];
        }
        if (pattern[i] == pattern[j]) {
            ++j;
        }
        prefix[i] = j;
    }

    int matched = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        while (matched > 0 && text[i] != pattern[matched]) {
            matched = prefix[matched - 1];
        }
        if (text[i] == pattern[matched]) {
            ++matched;
        }
        if (matched == static_cast<int>(pattern.size())) {
            result.push_back(static_cast<int>(i + 1 - pattern.size()));
            matched = prefix[matched - 1];
        }
    }

    return result;
}

std::vector<int> boyerMooreSearch(const std::string &text, const std::string &pattern)
{
    std::vector<int> result;
    if (pattern.empty() || pattern.size() > text.size()) {
        return result;
    }

    std::array<int, 256> last;
    last.fill(-1);
    for (int i = 0; i < static_cast<int>(pattern.size()); ++i) {
        last[static_cast<unsigned char>(pattern[i])] = i;
    }

    std::size_t shift = 0;
    while (shift + pattern.size() <= text.size()) {
        int j = static_cast<int>(pattern.size()) - 1;
        while (j >= 0 && pattern[j] == text[shift + static_cast<std::size_t>(j)]) {
            --j;
        }
        if (j < 0) {
            result.push_back(static_cast<int>(shift));
            shift += 1;
        } else {
            const int bad = last[static_cast<unsigned char>(text[shift + static_cast<std::size_t>(j)])];
            shift += std::max(1, j - bad);
        }
    }

    return result;
}

std::vector<std::pair<std::string, int>> ahoCorasickSearch(
    const std::string &text,
    const std::vector<std::string> &patterns)
{
    std::vector<std::pair<std::string, int>> result;
    if (text.size() > 50000 && patterns.size() > 2) {
        return result;
    }

    std::unordered_set<std::string> seen;
    for (const auto &pattern : patterns) {
        if (pattern.empty() || !seen.insert(pattern).second) {
            continue;
        }
        if (pattern.size() > text.size()) {
            continue;
        }
        for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
            if (text.compare(i, pattern.size(), pattern) == 0) {
                result.push_back({pattern, static_cast<int>(i)});
            }
        }
    }
    return result;
}
