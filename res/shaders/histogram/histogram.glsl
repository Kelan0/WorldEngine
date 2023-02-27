
#include "shaders/common/common.glsl"

struct HistogramBufferHeader {
    uint binCount;
    float minLogLum;
    float logLumRange;
    float averageLuminance;
    uint maxValue;
    uint sumValue;
    float prevExposure;
    float exposure;
};

float getHistogramBinFromLuminance(float value, float minLogLum, float oneOverLogLumRange, float binCount) {
    const float EPSILON = 1.0 / 255.0;
    if (value < EPSILON) {
        return 0;
    }
    float logLuminance = clamp((log2(value) - minLogLum) * oneOverLogLumRange, 0.0, 1.0);
    return logLuminance * (binCount - 2.0) + 1.0;
}

float getLuminanceFromHistogramBin(float bin, float minLogLum, float logLumRange, float binCount) {
    return exp2((((bin - 1.0) / (binCount - 2.0)) * logLumRange) + minLogLum);
}