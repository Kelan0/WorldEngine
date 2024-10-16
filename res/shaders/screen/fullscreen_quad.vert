#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec2 fs_texture;

void main() {
    vec2 vertices[3] = vec2[3](
        vec2(-1.0, -1.0), 
        vec2(+3.0, -1.0), 
        vec2(-1.0, +3.0)
    );


    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
    fs_texture = gl_Position.xy * 0.5 + vec2(0.5);
    fs_texture.y = 1.0 - fs_texture.y;
}