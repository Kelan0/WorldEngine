#version 450

layout (local_size_x = 16) in;
layout (local_size_y = 16) in;
layout (local_size_z = 1) in;

layout(push_constant) uniform PC1 {
    uvec2 srcSize;
    uvec2 dstSize;
    float blurRadius;
    uint blurDirection;
};

layout(set = 0, binding = 0) uniform sampler2D srcImage;
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2D dstImage;

// const float halfKernel_9x1[5] = float[](0.382928, 0.241732, 0.060598, 0.005977, 0.000229); // sigma=1
const float halfKernel_9x1[5] = float[](0.20236, 0.179044, 0.124009, 0.067234, 0.028532); // sigma=2

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);

    if (invocation.x >= dstSize.x || invocation.y >= dstSize.y)
        return;

    vec2 textureCoord = (vec2(invocation.xy) + vec2(0.5)) / vec2(srcSize);
    vec2 textureOffset = blurDirection == 0 
                ? vec2(1.0 / float(srcSize.x), 0.0) 
                : vec2(0.0, 1.0 / float(srcSize.y));

    vec3 texSample = texture(srcImage, textureCoord).rgb;
    vec3 finalColour = vec3(texSample.rg * halfKernel_9x1[0], texSample.b);

    for (int i = 1; i < 5; ++i) {
        texSample = texture(srcImage, textureCoord + textureOffset * i).rgb;
        finalColour.rg += texSample.rg * halfKernel_9x1[i];
        finalColour.b = max(finalColour.b, texSample.b);
        
        texSample = texture(srcImage, textureCoord - textureOffset * i).rgb;
        finalColour.rg += texSample.rg * halfKernel_9x1[i];
        finalColour.b = max(finalColour.b, texSample.b);
    }

    imageStore(dstImage, ivec2(invocation), vec4(finalColour, 0.0));
}