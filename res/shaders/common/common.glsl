#ifndef _COMMON_GLSL
#define _COMMON_GLSL


const float PI = 3.14159265359;
const float SQRT2 = 1.4142135623730951;
const vec2 invAtan = vec2(0.15915494309, 0.31830988618);

const float VELOCITY_PRECISION_SCALE = 100.0;

const vec3 cubeCornerVertices[24] = vec3[](
    vec3(+1.0, +1.0, +1.0), vec3(+1.0, +1.0, -1.0), vec3(+1.0, -1.0, -1.0), vec3(+1.0, -1.0, +1.0), // +x
    vec3(-1.0, +1.0, -1.0), vec3(-1.0, +1.0, +1.0), vec3(-1.0, -1.0, +1.0), vec3(-1.0, -1.0, -1.0), // -x
    vec3(-1.0, +1.0, -1.0), vec3(+1.0, +1.0, -1.0), vec3(+1.0, +1.0, +1.0), vec3(-1.0, +1.0, +1.0), // +y
    vec3(-1.0, -1.0, +1.0), vec3(+1.0, -1.0, +1.0), vec3(+1.0, -1.0, -1.0), vec3(-1.0, -1.0, -1.0), // -y
    vec3(-1.0, +1.0, +1.0), vec3(+1.0, +1.0, +1.0), vec3(+1.0, -1.0, +1.0), vec3(-1.0, -1.0, +1.0), // +z
    vec3(+1.0, +1.0, -1.0), vec3(-1.0, +1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3(+1.0, -1.0, -1.0)  // -z
);



vec3 getCubemapCoordinate(in vec3 ray) {
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

vec2 getEquirectangularCoordinate(in vec3 ray, in ivec2 size) {
    return vec2((0.5 + (vec2(atan(ray.z, ray.x), asin(-ray.y)) * invAtan)) * size);
}

vec3 getCubemapRay(in vec2 textureCoord, in int face) {
    vec3 v00 = cubeCornerVertices[face * 4 + 0];
    vec3 v10 = cubeCornerVertices[face * 4 + 1];
    vec3 v11 = cubeCornerVertices[face * 4 + 2];
    vec3 v01 = cubeCornerVertices[face * 4 + 3];
    vec3 vy0 = mix(v00, v10, textureCoord.x);
    vec3 vy1 = mix(v01, v11, textureCoord.x);
    return normalize(mix(vy0, vy1, textureCoord.y)); // normalized interpolated coordinate on unit cube
}

vec3 getViewRay(in vec2 screenPos, in mat4 cameraRays) {
    vec3 v00 = vec3(cameraRays[0]);
    vec3 v10 = vec3(cameraRays[1]);
    vec3 v11 = vec3(cameraRays[2]);
    vec3 v01 = vec3(cameraRays[3]);
    vec3 vy0 = mix(v00, v10, screenPos.x);
    vec3 vy1 = mix(v01, v11, screenPos.x);
    vec3 vxy = mix(vy0, vy1, screenPos.y);
    return normalize(vxy);
}

vec3 getPerpendicularVector(in vec3 v) {
    vec3 v1 = abs(v.z) < 0.999 
            ? vec3(0.0, 0.0, 1.0)
            : vec3(1.0, 0.0, 0.0);
    return cross(v1, v);
}

#endif