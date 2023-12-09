#include "Float16.h"

short floatToFloat16(float value) {
    return glm::detail::toFloat16(value);
//    uint16_t u16;
//    uint32_t u32;
//    memcpy(&u32, &value, sizeof(float));
//    u16 = ((u32 & 0x7fffffff) >> 13) - (0x38000000 >> 13);
//    u16 |= ((u32 & 0x80000000) >> 16);
//    return u16;
}

float float16ToFloat(short value) {
    return glm::detail::toFloat32(value);
//    uint32_t u32 = ((value & 0x8000) << 16);
//    u32 |= ((value & 0x7fff) << 13) + 0x38000000;
//    float fRet;
//    memcpy(&fRet, &u32, sizeof(float));
//    return fRet;
}
