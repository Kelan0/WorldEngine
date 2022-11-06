#version 450

#include "res/shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 256

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uint binCount;
    float offset;
    float scale;
    float lowPercent;
    float highPercent;
    float speedUp;
    float speedDown;
    float exposureCompensation;
    float deltaTime;
};

layout(set = 0, binding = 1, std430) buffer HistogramBuffer {
    HistogramBufferHeader histogramHeader;
    uint histogramBuffer[];
};

layout(set = 0, binding = 2, std430) buffer PrevHistogramBuffer {
    HistogramBufferHeader prevHistogramHeader;
    uint prevHistogramBuffer[];
};

shared uint shared_bins_max[256];
shared uint shared_bins_sum[256];

void filterLuminance(in uint binIndex, in float oneOverMaxValue, in float offset, in float scale, inout vec4 filterVal) {
    float bin = float(binIndex) / float(binCount);
    float binValue = float(histogramBuffer[binIndex]) * oneOverMaxValue;

    float deltaOffset = min(filterVal.z, binValue);
    binValue -= deltaOffset;
    filterVal.zw -= deltaOffset.xx;

    binValue = min(filterVal.w, binValue);
    filterVal.w -= binValue;

    float luminance = getLuminanceFromHistogramBin(bin, offset, scale);

    filterVal.xy += vec2(luminance * binValue, binValue);
}

float getAverageLuminance(in float lowPercent, in float highPercent, in float oneOverMaxValue, in uint sumValue, in float offset, in float scale) {
    float fSumValue = float(sumValue) * oneOverMaxValue;
    vec4 filterVal = vec4(0.0, 0.0, fSumValue * lowPercent, fSumValue * highPercent);

    for (uint i = 0; i < binCount; ++i)
        filterLuminance(i, oneOverMaxValue, offset, scale, filterVal);

    return filterVal.x / max(filterVal.y, 1e-5);
}

float getExposureMultiplier(in float luminance) {
    luminance = max(1e-5, luminance);
//    float keyValue = 1.03 - (2.0 / (2.0 + log2(luminance + 1.0)));

    float keyValue = 0.18;
    float exposure = (keyValue / luminance) - exposureCompensation;
//    float exposure = 1.0 / (9.6 * luminance);

    return exposure;
}
float getStandardOutputBasedExposure(float aperture, float shutterSpeed, float iso) {
    float l_avg = (1000.0f / 65.0f) * (aperture * aperture) / (iso * shutterSpeed);
    return 0.18 / l_avg;
}

float interpolateExposure(in float newExposure, in float oldExposure, in float speedUp, in float speedDown, in float dt) {
    float delta = newExposure - oldExposure;
    float speed = mix(speedDown, speedUp, delta > 0.0);
    return oldExposure + delta * (1.0 - exp2(-dt * speed));
}

void main() {
    const uint invocationIndex = gl_GlobalInvocationID.x;

    if (invocationIndex < binCount) {
        shared_bins_max[invocationIndex] = histogramBuffer[invocationIndex];
        shared_bins_sum[invocationIndex] = histogramBuffer[invocationIndex];

        barrier();

        for (uint i = binCount >> 1; i > 0; i >>= 1) {
            if (invocationIndex < i) {
                shared_bins_max[invocationIndex] = max(shared_bins_max[invocationIndex], shared_bins_max[invocationIndex + i]);
                shared_bins_sum[invocationIndex] += shared_bins_sum[invocationIndex + i];
            }

            barrier();
        }
        barrier();

        if (invocationIndex == 0) {
            histogramHeader.maxValue = shared_bins_max[0];
            histogramHeader.sumValue = shared_bins_sum[0];
            histogramHeader.averageLuminance = getAverageLuminance(lowPercent, highPercent, 1.0 / shared_bins_max[0], shared_bins_sum[0], offset, scale);
            float exposure = getExposureMultiplier(histogramHeader.averageLuminance);
//            float exposure = getStandardOutputBasedExposure(1.4, 0.02, 2000);
            exposure = interpolateExposure(exposure, prevHistogramHeader.exposure, speedUp, speedDown, deltaTime);
            histogramHeader.prevExposure = prevHistogramHeader.exposure;
            histogramHeader.exposure = exposure;
        }
    }
}