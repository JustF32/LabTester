#pragma once

#include <string>
#include <utility>
#include <vector>

std::vector<int> rabinKarpSearch(const std::string &text, const std::string &pattern);
std::vector<int> kmpSearch(const std::string &text, const std::string &pattern);
std::vector<int> boyerMooreSearch(const std::string &text, const std::string &pattern);

std::vector<std::pair<std::string, int>> ahoCorasickSearch(
    const std::string &text,
    const std::vector<std::string> &patterns
);
