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

// https://github.com/microsoft/mimalloc/issues/201
inline void Util::memcpy_sse(void* dst, void const* src, size_t size) {
    // https://hero.handmade.network/forums/code-discussion/t/157-memory_bandwidth_+_implementing_memcpy
    size_t stride = 2 * sizeof ( __m128 );
    while ( size ) {
        __m128 a = _mm_load_ps ( ( float * ) ( ( uint8_t const * ) src ) + 0 * sizeof ( __m128 ) );
        __m128 b = _mm_load_ps ( ( float * ) ( ( ( uint8_t const * ) src ) + 1 * sizeof ( __m128 ) ) );
        _mm_stream_ps ( ( float * ) ( ( ( uint8_t * ) dst ) + 0 * sizeof ( __m128 ) ), a );
        _mm_stream_ps ( ( float * ) ( ( ( uint8_t * ) dst ) + 1 * sizeof ( __m128 ) ), b );
        size -= stride;
        src = ( ( uint8_t const * ) src ) + stride;
        dst = ( ( uint8_t * ) dst ) + stride;
    }
}




template<>
inline void Util::to_string<int8_t>(const int8_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<uint8_t>(const uint8_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<int16_t>(const int16_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<uint16_t>(const uint16_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<int32_t>(const int32_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<uint32_t>(const uint32_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<int64_t>(const int64_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<uint64_t>(const uint64_t& obj, std::string& outString) {
    outString = std::to_string(obj);
}

template<>
inline void Util::to_string<glm::vec1>(const glm::vec1& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::ivec1>(const glm::ivec1& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::uvec1>(const glm::uvec1& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::vec2>(const glm::vec2& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::ivec2>(const glm::ivec2& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::uvec2>(const glm::uvec2& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::vec3>(const glm::vec3& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::ivec3>(const glm::ivec3& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::uvec3>(const glm::uvec3& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::vec4>(const glm::vec4& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::ivec4>(const glm::ivec4& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}

template<>
inline void Util::to_string<glm::uvec4>(const glm::uvec4& obj, std::string& outString) {
    outString = std::to_string(obj.x);
}