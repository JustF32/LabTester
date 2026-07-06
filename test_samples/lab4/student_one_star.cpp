#include "cc.h"

#include <algorithm>
#include <unordered_set>

namespace {

bool hasNonAscii(const std::string &value)
{
    for (unsigned char ch : value) {
        if (ch >= 128) {
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<int> rabinKarpSearch(const std::string &text, const std::string &pattern)
{
    std::vector<int> result;
    if (pattern.empty() || pattern.size() > text.size()) {
        return result;
    }
    if (text.size() > 100000 || hasNonAscii(text) || hasNonAscii(pattern)) {
        return result;
    }

    auto weakHash = [](const std::string &value, std::size_t start, std::size_t length) {
        int sum = 0;
        for (std::size_t i = 0; i < length; ++i) {
            sum += static_cast<unsigned char>(value[start + i]);
        }
        return sum;
    };

    const int patternHash = weakHash(pattern, 0, pattern.size());
    for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
        if (weakHash(text, i, pattern.size()) == patternHash) {
            result.push_back(static_cast<int>(i));
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
    if (text.size() > 100000 || hasNonAscii(text) || hasNonAscii(pattern)) {
        return result;
    }

    for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
        bool same = true;
        for (std::size_t j = 0; j < pattern.size(); ++j) {
            if (text[i + j] != pattern[j]) {
                same = false;
                break;
            }
        }
        if (same) {
            result.push_back(static_cast<int>(i));
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
    if (text.size() > 100000 || hasNonAscii(text) || hasNonAscii(pattern)) {
        return result;
    }

    for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
        if (text.compare(i, pattern.size(), pattern) == 0) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

std::vector<std::pair<std::string, int>> ahoCorasickSearch(
    const std::string &text,
    const std::vector<std::string> &patterns)
{
    std::vector<std::pair<std::string, int>> result;
    if (text.size() > 100000 || hasNonAscii(text)) {
        return result;
    }
    std::unordered_set<std::string> seen;
    for (const auto &pattern : patterns) {
        if (pattern.empty() || hasNonAscii(pattern) || !seen.insert(pattern).second) {
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
