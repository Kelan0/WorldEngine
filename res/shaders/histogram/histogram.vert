#version 450

#include "res/shaders/common/common.glsl"

layout(location = 0) in vec2 position;

layout(location = 0) out vec4 fs_colour;

layout(push_constant) uniform PC1 {
    uvec2 resolution;
    float maxBrightness;
    uint binCount;
    uint channel;
};

layout(set = 0, binding = 0) uniform sampler2D srcTexture;

void main() {
    vec3 colour = texture(srcTexture, position).rgb;
    colour = min(colour / maxBrightness, 1.0);

    float bin = -10.0; // Will be discard if not set

    switch (channel) {
    case 0:
        fs_colour = vec4(1.0, 0.0, 0.0, 0.0);
        bin = colour.r;
        break;
    case 1:
        fs_colour = vec4(0.0, 1.0, 0.0, 0.0);
        bin = colour.g;
        break;
    case 2:
        fs_colour = vec4(0.0, 0.0, 1.0, 0.0);
        bin = colour.b;
        break;
    case 3:
        fs_colour = vec4(0.0, 0.0, 0.0, 1.0);
        bin = dot(colour.rgb, RGB_LUMINANCE);
        break;
    }

    // Transform bin to center of output pixel
    bin = (floor(bin * (binCount - 1)) + 0.5) / float(binCount);

    float ndc_x = (bin * 2.0 - 1.0);

    gl_PointSize = 1.0;
    gl_Position = vec4(ndc_x, 0.0, 0.0, 1.0);
}