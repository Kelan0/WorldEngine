#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    vec2 texelSize;
    float filterRadius;
    uint textureIndex;
};

layout(set = 0, binding = 1) uniform sampler2D srcTexture[];

void main() {
    float r = 0.008;
    vec3 a = texture(srcTexture[textureIndex], vec2(fs_texture.x - r, fs_texture.y + r)).rgb;
    vec3 b = texture(srcTexture[textureIndex], vec2(fs_texture.x,     fs_texture.y + r)).rgb;
    vec3 c = texture(srcTexture[textureIndex], vec2(fs_texture.x + r, fs_texture.y + r)).rgb;

    vec3 d = texture(srcTexture[textureIndex], vec2(fs_texture.x - r, fs_texture.y)).rgb;
    vec3 e = texture(srcTexture[textureIndex], vec2(fs_texture.x,     fs_texture.y)).rgb;
    vec3 f = texture(srcTexture[textureIndex], vec2(fs_texture.x + r, fs_texture.y)).rgb;

    vec3 g = texture(srcTexture[textureIndex], vec2(fs_texture.x - r, fs_texture.y - r)).rgb;
    vec3 h = texture(srcTexture[textureIndex], vec2(fs_texture.x,     fs_texture.y - r)).rgb;
    vec3 i = texture(srcTexture[textureIndex], vec2(fs_texture.x + r, fs_texture.y - r)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    vec3 finalColour = e * 4.0 +
            (b + d + f + h) * 2.0 +
            (a + c + g + i);
    finalColour *= 1.0 / 16.0;
    outColor = vec4(finalColour, 1.0);
}