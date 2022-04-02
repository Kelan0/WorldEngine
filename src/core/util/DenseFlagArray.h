
#ifndef WORLDENGINE_DENSEFLAGARRAY_H
#define WORLDENGINE_DENSEFLAGARRAY_H

#include "core/core.h"

class DenseFlagArray {
public:
    using pack_t = uint8_t;
    static constexpr size_t pack_bits = 8;
    static constexpr pack_t TRUE_BITS = (pack_t)(~0);
    static constexpr pack_t FALSE_BITS = (pack_t)(0);

public:
    DenseFlagArray();

    ~DenseFlagArray();

    size_t size() const;

    size_t capacity() const;

    void clear();

    void reserve(const size_t& capacity);

    void resize(const size_t& size, const bool& flag = false);

    void ensureSize(const size_t& size, const bool& flag = false);

    void expand(const size_t& index, const bool& flag = false);

    bool get(const size_t& index) const;

    void set(const size_t& index, const bool& flag);

    void set(const size_t& index, const size_t& count, const bool& flag);

    void push_back(const bool& flag);

    bool operator[](const size_t& index) const;
private:
    std::vector<pack_t> m_data;
    size_t m_size;
};


#endif //WORLDENGINE_DENSEFLAGARRAY_H
