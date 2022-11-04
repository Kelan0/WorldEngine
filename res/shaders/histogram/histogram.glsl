
#include "res/shaders/common/common.glsl"

struct HistogramBufferHeader {
    uint binCount;
    float offset;
    float scale;
    float averageLuminance;
    uint maxValue;
};

float getHistogramBinFromLuminance(float value, float offset, float scale) {
    float logValue = log2(value);
//    logValue = mix(-9999.0, logValue, value < 1e-4); // Correct for log(0)
    return clamp(logValue * scale + offset, 0.0, 1.0);
}

float getLuminanceFromHistogramBin(float bin, float offset, float scale) {
    return exp2((bin - offset) / scale);
}
