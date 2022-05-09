#version 450

layout (local_size_x = 1) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

#include "res/shaders/common/common.glsl"

layout(set = 0, binding = 0) uniform UBO1 {
    uvec2 srcMapSize;
    uvec2 dstMapSize;
};

layout(set = 0, binding = 1) uniform samplerCube srcEnvironmentMap;
layout(set = 0, binding = 2, rgba32f) uniform writeonly imageCube dstDiffuseIrradianceMap;

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);
    vec2 textureCoord = (vec2(invocation.xy + 0.5)) / vec2(dstMapSize);
    
    const float dt = (1.0 / 250.0);// * (2.0 * PI);

    const vec3 normal = getCubemapRay(textureCoord, invocation.z);


    vec3 irradiance = vec3(0.0);  

    vec3 right = normalize(getPerpendicularVector(normal));
    vec3 up = normalize(cross(normal, right));

    float sampleDelta = 0.0025;
    float sampleCount = 0.0; 
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            vec3 tangentSample = vec3(sinTheta * cosPhi,  sinTheta * sinPhi, cosTheta);
            
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

            irradiance += texture(srcEnvironmentMap, sampleVec).rgb * cosTheta * sinTheta;
            sampleCount++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(sampleCount));


    imageStore(dstDiffuseIrradianceMap, ivec3(invocation), vec4(irradiance, 1.0));
    // imageStore(dstDiffuseIrradianceMap, dstCoord, texture(srcEnvironmentMap, normal));
}