#version 450

#include "shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 16

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uint binCount;
};

layout(set = 0, binding = 1, std430) writeonly buffer HistogramBuffer {
    HistogramBufferHeader dstHistogramHeader;
    uint dstHistogramBuffer[];
};

void main() {
    const uint invocationIndex = gl_GlobalInvocationID.x;

    dstHistogramHeader.binCount = binCount;
    dstHistogramHeader.averageLuminance = 0.0;
    dstHistogramHeader.maxValue = 0;
    dstHistogramHeader.sumValue = 0;

    if (invocationIndex < binCount) {
        dstHistogramBuffer[invocationIndex] = 0;
    }
}