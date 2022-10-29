#include "core/util/Util.h"

uint64_t Util::nextPowerOf2(uint64_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}




std::mt19937& Util::rng() {
    static std::mt19937 r(std::random_device{}());
    return r;
}

void Util::trimLeft(std::string& str) {
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void Util::trimRight(std::string& str) {
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

void Util::trim(std::string& str) {
    trimLeft(str);
    trimRight(str);
}

std::string Util::trimLeftCpy(std::string str) {
    trimLeft(str);
    return str;
}

std::string Util::trimRightCpy(std::string str) {
    trimRight(str);
    return str;
}

std::string Util::trimCpy(std::string str) {
    trim(str);
    return str;
}

// https://codereview.stackexchange.com/questions/260457/efficiently-splitting-a-string-in-c
void Util::splitString(const std::string_view& str, char separator, std::vector<std::string_view>& outSplitString) {
    for (auto p = str.begin();; ++p) {
        auto q = p;
        p = std::find(p, str.end(), separator);
        outSplitString.emplace_back(q, p);
        if (p == str.end())
            return;
    }
}
void Util::splitString(const std::string_view& str, char separator, std::vector<std::string>& outSplitString) {
    for (auto p = str.begin();; ++p) {
        auto q = p;
        p = std::find(p, str.end(), separator);
        if (p != q)
            outSplitString.emplace_back(q, p);
        if (p == str.end())
            return;
    }
}