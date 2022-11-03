#version 450

#include "res/shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 16

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 resolution;
    float maxBrightness;
    uint binCount;
    uint channel;
    float offset;
    float scale;
};

layout(set = 0, binding = 1, std430) writeonly buffer HistogramBuffer {
    uint dstHistogramBuffer[];
};

void main() {
    const uint invocationIndex = gl_GlobalInvocationID.x;

    if (invocationIndex < binCount) {
        dstHistogramBuffer[invocationIndex] = 0;
    }
}