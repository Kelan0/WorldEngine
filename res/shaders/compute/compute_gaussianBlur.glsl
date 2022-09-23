#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (local_size_x = 8) in;
layout (local_size_y = 8) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 srcSize;
    uvec2 dstSize;
    float blurRadius;
    uint blurDirection;
    uint imageIndex;
};

layout(set = 0, binding = 0, rgba32f) uniform image2D varianceShadowImage[];

// const float halfKernel_9x1[5] = float[](0.382928, 0.241732, 0.060598, 0.005977, 0.000229); // sigma=1
const float halfKernel_9x1[5] = float[](0.20236, 0.179044, 0.124009, 0.067234, 0.028532); // sigma=2
const uint kernelSize = 5;

vec2 getShadowData(vec4 value) {
    return blurDirection == 0 ? value.rg : value.ba;
}

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);

    if (invocation.x >= dstSize.x || invocation.y >= dstSize.y)
        return;

    ivec2 coord = ivec2(invocation);
    ivec2 coordOffset = blurDirection == 0 ? ivec2(1, 0) : ivec2(0, 1);

//    vec2 textureCoord = (vec2(invocation.xy) + vec2(0.5)) / vec2(srcSize);
//    vec2 textureOffset = blurDirection == 0
//                ? vec2(1.0 / float(srcSize.x), 0.0)
//                : vec2(0.0, 1.0 / float(srcSize.y));

    vec4 initialSample = imageLoad(varianceShadowImage[imageIndex], coord).rgba;
    vec4 texSample = initialSample;
    vec2 data = getShadowData(texSample) * halfKernel_9x1[0];

    for (int i = 1; i < kernelSize; ++i) {
        texSample = imageLoad(varianceShadowImage[imageIndex], ivec2(coord + coordOffset * i)).rgba;
        data.rg += getShadowData(texSample) * halfKernel_9x1[i];
//        data.b = max(data.b, texSample.b);
        
        texSample = imageLoad(varianceShadowImage[imageIndex], ivec2(coord - coordOffset * i)).rgba;
        data.rg += getShadowData(texSample) * halfKernel_9x1[i];
//        data.b = max(data.b, texSample.b);
    }

    imageStore(varianceShadowImage[imageIndex], ivec2(coord), blurDirection == 0
            ? vec4(initialSample.rg, data.rg)
            : vec4(data.rg, initialSample.rg));
}