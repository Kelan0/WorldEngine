#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "res/shaders/common/common.glsl"

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    uvec2 resolution;
    float taaHistoryFactor;
    bool taaUseFullKernel;
};

layout(set = 0, binding = 1) uniform sampler2D frameTexture;
layout(set = 0, binding = 2) uniform sampler2D velocityTexture;
layout(set = 0, binding = 3) uniform sampler2D depthTexture;
layout(set = 0, binding = 4) uniform sampler2D previousFrameTexture;
layout(set = 0, binding = 5) uniform sampler2D previousVelocityTexture;

vec3 calculateTemporalAntiAliasing(in vec3 frameColour) {
    const vec2 TAA_PLUS_KERNEL[] = vec2[4](vec2(-1,0),vec2(+1,0),vec2(0,-1),vec2(0,+1));
    const vec2 TAA_FULL_KERNEL[] = vec2[8](vec2(-1,-1),vec2(0,-1),vec2(+1,-1),vec2(-1,0),vec2(+1,0),vec2(-1,+1),vec2(0,+1),vec2(+1,+1));

    const vec2 pixelSize = vec2(1.0) / vec2(resolution);
    vec2 closestCoord = fs_texture;
    float closestDepth = texture(depthTexture, closestCoord).r;
    vec2 currentCoord;
    float currentDepth;
    vec3 currentColour;

    vec3 colourBoundMin = frameColour;
    vec3 colourBoundMax = frameColour;

    for (uint i = 0; i < (taaUseFullKernel ? 8 : 4); ++i) {
        vec2 offset = taaUseFullKernel ? TAA_FULL_KERNEL[i] : TAA_PLUS_KERNEL[i];
        currentCoord = fs_texture + offset * pixelSize;
        currentDepth = texture(depthTexture, currentCoord).r;
        currentColour = texture(frameTexture, currentCoord).rgb;
        if (currentDepth < closestDepth) {
            closestDepth = currentDepth;
            closestCoord = currentCoord;
        }
        colourBoundMin = min(colourBoundMin, currentColour);
        colourBoundMax = max(colourBoundMax, currentColour);
    }

    vec2 currVelocity = texture(velocityTexture, closestCoord).xy / VELOCITY_PRECISION_SCALE;
    vec2 prevCoord = fs_texture - currVelocity;
    vec2 prevVelocity = texture(previousVelocityTexture, prevCoord).xy / VELOCITY_PRECISION_SCALE;

    float velocityLength = length(prevVelocity - currVelocity);
    float velocityDisocclusion = clamp((velocityLength - 1e-3) * 10.0, 0.0, 1.0);

    vec3 prevFrameColour = texture(previousFrameTexture, prevCoord).rgb;
    prevFrameColour = clamp(prevFrameColour, colourBoundMin, colourBoundMax);
    vec3 accumulation = mix(prevFrameColour, frameColour, taaHistoryFactor);

    return mix(accumulation, frameColour, velocityDisocclusion);
}

void main() {
    vec3 finalColour = texture(frameTexture, fs_texture).rgb;

    finalColour = calculateTemporalAntiAliasing(finalColour);
    outColor = vec4(finalColour, 1.0);
}