
#include "DenseFlagArray.h"

#define BIT(bitIndex) (1 << (bitIndex))
#define SET_BIT(val, bitIndex, isSet) if (isSet) val |= BIT(bitIndex); else val &= ~BIT(bitIndex);
#define GET_BIT(val, bitIndex) (((val) >> (bitIndex)) & 1)

DenseFlagArray::DenseFlagArray():
    m_size(0) {
}

DenseFlagArray::~DenseFlagArray() {

}

size_t DenseFlagArray::size() const {
    return m_size;
}

size_t DenseFlagArray::capacity() const {
    return m_data.capacity() * pack_bits;
}

void DenseFlagArray::clear() {
    m_data.clear();
    m_size = 0;
}

void DenseFlagArray::reserve(const size_t& capacity) {
    m_data.reserve(capacity * pack_bits);
}

void DenseFlagArray::resize(const size_t& size, const bool& flag) {
    PROFILE_SCOPE("DenseFlagArray::resize")
    size_t packedSize = INT_DIV_CEIL(size, pack_bits);
    
    if (packedSize != m_data.size())
        m_data.resize(packedSize, flag ? TRUE_BITS : FALSE_BITS);

    m_size = size;
}

void DenseFlagArray::ensureSize(const size_t& size, const bool& flag) {
    if (m_size < size)
        resize(size, flag);
}

void DenseFlagArray::expand(const size_t& index, const bool& flag) {
    if (m_size <= index) {
        size_t next = index + 1;
        if (next >= capacity())
            reserve(next + next / 2);
        resize(next, flag);
    }
}

bool DenseFlagArray::get(const size_t& index) const {
    if (index >= size()) {
        assert(false);
    }
    assert(index < size());

    if constexpr (pack_bits == 1) {
        return m_data[index];
    } else {
        return GET_BIT(m_data[index / pack_bits], index % pack_bits);
    }
}

void DenseFlagArray::set(const size_t& index, const bool& flag) {
    assert(index < size());

//    ensureCapacity(index);

    SET_BIT(m_data[index / pack_bits], index % pack_bits, flag);
}

void DenseFlagArray::set(const size_t& index, const size_t& count, const bool& flag) {
    PROFILE_SCOPE("DenseFlagArray::set");

    assert(index + count <= size());

    if (count == 0) {
        return;
    }
    if (count == 1) {
        SET_BIT(m_data[index / pack_bits], index % pack_bits, flag);
        return;
    }

    size_t firstPackIndex = index / pack_bits;
    size_t firstBitIndex = index % pack_bits;
    size_t lastPackIndex = (index + count) / pack_bits;
    size_t lastBitIndex = (index + count) % pack_bits;

    PROFILE_REGION("Set first unaligned bits")
    if (firstBitIndex != 0) {
        if (firstPackIndex == lastPackIndex) {
            for (size_t i = firstBitIndex; i < lastBitIndex; ++i)
                SET_BIT(m_data[firstPackIndex], i, flag);
            lastBitIndex = 0;
        } else {
            for (size_t i = firstBitIndex; i < pack_bits; ++i)
                SET_BIT(m_data[firstPackIndex], i, flag);
            ++firstPackIndex;
        }
    }

    PROFILE_REGION("Set all aligned bits")
    if (firstPackIndex != lastPackIndex)
        memset(&m_data[firstPackIndex], flag ? TRUE_BITS : FALSE_BITS, (lastPackIndex - firstPackIndex) * sizeof(pack_t));

//    for (size_t i = firstPackIndex; i != lastPackIndex; ++i)
//        m_data[i] = flag ? TRUE_BITS : FALSE_BITS;

    PROFILE_REGION("Set last unaligned bits")
    for (size_t i = 0; i < lastBitIndex; ++i)
        SET_BIT(m_data[lastPackIndex], i, flag);

}

bool DenseFlagArray::operator[](const size_t& index) const {
    return get(index);
}

void DenseFlagArray::push_back(const bool& flag) {
    size_t index = size();
    expand(index);
    set(index, flag);
}
