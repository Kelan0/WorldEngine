
#ifndef WORLDENGINE_UTIL_H
#define WORLDENGINE_UTIL_H

#include "core/core.h"
#include <random>
#include <concepts>
#include <array>

namespace Util {
    template<class FwdIt1, class FwdIt2>
    bool moveIter(FwdIt1 src, FwdIt2 dst) {
        if (src < dst) {
            while (src != dst) {
                std::iter_swap(src, src + 1);
                ++src;
            }
            return true;
        } else if (src > dst) {
            while (src != dst) {
                std::iter_swap(src, src - 1);
                --src;
            }
            return true;
        }
        return false;
    }

    template<class Vec>
    typename Vec::iterator iter(const Vec& vec, size_t idx) {
        return vec.begin() + idx;
    }

    uint64_t nextPowerOf2(uint64_t v);

    template<size_t strLen>
    double getMemorySizeMagnitude(uint64_t bytes, char(&sizeLabel)[strLen]) {
        strcpy_s(sizeLabel, strLen, strLen >= 5 ? "bytes" : "B");
        double size = (double)bytes;
        if (size >= 1024) {
            size /= 1024;
            strcpy_s(sizeLabel, strLen, strLen >= 3 ? "KiB" : strLen >= 2 ? "KB" : "K");
            if (size >= 1024) {
                size /= 1024;
                strcpy_s(sizeLabel, strLen, strLen >= 3 ? "MiB" : strLen >= 2 ? "MB" : "M");
                if (size >= 1024) {
                    size /= 1024;
                    strcpy_s(sizeLabel, strLen, strLen >= 3 ? "GiB" : strLen >= 2 ? "GB" : "G");
                }
            }
        }

        return size;
    }

    template<typename T>
    size_t removeVectorOverflowStart(std::vector<T>& vec, const size_t& maxSize) {
        if (vec.size() > maxSize) {
            int64_t removeCount = vec.size() - maxSize;
            vec.erase(vec.begin(), vec.begin() + removeCount);
            return removeCount;
        }
        return 0;
    }

    template<class Map, class Key, typename Func>
    typename Map::mapped_type & mapComputeIfAbsent(Map& map, Key const& key, Func computeFunc) {
        typedef typename Map::mapped_type value_t;
        std::pair<typename Map::iterator, bool> result = map.insert(typename Map::value_type(key, value_t{}));
        value_t& val = result.first->second;
        if (result.second)
            val = computeFunc(key);
        return val;
    }

    template<class Map, class Key, typename Func>
    typename Map::mapped_type & mapInsertIfAbsent(Map& map, Key const& key, Func computeFunc) {
        typedef typename Map::mapped_type value_t;
        typename Map::iterator it = map.find(key);
        if (it == map.end()) {
            std::pair<typename Map::iterator, bool> result = map.insert(typename Map::value_type(key, computeFunc(key)));
            assert(result.second);
            it = result.first;
        }
        return it->second;
    }

    std::mt19937& rng();

    template<typename T>
    T random(const T& min, const T& max) {
        static_assert(std::is_floating_point_v<T> || std::is_integral_v<T>);

        if constexpr (std::is_floating_point<T>::value) {
            return std::uniform_real_distribution<T>(min, max)(rng());
        }
        if constexpr(std::is_integral<T>::value) {
            return std::uniform_int_distribution<T>(min, max)(rng());
        }
        return (T)0;
    }

    template<typename T, size_t C>
    std::array<T, C> randomArray(const T& min, const T& max) {
        std::array<T, C> array{};
        for (size_t i = 0; i < C; ++i) {
            array[i] = random<T>(min, max);
        }
        return array;
    }

    template<typename T>
    T createHaltonSequence(const uint32_t& index, const uint32_t& base) {
        static_assert(std::is_floating_point_v<T>);
        T f = T(1);
        T r = T(0);
        uint32_t current = index;
        do {
            f /= (T)base;
            r += f * (T)(current % base);
            current /= base;
        } while (current > 0);
        return r;
    }

    void trimLeft(std::string& str);

    void trimRight(std::string& str);

    void trim(std::string& str);

    std::string trimLeftCpy(std::string str);

    std::string trimRightCpy(std::string& str);

    std::string trimCpy(std::string& str);

    void splitString(const std::string_view& str, char separator, std::vector<std::string_view>& outSplitString);
    void splitString(const std::string_view& str, char separator, std::vector<std::string>& outSplitString);
}

#endif //WORLDENGINE_UTIL_H
