#version 450

#include "res/shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 256

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 resolution;
    float maxBrightness;
    uint binCount;
    float offset;
    float scale;
};

layout(set = 0, binding = 1, std430) buffer HistogramBuffer {
    HistogramBufferHeader histogramHeader;
    uint histogramBuffer[];
};

shared uint shared_bins[256];

void filterLuminance(in uint binIndex, in float oneOverMaxValue, in float offset, in float scale, inout vec4 filterVal) {
    float binValue = float(histogramBuffer[binIndex]) * oneOverMaxValue;

    float deltaOffset = min(filterVal.z, binValue);
    binValue -= deltaOffset;
    filterVal.zw -= deltaOffset.xx;

    binValue = min(filterVal.w, binValue);
    filterVal.w -= binValue;

    float luminance = getLuminanceFromHistogramBin(float(binIndex) / float(binCount), offset, scale);

    filterVal.xy += vec2(luminance * binValue, binValue);
}

float getAverageLuminance(in float lowPercent, in float highPercent, in float oneOverMaxValue, in float offset, in float scale) {
    float totalSum = 0.0;

    for (uint i = 0; i < binCount; ++i)
        totalSum += float(histogramBuffer[i]) * oneOverMaxValue;

    vec4 filterVal = vec4(0.0, 0.0, totalSum * lowPercent, totalSum * highPercent);

    for (uint i = 0; i < binCount; ++i)
        filterLuminance(i, oneOverMaxValue, offset, scale, filterVal);

    return filterVal.x / max(filterVal.y, 1e-5);
}

void main() {
    const uint invocationIndex = gl_GlobalInvocationID.x;

    if (invocationIndex < binCount) {
        shared_bins[invocationIndex] = histogramBuffer[invocationIndex];

        barrier();

        for (uint i = binCount >> 1; i > 0; i >>= 1) {
            if (invocationIndex < i)
                shared_bins[invocationIndex] = max(shared_bins[invocationIndex], shared_bins[invocationIndex + i]);

            barrier();
        }
        barrier();

        if (invocationIndex == 0) {
            histogramHeader.maxValue = shared_bins[0];
            histogramHeader.averageLuminance = getAverageLuminance(0.1, 0.9, 1.0 / float(shared_bins[0]), offset, scale);
        }
    }
}