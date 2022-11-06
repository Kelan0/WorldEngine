#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "res/shaders/common/common.glsl"
#include "res/shaders/histogram/histogram.glsl"

layout(push_constant) uniform PC1 {
    vec2 texelSize;
    uint passIndex;
    uint _pad0;
};

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    float filterRadius;
    float threshold;
    float softThreshold;
    float maxBrightness;
};

layout(set = 0, binding = 1) uniform sampler2D srcTexture;

layout(set = 0, binding = 2, std430) readonly buffer ExposureBuffer {
    HistogramBufferHeader exposureBufferHeader;
};

vec3 applyBloomThreshold(in vec3 colour) {
    float brightness = max(colour.r, max(colour.g, colour.b));
    float knee = threshold * softThreshold;
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-5);
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 1e-5);
    return colour * contribution;
}

vec3 sampleColour(in vec2 coord) {
    vec3 colour = texture(srcTexture, coord).rgb;
    if (passIndex == 0) {
        colour = applyBloomThreshold(colour);
    }
    return colour;
}

vec3 boxFilter(vec2 coord, float delta) {
    vec4 offset = texelSize.xyxy * vec2(-delta, delta).xxyy;
    vec3 colour = texture(srcTexture, coord + offset.xy).rgb
                + texture(srcTexture, coord + offset.zy).rgb
                + texture(srcTexture, coord + offset.xw).rgb
                + texture(srcTexture, coord + offset.zw).rgb;
    return colour * 0.25;
}


vec3 downsampleImpl1() {
    const vec2 r1 = texelSize;
    const vec2 r2 = r1 * 2.0;

    vec3 finalColour;

    vec2 coord;
    vec3 a = sampleColour(vec2(fs_texture.x - r2.x, fs_texture.y + r2.y));
    vec3 b = sampleColour(vec2(fs_texture.x,        fs_texture.y + r2.y));
    vec3 c = sampleColour(vec2(fs_texture.x + r2.x, fs_texture.y + r2.y));
    vec3 d = sampleColour(vec2(fs_texture.x - r2.x, fs_texture.y));
    vec3 e = sampleColour(vec2(fs_texture.x,        fs_texture.y));
    vec3 f = sampleColour(vec2(fs_texture.x + r2.x, fs_texture.y));
    vec3 g = sampleColour(vec2(fs_texture.x - r2.x, fs_texture.y - r2.y));
    vec3 h = sampleColour(vec2(fs_texture.x,        fs_texture.y - r2.y));
    vec3 i = sampleColour(vec2(fs_texture.x + r2.x, fs_texture.y - r2.y));

    vec3 j = sampleColour(vec2(fs_texture.x - r1.x, fs_texture.y + r1.y));
    vec3 k = sampleColour(vec2(fs_texture.x + r1.x, fs_texture.y + r1.y));
    vec3 l = sampleColour(vec2(fs_texture.x - r1.x, fs_texture.y - r1.y));
    vec3 m = sampleColour(vec2(fs_texture.x + r1.x, fs_texture.y - r1.y));

    if (passIndex == 0) {
        vec3 g0 = (a + b + d + e) * (0.125 / 4.0);
        vec3 g1 = (b + c + e + f) * (0.125 / 4.0);
        vec3 g2 = (d + e + g + h) * (0.125 / 4.0);
        vec3 g3 = (e + f + h + i) * (0.125 / 4.0);
        vec3 g4 = (j + k + l + m) * (0.5 / 4.0);
        g0 *= 1.0 / (1.0 + dot(g0, RGB_LUMINANCE) * 0.25);
        g1 *= 1.0 / (1.0 + dot(g1, RGB_LUMINANCE) * 0.25);
        g2 *= 1.0 / (1.0 + dot(g2, RGB_LUMINANCE) * 0.25);
        g3 *= 1.0 / (1.0 + dot(g3, RGB_LUMINANCE) * 0.25);
        g4 *= 1.0 / (1.0 + dot(g4, RGB_LUMINANCE) * 0.25);
        finalColour = g0 + g1 + g2 + g3 + g4;
    } else {
        finalColour = e * 0.125 +
        (a + c + g + i) * 0.03125 +
        (b + d + f + h) * 0.0625 +
        (j + k + l + m) * 0.125;
    }

    return finalColour;
}

