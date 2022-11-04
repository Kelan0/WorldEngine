#version 450

#include "res/shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16
#define GROUP_SIZE GROUP_SIZE_X * GROUP_SIZE_Y

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = GROUP_SIZE_Y) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 resolution;
    float maxBrightness;
    uint binCount;
    float offset;
    float scale;
};

layout(set = 0, binding = 0) uniform sampler2D srcImage;
layout(set = 0, binding = 1, std430) writeonly buffer HistogramBuffer {
    HistogramBufferHeader dstHistogramHeader;
    uint dstHistogramBuffer[];
};

shared uint shared_bins[256];

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);
    const uint localIndex = gl_LocalInvocationIndex;

    if (localIndex < 256) {
        shared_bins[localIndex] = 0;
    }

    barrier();

    if (invocation.x < resolution.x && invocation.y < resolution.y) {
        vec2 coord = vec2(invocation.xy) / vec2(resolution);
        vec3 colour = texture(srcImage, coord).rgb;

        float luminance = dot(colour.rgb, RGB_LUMINANCE);
        float bin = getHistogramBinFromLuminance(luminance, offset, scale) * (binCount - 1);
        atomicAdd(shared_bins[uint(bin)], 1);
    }

    barrier();

    if (localIndex < binCount) {
        atomicAdd(dstHistogramBuffer[localIndex], shared_bins[localIndex].r);
    }
}