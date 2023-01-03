#version 450

layout (local_size_x = 16) in;
layout (local_size_y = 16) in;
layout (local_size_z = 1) in;

#include "shaders/common/common.glsl"

layout(set = 0, binding = 0) uniform UBO1 {
    ivec2 faceSize;
    ivec2 sourceSize;
    ivec2 sampleCount;
};

layout(set = 0, binding = 1, rgba32f) uniform readonly imageBuffer srcEquirectangularImageBuffer;
layout(set = 0, binding = 2, rgba32f) uniform writeonly imageBuffer dstCubemapLayersImageBuffer;

// layout(binding = 0, rgba32f) uniform readonly image2D srcImage;
// layout(binding = 1) uniform writeonly imageCube dstImage;

vec4 loadSourceTexel(in vec2 coord) {
    int texelIndex00 = int(coord.x) + int(coord.y) * int(sourceSize.x);
    vec4 colour00 = imageLoad(srcEquirectangularImageBuffer, texelIndex00);

    vec2 subpixel = fract(coord);
    if (subpixel.x < 1e-3 && subpixel.y < 1e-3) {
        return colour00;
    } else {

        int texelIndex01 = int(coord.x) + int(coord.y + 1) * int(sourceSize.x);
        int texelIndex11 = int(coord.x + 1) + int(coord.y + 1) * int(sourceSize.x);
        int texelIndex10 = int(coord.x + 1) + int(coord.y) * int(sourceSize.x);

        vec4 colour01 = imageLoad(srcEquirectangularImageBuffer, texelIndex01);
        vec4 colour11 = imageLoad(srcEquirectangularImageBuffer, texelIndex11);
        vec4 colour10 = imageLoad(srcEquirectangularImageBuffer, texelIndex10);
        
        vec4 colourY0 = mix(colour00, colour10, subpixel.x);
        vec4 colourY1 = mix(colour01, colour11, subpixel.x);
        return mix(colourY0, colourY1, subpixel.y);
    }
}

void main() {
    ivec3 pixelCoord = ivec3(gl_GlobalInvocationID.xyz);
    if (pixelCoord.x >= faceSize.x || pixelCoord.y >= faceSize.y || pixelCoord.z >= 6)
        return;

    vec4 colour = vec4(0.0);
    float counter = 0.0;

    ivec2 numSamples = clamp(sampleCount, 1, 64);

    vec2 inverseFaceSize = vec2(1.0) / vec2(faceSize);
    vec2 sampleOffset = vec2(1.0) / vec2(numSamples);
    vec2 subpixelCoord;
    int i, j;

    for (i = 0, subpixelCoord.y = pixelCoord.y + sampleOffset.y * 0.5; i < numSamples.y; ++i, subpixelCoord.y += sampleOffset.y) {
        for (j = 0, subpixelCoord.x = pixelCoord.x + sampleOffset.x * 0.5; j < numSamples.x; ++j, subpixelCoord.x += sampleOffset.x) {
            vec3 ray = getCubemapRay(subpixelCoord * inverseFaceSize, pixelCoord.z);
            colour += loadSourceTexel(getEquirectangularCoordinate(ray, sourceSize));
            counter += 1.0;
        }
    }

    colour /= counter;


    vec3 centerRay = getCubemapRay(vec2(pixelCoord.x + 0.5, pixelCoord.y + 0.5) / vec2(faceSize), pixelCoord.z);
    ivec3 dstCoord = ivec3(getCubemapCoordinate(centerRay) * vec3(faceSize.xy, 1));
    int dstTexelIndex = int(dstCoord.x + dstCoord.y * faceSize.x + dstCoord.z * faceSize.x * faceSize.y);

    imageStore(dstCubemapLayersImageBuffer, dstTexelIndex, colour);
}