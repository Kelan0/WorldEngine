#version 450

#include "res/shaders/common/common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    bool bloomEnabled;
    float bloomIntensity;
    bool debugCompositeEnabled;
};

layout(set = 0, binding = 1) uniform sampler2D frameColourTexture;
layout(set = 0, binding = 2) uniform sampler2D debugCompositeColourTexture;
layout(set = 0, binding = 3) uniform sampler2D bloomTexture;
layout(set = 0, binding = 4) uniform sampler2D histogramTexture;

void debug(inout vec3 finalColour) {
    vec2 offset = vec2(50, 50);
    vec2 size = vec2(500, 250);

    if (gl_FragCoord.x >= offset.x && gl_FragCoord.x <= offset.x + size.x && gl_FragCoord.y >= offset.y && gl_FragCoord.y <= offset.y + size.y) {
        vec2 coord = gl_FragCoord.xy - offset;
        vec2 uv = coord / size;
        uv.y = 1.0 - uv.y;
        float eps = 2e-3;
        if (coord.x < 1 || coord.x > size.x - 1 || coord.y < 1 || coord.y > size.y - 1) {
            finalColour = vec3(0.0, 0.0, 0.0);
        } else {
            finalColour *= 2.0 / 3.0;

            float hist_x = uv.x;
//            hist_x = log2((hist_x / 8.0) + 1.0) * log2(256.0);
            vec4 hist = texture(histogramTexture, vec2(hist_x, 0.0)).rgba * 0.0004;

            if (hist.r >= uv.y) finalColour.r = 1.0;
            if (hist.g >= uv.y) finalColour.g = 1.0;
            if (hist.b >= uv.y) finalColour.b = 1.0;
        }
    }
}

void main() {
    vec3 finalColour = texture(frameColourTexture, fs_texture).rgb;

    if (bloomEnabled) {
        vec3 bloomColour = textureLod(bloomTexture, fs_texture, 0).rgb;
        finalColour = mix(finalColour, bloomColour, bloomIntensity);
//        finalColour = finalColour + bloomColour * bloomIntensity;
    }

    finalColour = finalColour / (finalColour + vec3(1.0));
    finalColour = pow(finalColour, vec3(1.0 / 2.2));

    if (debugCompositeEnabled) {
        vec4 debugCompositeColour = texture(debugCompositeColourTexture, fs_texture);
        finalColour = mix(finalColour, debugCompositeColour.rgb, debugCompositeColour.a);
    }

    debug(finalColour);
    outColor = vec4(finalColour, 1.0);
}