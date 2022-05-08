#version 450

layout (local_size_x = 1) in;
layout (local_size_y = 1) in;
layout (local_size_z = 1) in;

const float PI = 3.14159265359;
const vec2 invAtan = vec2(0.15915494309, 0.31830988618);
const vec3 cubeCornerVertices[24] = vec3[](
    vec3(+1.0, -1.0, -1.0), vec3(+1.0, +1.0, -1.0), vec3(+1.0, +1.0, +1.0), vec3(+1.0, -1.0, +1.0), // +x
    vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0, +1.0), vec3(-1.0, +1.0, +1.0), vec3(-1.0, +1.0, -1.0), // -x
    vec3(-1.0, +1.0, -1.0), vec3(-1.0, +1.0, +1.0), vec3(+1.0, +1.0, +1.0), vec3(+1.0, +1.0, -1.0), // +y
    vec3(-1.0, -1.0, -1.0), vec3(+1.0, -1.0, -1.0), vec3(+1.0, -1.0, +1.0), vec3(-1.0, -1.0, +1.0), // -y
    vec3(-1.0, -1.0, +1.0), vec3(+1.0, -1.0, +1.0), vec3(+1.0, +1.0, +1.0), vec3(-1.0, +1.0, +1.0), // +z
    vec3(-1.0, -1.0, -1.0), vec3(-1.0, +1.0, -1.0), vec3(+1.0, +1.0, -1.0), vec3(+1.0, -1.0, -1.0)  // -z
);

layout(set = 0, binding = 0) uniform UBO1 {
    uvec2 srcMapSize;
    uvec2 dstMapSize;
};

layout(set = 0, binding = 1) uniform samplerCube srcEnvironmentMap;
layout(set = 0, binding = 2, rgba32f) uniform writeonly imageCube dstDiffuseIrradianceMap;

vec3 getCubemapCoordinate(vec3 ray) {
    vec3 absRay = abs(ray);
    float sc, tc, ma;
    int layer;

    if (absRay.x > absRay.y && absRay.x > absRay.z) { // major axis = x
        ma = absRay.x;
        if (ray.x > 0) 
            sc = -ray.z, tc = -ray.y, layer = 0; // positive x
        else 
            sc = ray.z, tc = -ray.y, layer = 1; // negative x
    } else if (absRay.y > absRay.z) { // major axis = y
        ma = absRay.y;
        if (ray.y > 0) 
            sc = ray.x, tc = ray.z, layer = 2; // positive y
        else 
            sc = ray.x, tc = -ray.z, layer = 3; // negative y
    } else { // major axis = z
        ma = absRay.z;
        if (ray.z > 0) 
            sc = ray.x, tc = -ray.y, layer = 4; // positive z
        else 
            sc = -ray.x, tc = -ray.y, layer = 5; // negative z
    }

    float s = (sc / ma) * 0.5 + 0.5;
    float t = (tc / ma) * 0.5 + 0.5;

    return vec3(s, t, layer);
}

vec3 calculateRay(vec2 textureCoord, int face) {
    vec3 v00 = cubeCornerVertices[face * 4 + 0];
    vec3 v10 = cubeCornerVertices[face * 4 + 1];
    vec3 v11 = cubeCornerVertices[face * 4 + 2];
    vec3 v01 = cubeCornerVertices[face * 4 + 3];
    vec3 vy0 = mix(v00, v10, textureCoord.x);
    vec3 vy1 = mix(v01, v11, textureCoord.x);
    return normalize(mix(vy0, vy1, textureCoord.y)); // normalized interpolated coordinate on unit cube
}

vec3 getPerpendicularVector(in vec3 v) {
    vec3 v1 = v.y > v.x && v.y > v.z 
            ? vec3(1.0, 0.0, 0.0)  // Y is largest component, use X-axis
            : vec3(0.0, 1.0, 0.0); // Y not largest component, use Y-axis
    return cross(v1, v);
}

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);
    vec2 textureCoord = (vec2(invocation.xy + 0.5)) / vec2(dstMapSize);
    
    const vec3 srcSize = vec3(srcMapSize, 1.0);
    const vec3 dstSize = vec3(dstMapSize, 1.0);
    const float dt = (1.0 / 250.0);// * (2.0 * PI);

    const vec3 normal = calculateRay(textureCoord, invocation.z);
    const ivec3 dstCoord = ivec3(getCubemapCoordinate(normal) * dstSize);


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


    imageStore(dstDiffuseIrradianceMap, dstCoord, vec4(irradiance, 1.0));
    // imageStore(dstDiffuseIrradianceMap, dstCoord, texture(srcEnvironmentMap, normal));
}