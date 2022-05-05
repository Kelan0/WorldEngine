#version 450

layout (local_size_x = 16) in;
layout (local_size_y = 16) in;
layout (local_size_z = 1) in;

#define FACE_POS_X = 0,
#define FACE_NEG_X = 1,
#define FACE_POS_Y = 2,
#define FACE_NEG_Y = 3,
#define FACE_POS_Z = 4,
#define FACE_NEG_Z = 5,


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
    ivec2 faceSize;
    ivec2 sourceSize;
};

layout(set = 0, binding = 1, rgba32f) uniform readonly imageBuffer srcEquirectangularImageBuffer;
layout(set = 0, binding = 2, rgba32f) uniform writeonly imageBuffer dstCubemapLayersImageBuffer;

// layout(binding = 0, rgba32f) uniform readonly image2D srcImage;
// layout(binding = 1) uniform writeonly imageCube dstImage;

ivec3 getCubemapCoordinate(vec3 ray) {
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

    return ivec3(faceSize.x * s, faceSize.y * t, layer);
}

ivec2 getEquirectangularCoordinate(vec3 ray, ivec2 size) {
    return ivec2((0.5 + (vec2(atan(ray.z, ray.x), asin(-ray.y)) * invAtan)) * size);
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

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 textureCoord = (vec2(pixelCoord) + vec2(0.5)) / vec2(faceSize);

    for (int i = 0; i < 6; i++) {
        vec3 ray = calculateRay(textureCoord, i);
        
        ivec2 srcCoord = getEquirectangularCoordinate(ray, sourceSize);
        ivec3 dstCoord = getCubemapCoordinate(ray);

        int srcTexelIndex = int(srcCoord.x + srcCoord.y * sourceSize.x);
        int dstTexelIndex = int(dstCoord.x + dstCoord.y * faceSize.x + dstCoord.z * faceSize.x * faceSize.y);

        vec4 srcColour = imageLoad(srcEquirectangularImageBuffer, srcTexelIndex);
        imageStore(dstCubemapLayersImageBuffer, dstTexelIndex, srcColour);
    }
}