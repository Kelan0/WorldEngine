#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    bool bloomEnabled;
    float bloomIntensity;
};

layout(set = 0, binding = 1) uniform sampler2D frameTexture;
layout(set = 0, binding = 2) uniform sampler2D bloomTexture;

void main() {
    vec3 finalColour = texture(frameTexture, fs_texture).rgb;

    if (bloomEnabled) {
        vec3 bloomColour = textureLod(bloomTexture, fs_texture, 0).rgb;
        finalColour = mix(finalColour, bloomColour, bloomIntensity);
    }

    finalColour = finalColour / (finalColour + vec3(1.0));
    finalColour = pow(finalColour, vec3(1.0 / 2.2));

    outColor = vec4(finalColour, 1.0);
}