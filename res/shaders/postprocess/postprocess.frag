#version 450

#include "res/shaders/common/common.glsl"
#include "res/shaders/histogram/histogram.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PC1 {
    float deltaTime;
    float time;
    float test;
};

layout(set = 0, binding = 0) uniform UBO1 {
    bool bloomEnabled;
    float bloomIntensity;
    bool debugCompositeEnabled;
    float histogramOffset;
    float histogramScale;
};

layout(set = 0, binding = 1) uniform sampler2D frameColourTexture;
layout(set = 0, binding = 2) uniform sampler2D debugCompositeColourTexture;
layout(set = 0, binding = 3) uniform sampler2D bloomTexture;
//layout(set = 0, binding = 4) uniform sampler2D histogramTexture;
layout(set = 0, binding = 4, std430) buffer readonly HistogramBuffer {
    HistogramBufferHeader histogramHeader;
    uint histogramBuffer[];
};

//void debug(inout vec3 finalColour) {
//    vec2 offset = vec2(50, 50);
//    vec2 size = vec2(1400, 250);
//
//    if (gl_FragCoord.x >= offset.x && gl_FragCoord.x <= offset.x + size.x && gl_FragCoord.y >= offset.y && gl_FragCoord.y <= offset.y + size.y) {
//        vec2 coord = gl_FragCoord.xy - offset;
//        vec2 uv = coord / size;
//        uv.y = 1.0 - uv.y;
//        float eps = 2e-3;
//        if (coord.x < 1 || coord.x > size.x - 1 || coord.y < 1 || coord.y > size.y - 1) {
//            finalColour = vec3(0.0, 0.0, 0.0);
//        } else {
//            finalColour = min(finalColour, 1.0) * 2.0 / 3.0;
//
//            float hist_max = histogramHeader.maxValue * 1.05;
//
////            float transition = sin(time)
//            float hist_x = uv.x;
//            float hist_x_norm = getHistogramBinFromLuminance(uv.x * 20.0, histogramHeader.offset, histogramHeader.scale);
//            hist_x = mix(hist_x, hist_x_norm, test);
//            hist_x = floor(hist_x * float(histogramHeader.binCount)) / float(histogramHeader.binCount);
//            uint hist_ux = uint(hist_x * histogramHeader.binCount);
//            if (hist_ux % 16 == 0) finalColour.rgb = vec3(0.8);
//
////            vec4 hist = texture(histogramTexture, vec2(hist_x, 0.0)).rgba  / hist_max;
////            if (hist.r >= uv.y) finalColour.r = 1.0;
////            if (hist.g >= uv.y) finalColour.g = 1.0;
////            if (hist.b >= uv.y) finalColour.b = 1.0;
//
//            float hist = float(histogramBuffer[uint(hist_x * histogramHeader.binCount)]) / hist_max;
//            if (hist >= uv.y) {
//                finalColour = vec3(1.0);
//            }
//
//            float hist_x_avg = getHistogramBinFromLuminance(histogramHeader.averageLuminance, histogramHeader.offset, histogramHeader.scale);
////            float hist_x_avg_norm = getLuminanceFromHistogramBin(hist_x_avg, histogramHeader.offset, histogramHeader.scale);
////            hist_x_avg = mix(hist_x_avg, hist_x_avg_norm, test);
//            hist_x_avg = floor(hist_x_avg * float(histogramHeader.binCount)) / float(histogramHeader.binCount);
//            uint hist_ux_avg = uint(hist_x_avg * histogramHeader.binCount);
//            if (hist_ux == hist_ux_avg) {
//                finalColour = vec3(1.0, 0.0, 0.0);
//            }
//        }
//    }
//}

void main() {
    vec3 finalColour = texture(frameColourTexture, fs_texture).rgb;

    if (bloomEnabled) {
        vec3 bloomColour = textureLod(bloomTexture, fs_texture, 0).rgb;
        finalColour = mix(finalColour, bloomColour, bloomIntensity);
//        finalColour = finalColour + bloomColour * bloomIntensity;
    }

    finalColour *= histogramHeader.exposure;
    finalColour = finalColour / (finalColour + vec3(1.0));
    finalColour = pow(finalColour, vec3(1.0 / 2.2));

    if (debugCompositeEnabled) {
        vec4 debugCompositeColour = texture(debugCompositeColourTexture, fs_texture);
        finalColour = mix(finalColour, debugCompositeColour.rgb, debugCompositeColour.a);
    }

//    debug(finalColour);

    outColor = vec4(finalColour, 1.0);
}