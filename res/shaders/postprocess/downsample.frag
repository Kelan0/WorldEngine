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
    const vec2 texelSize2 = texelSize * 2.0;
    vec3 a = texture(srcTexture[textureIndex], vec2(fs_texture.x - texelSize2.x, fs_texture.y + texelSize2.y)).rgb;
    vec3 b = texture(srcTexture[textureIndex], vec2(fs_texture.x,                fs_texture.y + texelSize2.y)).rgb;
    vec3 c = texture(srcTexture[textureIndex], vec2(fs_texture.x + texelSize2.x, fs_texture.y + texelSize2.y)).rgb;

    vec3 d = texture(srcTexture[textureIndex], vec2(fs_texture.x - texelSize2.x, fs_texture.y)).rgb;
    vec3 e = texture(srcTexture[textureIndex], vec2(fs_texture.x,                fs_texture.y)).rgb;
    vec3 f = texture(srcTexture[textureIndex], vec2(fs_texture.x + texelSize2.x, fs_texture.y)).rgb;

    vec3 g = texture(srcTexture[textureIndex], vec2(fs_texture.x - texelSize2.x, fs_texture.y - texelSize2.y)).rgb;
    vec3 h = texture(srcTexture[textureIndex], vec2(fs_texture.x,                fs_texture.y - texelSize2.y)).rgb;
    vec3 i = texture(srcTexture[textureIndex], vec2(fs_texture.x + texelSize2.x, fs_texture.y - texelSize2.y)).rgb;

    vec3 j = texture(srcTexture[textureIndex], vec2(fs_texture.x - texelSize.x, fs_texture.y + texelSize.y)).rgb;
    vec3 k = texture(srcTexture[textureIndex], vec2(fs_texture.x + texelSize.x, fs_texture.y + texelSize.y)).rgb;
    vec3 l = texture(srcTexture[textureIndex], vec2(fs_texture.x - texelSize.x, fs_texture.y - texelSize.y)).rgb;
    vec3 m = texture(srcTexture[textureIndex], vec2(fs_texture.x + texelSize.x, fs_texture.y - texelSize.y)).rgb;

    vec3 finalColour = e * 0.125 +
        (a + c + g + i) * 0.03125 +
        (b + d + f + h) * 0.0625 +
        (j + k + l + m) * 0.125;
    outColor = vec4(finalColour, 1.0);
}