vec3 downsampleBox13Tap(in sampler2D tex, in vec2 coord, in vec2 texelSize) {
    vec3 a = texture(tex, coord + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 b = texture(tex, coord + texelSize * vec2( 0.0, -1.0)).rgb;
    vec3 c = texture(tex, coord + texelSize * vec2( 1.0, -1.0)).rgb;
    vec3 d = texture(tex, coord + texelSize * vec2(-0.5, -0.5)).rgb;
    vec3 e = texture(tex, coord + texelSize * vec2( 0.5, -0.5)).rgb;
    vec3 f = texture(tex, coord + texelSize * vec2(-1.0,  0.0)).rgb;
    vec3 g = texture(tex, coord                               ).rgb;
    vec3 h = texture(tex, coord + texelSize * vec2( 1.0,  0.0)).rgb;
    vec3 i = texture(tex, coord + texelSize * vec2(-0.5,  0.5)).rgb;
    vec3 j = texture(tex, coord + texelSize * vec2( 0.5,  0.5)).rgb;
    vec3 k = texture(tex, coord + texelSize * vec2(-1.0,  1.0)).rgb;
    vec3 l = texture(tex, coord + texelSize * vec2( 0.0,  1.0)).rgb;
    vec3 m = texture(tex, coord + texelSize * vec2( 1.0,  1.0)).rgb;

    vec2 div = (1.0 / 4.0) * vec2(0.5, 0.125);

    vec3 o = (d + e + i + j) * div.x;
    o += (a + b + g + f) * div.y;
    o += (b + c + h + g) * div.y;
    o += (f + g + l + k) * div.y;
    o += (g + h + m + l) * div.y;
    return o;
}

vec3 upsampleTent(in sampler2D tex, vec2 coord, vec2 texelSize, vec2 radius) {
    vec4 d = texelSize.xyxy * vec4(1.0, 1.0, -1.0, 0.0) * radius.xyxy;

    vec3 s;
    s =  texture(tex, coord - d.xy).rgb;
    s += texture(tex, coord - d.wy).rgb * 2.0;
    s += texture(tex, coord - d.zy).rgb;

    s += texture(tex, coord + d.zw).rgb * 2.0;
    s += texture(tex, coord       ).rgb * 4.0;
    s += texture(tex, coord + d.xw).rgb * 2.0;

    s += texture(tex, coord + d.zy).rgb;
    s += texture(tex, coord + d.wy).rgb * 2.0;
    s += texture(tex, coord + d.xy).rgb;

    return s * (1.0 / 16.0);
}

vec3 downsampleImpl2() {
    vec3 finalColour = downsampleBox13Tap(srcTexture, fs_texture, texelSize * 1);
    if (passIndex == 0) {
        finalColour *= exposureBufferHeader.exposure;
        finalColour = applyBloomThreshold(finalColour);
        float luminance0 = dot(finalColour, RGB_LUMINANCE);
        finalColour *= 1.0 / (1.0 + luminance0 * 1.0 / maxBrightness);
//        finalColour = min(finalColour, maxBrightness);
    }
    return finalColour;
}

void downsampleStage() {
    vec3 finalColour;
//    finalColour = downsampleImpl1();
    finalColour = downsampleImpl2();
    outColor = vec4(finalColour, 1.0);
}

void upsampleStage() {
    vec3 finalColour;
//    finalColour = texture(srcTexture, fs_texture).rgb;

    finalColour = upsampleTent(srcTexture, fs_texture, texelSize, vec2(filterRadius));

    outColor = vec4(finalColour, 1.0);
}