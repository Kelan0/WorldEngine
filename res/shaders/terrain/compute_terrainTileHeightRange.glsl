#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (local_size_x = 16) in;
layout (local_size_y = 16) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    ivec2 dstResolution;
    ivec2 tileOffset;
    uint level;
    uint srcImageIndex;
    uint dstImageIndex;
};

layout(set = 0, binding = 0) uniform sampler2D baseImage;
layout(set = 1, binding = 0, rg32f) uniform readonly image2D srcHeightmapImage[];
layout(set = 1, binding = 1, rg32f) uniform writeonly image2D dstHeightmapImage[];

void main() {
    ivec2 dstPos = ivec2(gl_GlobalInvocationID.xy);

    if (dstPos.x >= dstResolution.x || dstPos.y >= dstResolution.y)
        return;

    ivec2 srcPos = tileOffset + dstPos * 2;

    vec2 h00, h01, h10, h11;
    float minElevation, maxElevation;

    if (level == 0) {
        h00 = texelFetch(baseImage, srcPos.xy, 0).xx;
        h01 = texelFetch(baseImage, srcPos.xy + ivec2(0, 1), 0).xx;
        h10 = texelFetch(baseImage, srcPos.xy + ivec2(1, 0), 0).xx;
        h11 = texelFetch(baseImage, srcPos.xy + ivec2(1, 1), 0).xx;
    } else {
        h00 = imageLoad(srcHeightmapImage[srcImageIndex], srcPos.xy).xy;
        h01 = imageLoad(srcHeightmapImage[srcImageIndex], srcPos.xy + ivec2(0, 1)).xy;
        h10 = imageLoad(srcHeightmapImage[srcImageIndex], srcPos.xy + ivec2(1, 0)).xy;
        h11 = imageLoad(srcHeightmapImage[srcImageIndex], srcPos.xy + ivec2(1, 1)).xy;
    }

    minElevation = min(min(h00.x, h01.x), min(h10.x, h11.x));
    maxElevation = max(max(h00.y, h01.y), max(h10.y, h11.y));

    imageStore(dstHeightmapImage[dstImageIndex], dstPos, vec4(minElevation, maxElevation, 0.0, 0.0));
}
