#version 450

#include "shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 256

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 resolution;
    uint binCount;
    float minLogLum;
    float logLumRange;
//    float offset;
//    float scale;
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

shared uint histogramShared_avg[256];
shared uint histogramShared_max[256];


float getExposureMultiplier(in float luminance) {
    luminance = max(0.01, luminance);
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

    uint countForThisBin = histogramBuffer[invocationIndex];
    histogramShared_avg[invocationIndex] = countForThisBin * invocationIndex;
    histogramShared_max[invocationIndex] = invocationIndex == 0 ? 0 : invocationIndex == (binCount - 1) ? 0 : countForThisBin;

    barrier();

    for (uint i = binCount >> 1; i > 0; i >>= 1) {
        if (invocationIndex < i) {
            histogramShared_avg[invocationIndex] += histogramShared_avg[invocationIndex + i];
            histogramShared_max[invocationIndex] = max(histogramShared_max[invocationIndex], histogramShared_max[invocationIndex + i]);
        }

        barrier();
    }
    barrier();

    if (invocationIndex == 0) {
        const float weightScale = 1.0;
        uint numPixels = resolution.x * resolution.y;
        float weightedLogAverage = ((histogramShared_avg[0] / weightScale) / max(numPixels - float(countForThisBin / weightScale), 1.0)) - 1.0;
        float weightedAverageLuminance = getLuminanceFromHistogramBin(weightedLogAverage, minLogLum, logLumRange, binCount);

        histogramHeader.averageLuminance = weightedAverageLuminance;
        float exposure = getExposureMultiplier(histogramHeader.averageLuminance);
        exposure = interpolateExposure(exposure, prevHistogramHeader.exposure, speedUp, speedDown, deltaTime);
        histogramHeader.prevExposure = prevHistogramHeader.exposure;
        histogramHeader.exposure = exposure;
        histogramHeader.minLogLum = minLogLum;
        histogramHeader.logLumRange = logLumRange;
        histogramHeader.binCount = binCount;

        histogramHeader.maxValue = histogramShared_max[0];
    }
}