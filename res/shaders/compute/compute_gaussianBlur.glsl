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

layout(set = 0, binding = 0) uniform sampler2D srcImage;
layout(set = 0, binding = 1, rgba32f) uniform image2D dstImage;

// const float halfKernel_9x1[5] = float[](0.382928, 0.241732, 0.060598, 0.005977, 0.000229); // sigma=1
const float halfKernel_9x1[5] = float[](0.20236, 0.179044, 0.124009, 0.067234, 0.028532); // sigma=2
const uint kernelSize = 5;

vec2 getShadowData(vec4 value) {
    return blurDirection == 0 ? value.rg : value.ba;
}

vec2 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
    vec2 data = vec2(0.0);
    vec2 off1 = vec2(1.411764705882353) * direction;
    vec2 off2 = vec2(3.2941176470588234) * direction;
    vec2 off3 = vec2(5.176470588235294) * direction;
    data += texture(image, uv).rg * 0.1964825501511404;
    data += texture(image, uv + (off1 / resolution)).rg * 0.2969069646728344;
    data += texture(image, uv - (off1 / resolution)).rg * 0.2969069646728344;
    data += texture(image, uv + (off2 / resolution)).rg * 0.09447039785044732;
    data += texture(image, uv - (off2 / resolution)).rg * 0.09447039785044732;
    data += texture(image, uv + (off3 / resolution)).rg * 0.010381362401148057;
    data += texture(image, uv - (off3 / resolution)).rg * 0.010381362401148057;
    return data;
}

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);

    if (invocation.x >= dstSize.x || invocation.y >= dstSize.y)
        return;

    vec2 srcResolution = vec2(srcSize);
    vec2 oneOverSrcSize = 1.0 / srcResolution;

    vec2 coordOffset = blurDirection == 0 ? vec2(1, 0) : vec2(0, 1);
    ivec2 coord = ivec2(invocation);
    vec2 srcCoord = vec2(coord) * oneOverSrcSize;

    vec2 data = blur13(srcImage, srcCoord, srcResolution, coordOffset);

    imageStore(dstImage, ivec2(coord), vec4(data, 0.0, 0.0));

//    vec2 srcCoordOffset = coordOffset * oneOverSrcSize;
//
//    vec4 initialSample = texture(srcImage, srcCoord).rgba;
//    vec4 data;
//    data.rg = initialSample.rg * halfKernel_9x1[0];
//    data.ba = initialSample.ba;
//
//
//    for (int i = 1; i < kernelSize; ++i) {
//        data.rg += texture(srcImage, vec2(srcCoord - srcCoordOffset * i)).rg * halfKernel_9x1[i];
////        data.b = max(data.b, texSample.b);
//    }
//
//    for (int i = 1; i < kernelSize; ++i) {
//        data.rg += texture(srcImage, vec2(srcCoord + srcCoordOffset * i)).rg * halfKernel_9x1[i];
////        data.b = max(data.b, texSample.b);
//    }
//
//    imageStore(dstImage, ivec2(coord), data);
}

//void main() {
//    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);
//
//    if (invocation.x >= dstSize.x || invocation.y >= dstSize.y)
//        return;
//
//    ivec2 coord = ivec2(invocation);
//    ivec2 coordOffset = blurDirection == 0 ? ivec2(1, 0) : ivec2(0, 1);
//
//    vec4 initialSample = imageLoad(varianceShadowImage[imageIndex], coord).rgba;
//    vec4 texSample = initialSample;
//    vec2 data = getShadowData(texSample) * halfKernel_9x1[0];
//
//    for (int i = 1; i < kernelSize; ++i) {
//        texSample = imageLoad(varianceShadowImage[imageIndex], ivec2(coord + coordOffset * i)).rgba;
//        data.rg += getShadowData(texSample) * halfKernel_9x1[i];
////        data.b = max(data.b, texSample.b);
//
//        texSample = imageLoad(varianceShadowImage[imageIndex], ivec2(coord - coordOffset * i)).rgba;
//        data.rg += getShadowData(texSample) * halfKernel_9x1[i];
////        data.b = max(data.b, texSample.b);
//    }
//
//    imageStore(varianceShadowImage[imageIndex], ivec2(coord), blurDirection == 0
//            ? vec4(initialSample.rg, data.rg)
//            : vec4(data.rg, initialSample.rg));
//}