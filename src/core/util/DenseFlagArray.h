
#ifndef WORLDENGINE_DENSEFLAGARRAY_H
#define WORLDENGINE_DENSEFLAGARRAY_H

#include "core/core.h"

// TODO: maybe we could use AVX/SIMD to operate on wider sections of the dense array at once?

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

    void reserve(size_t capacity);

    void resize(size_t size, bool flag = false);

    void ensureSize(size_t size, bool flag = false);

    void expand(size_t index, bool flag = false);

    bool get(size_t index) const;

    void set(size_t index, bool flag);

    void set(size_t index, size_t count, bool flag);

    void push_back(bool flag);

    bool operator[](size_t index) const;
private:
    std::vector<pack_t> m_data;
    size_t m_size;
};


#endif //WORLDENGINE_DENSEFLAGARRAY_H
