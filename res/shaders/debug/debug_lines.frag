#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;
layout(location = 1) in vec4 fs_colour;

layout(location = 0) out vec4 outColour;

layout(set = 0, binding = 0) uniform UBO1 {
    mat4 modelViewMatrix;
    mat4 projectionMatrix;
    vec2 resolution;
    bool depthTestEnabled;
};

layout(set = 0, binding = 1) uniform sampler2D frameDepthTexture;

void main() {
    vec2 pos = gl_FragCoord.xy / resolution;
    float frameDepth = texture(frameDepthTexture, pos).x;
    float fragDepth = gl_FragCoord.z;
    if (depthTestEnabled) {
        if (fragDepth > frameDepth) {
            discard;
        }
    }
    outColour = fs_colour;
//    outColour = vec4(frameDepth < fragDepth ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 1.0), 0.8);
//    outColour = vec4(vec2(fract(pos)), 0.0, 1.0);
}