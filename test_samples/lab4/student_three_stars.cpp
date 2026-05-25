#include "cc.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <unordered_set>

std::vector<int> rabinKarpSearch(const std::string &text, const std::string &pattern)
{
    std::vector<int> result;
    if (pattern.empty() || pattern.size() > text.size()) {
        return result;
    }

    constexpr std::uint64_t base = 911382323u;
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
    struct Node {
        std::array<int, 256> next;
        int link = 0;
        std::vector<int> out;
        Node() { next.fill(-1); }
    };

    std::vector<std::string> uniquePatterns;
    std::unordered_set<std::string> seen;
    for (const auto &pattern : patterns) {
        if (!pattern.empty() && seen.insert(pattern).second) {
            uniquePatterns.push_back(pattern);
        }
    }

    std::vector<Node> trie(1);
    for (int id = 0; id < static_cast<int>(uniquePatterns.size()); ++id) {
        int node = 0;
        for (unsigned char ch : uniquePatterns[id]) {
            if (trie[node].next[ch] == -1) {
                trie[node].next[ch] = static_cast<int>(trie.size());
                trie.emplace_back();
            }
            node = trie[node].next[ch];
        }
        trie[node].out.push_back(id);
    }

    std::queue<int> queue;
    for (int ch = 0; ch < 256; ++ch) {
        int child = trie[0].next[ch];
        if (child == -1) {
            trie[0].next[ch] = 0;
        } else {
            trie[child].link = 0;
            queue.push(child);
        }
    }

    while (!queue.empty()) {
        int node = queue.front();
        queue.pop();
        const int link = trie[node].link;
        trie[node].out.insert(trie[node].out.end(), trie[link].out.begin(), trie[link].out.end());
        for (int ch = 0; ch < 256; ++ch) {
            int child = trie[node].next[ch];
            if (child == -1) {
                trie[node].next[ch] = trie[link].next[ch];
            } else {
                trie[child].link = trie[link].next[ch];
                queue.push(child);
            }
        }
    }

    std::vector<std::pair<std::string, int>> result;
    int node = 0;
    for (int i = 0; i < static_cast<int>(text.size()); ++i) {
        node = trie[node].next[static_cast<unsigned char>(text[i])];
        for (int id : trie[node].out) {
            result.push_back({uniquePatterns[id], i + 1 - static_cast<int>(uniquePatterns[id].size())});
        }
    }

    return result;
}
