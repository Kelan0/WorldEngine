
#ifndef WORLDENGINE_HASH_H
#define WORLDENGINE_HASH_H

#include <utility>
#include <glm/glm.hpp>

namespace std {

    // Why isn't this included in the standard library?
    template<class T>
    static inline void hash_combine(std::size_t& seed, const T& v) {
        std::hash<T> h;
        seed ^= h(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<class It>
    static inline void hash_range(std::size_t& seed, It first, It last) {
        for (; first != last; ++first) {
            hash_combine(seed, *first);
        }
    }

    template<class It>
    static inline std::size_t hash_range(It first, It last) {
        size_t seed = 0;
        for (; first != last; ++first) {
            std::hash_combine(seed, *first);
        }
        return seed;
    }

    template<glm::length_t L, typename T, enum glm::qualifier Q>
    struct hash<glm::vec<L, T, Q>> {
        size_t operator()(const glm::vec<L, T, Q>& v) const {
            size_t seed = 0;
            for (int i = 0; i < L; ++i)
                std::hash_combine(seed, v[i]);
            return seed;
        }
    };

    template <typename Container>
    struct container_hash {
        std::size_t operator()(Container const& c) const {
            return std::hash_range(c.begin(), c.end());
        }
    };

    template<typename T>
    struct hash<std::vector<T>> {
        size_t operator()(const std::vector<T>& v) const {
            return std::hash_range(v.begin(), v.end());
        }
    };

    template<glm::length_t C, glm::length_t R, typename T, enum glm::qualifier Q>
    struct hash<glm::mat<C, R, T, Q>> {
        size_t operator()(const glm::mat<C, R, T, Q>& m) const {
            size_t seed = 0;
            for (int i = 0; i < C; ++i)
                for (int j = 0; j < R; ++j)
                    std::hash_combine(seed, m[i][j]);
            return seed;
        }
    };
}

#endif //WORLDENGINE_HASH_H
