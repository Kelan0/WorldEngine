#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec4 outShadowData;

void main() {
    float z = gl_FragCoord.z;
    vec2 d = vec2(dFdx(z), dFdy(z));
    float z2 = (z * z) + (0.25 * dot(d, d));
    // float z2 = z * z;
    outShadowData = vec4(z, z2, z, 0.0);
}