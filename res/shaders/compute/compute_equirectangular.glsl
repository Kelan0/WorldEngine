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
    ivec2 sampleCount;
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

vec2 getEquirectangularCoordinate(vec3 ray, ivec2 size) {
    return vec2((0.5 + (vec2(atan(ray.z, ray.x), asin(-ray.y)) * invAtan)) * size);
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

vec4 loadSourceTexel(in vec2 coord) {
    int texelIndex00 = int(coord.x) + int(coord.y) * int(sourceSize.x);
    vec4 colour00 = imageLoad(srcEquirectangularImageBuffer, texelIndex00);

    vec2 subpixel = fract(coord);
    if (subpixel.x < 1e-3 && subpixel.y < 1e-3) {
        return colour00;
    } else {

        int texelIndex01 = int(coord.x) + int(coord.y + 1) * int(sourceSize.x);
        int texelIndex11 = int(coord.x + 1) + int(coord.y + 1) * int(sourceSize.x);
        int texelIndex10 = int(coord.x + 1) + int(coord.y) * int(sourceSize.x);

        vec4 colour01 = imageLoad(srcEquirectangularImageBuffer, texelIndex01);
        vec4 colour11 = imageLoad(srcEquirectangularImageBuffer, texelIndex11);
        vec4 colour10 = imageLoad(srcEquirectangularImageBuffer, texelIndex10);
        
        vec4 colourY0 = mix(colour00, colour10, subpixel.x);
        vec4 colourY1 = mix(colour01, colour11, subpixel.x);
        return mix(colourY0, colourY1, subpixel.y);
    }
}

void main() {
    ivec3 pixelCoord = ivec3(gl_GlobalInvocationID.xyz);
    if (pixelCoord.x >= faceSize.x || pixelCoord.y >= faceSize.y || pixelCoord.z >= 6)
        return;

    vec4 colour = vec4(0.0);
    float counter = 0.0;

    ivec2 numSamples = clamp(sampleCount, 1, 64);

    vec2 inverseFaceSize = vec2(1.0) / vec2(faceSize);
    vec2 sampleOffset = vec2(1.0) / vec2(numSamples);
    vec2 subpixelCoord;
    int i, j;

    for (i = 0, subpixelCoord.y = pixelCoord.y + sampleOffset.y * 0.5; i < numSamples.y; ++i, subpixelCoord.y += sampleOffset.y) {
        for (j = 0, subpixelCoord.x = pixelCoord.x + sampleOffset.x * 0.5; j < numSamples.x; ++j, subpixelCoord.x += sampleOffset.x) {
            vec3 ray = calculateRay(subpixelCoord * inverseFaceSize, pixelCoord.z);
            colour += loadSourceTexel(getEquirectangularCoordinate(ray, sourceSize));
            counter += 1.0;
        }
    }    

    colour /= counter;


    vec3 centerRay = calculateRay(vec2(pixelCoord.x + 0.5, pixelCoord.y + 0.5) / vec2(faceSize), pixelCoord.z);
    ivec3 dstCoord = getCubemapCoordinate(centerRay);
    int dstTexelIndex = int(dstCoord.x + dstCoord.y * faceSize.x + dstCoord.z * faceSize.x * faceSize.y);

    imageStore(dstCubemapLayersImageBuffer, dstTexelIndex, colour);
}