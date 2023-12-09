#ifndef WORLDENGINE_FLOAT16_H
#define WORLDENGINE_FLOAT16_H

#include "core/core.h"
#include <glm/detail/type_half.hpp>

extern short floatToFloat16(float value);

extern float float16ToFloat(short value);

// Implementation mostly copied from:
// https://stackoverflow.com/questions/22210684/16-bit-floats-and-gl-half-float
class Float16 {
public:
    Float16();

    Float16(float value);

    Float16(const Float16& value);

    operator glm::detail::hdata();
    operator glm::detail::hdata() const;

    operator float();
    operator float() const;

    friend Float16 operator+(const Float16& val1, const Float16& val2);

    friend Float16 operator-(const Float16& val1, const Float16& val2);

    friend Float16 operator*(const Float16& val1, const Float16& val2);

    friend Float16 operator/(const Float16& val1, const Float16& val2);

    Float16& operator=(const Float16& val);

    Float16& operator+=(const Float16& val);

    Float16& operator-=(const Float16& val);

    Float16& operator*=(const Float16& val);

    Float16& operator/=(const Float16& val);

    Float16& operator-();

private:
    short m_value;
};


inline Float16::Float16() {
}


inline Float16::Float16(float value) {
    m_value = floatToFloat16(value);
}


inline Float16::Float16(const Float16& value) {
    m_value = value.m_value;
}


inline Float16::operator glm::detail::hdata() {
    return (glm::detail::hdata)m_value;
}

inline Float16::operator glm::detail::hdata() const {
    return (glm::detail::hdata)m_value;
}

inline Float16::operator float() {
    return float16ToFloat(m_value);
}

inline Float16::operator float() const {
    return float16ToFloat(m_value);
}

inline Float16& Float16::operator=(const Float16& val) {
    m_value = val.m_value;
}

inline Float16& Float16::operator+=(const Float16& val) {
    *this = *this + val;
    return *this;
}

inline Float16& Float16::operator-=(const Float16& val) {
    *this = *this - val;
    return *this;
}

inline Float16& Float16::operator*=(const Float16& val) {
    *this = *this * val;
    return *this;
}

inline Float16& Float16::operator/=(const Float16& val) {
    *this = *this / val;
    return *this;
}

inline Float16& Float16::operator-() {
    *this = Float16(-(float)*this);
    return *this;
}

inline Float16 operator+(const Float16& val1, const Float16& val2) {
    return Float16((float)val1 + (float)val2);
}

inline Float16 operator-(const Float16& val1, const Float16& val2) {
    return Float16((float)val1 - (float)val2);
}

inline Float16 operator*(const Float16& val1, const Float16& val2) {
    return Float16((float)val1 * (float)val2);
}

inline Float16 operator/(const Float16& val1, const Float16& val2) {
    return Float16((float)val1 / (float)val2);
}


#endif //WORLDENGINE_FLOAT16_H
