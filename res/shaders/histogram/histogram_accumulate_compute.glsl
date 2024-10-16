#version 450

#include "shaders/histogram/histogram.glsl"

#define GROUP_SIZE_X 16
#define GROUP_SIZE_Y 16
#define GROUP_SIZE GROUP_SIZE_X * GROUP_SIZE_Y

layout (local_size_x = GROUP_SIZE_X) in;
layout (local_size_y = GROUP_SIZE_Y) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 resolution;
    uint binCount;
    float minLogLum;
    float oneOverLogLumRange;
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
        uint weight = 1;

//        if (true) {  // Bias to center of screen
//            vec2 d = abs(coord - vec2(0.5));
//            float vfactor = clamp(1.0 - dot(d, d), 0.0, 1.0);
//            vfactor *= vfactor;
//            weight = uint(64.0 * vfactor);
//        }

        vec3 hdrColour = texture(srcImage, coord).rgb;

        float luminance = dot(hdrColour.rgb, RGB_LUMINANCE);
        float bin = getHistogramBinFromLuminance(luminance, minLogLum, oneOverLogLumRange, binCount);
        atomicAdd(shared_bins[uint(bin)], weight);
    }

    barrier();

    if (localIndex < binCount) {
        atomicAdd(dstHistogramBuffer[localIndex], shared_bins[localIndex].r);
    }
}