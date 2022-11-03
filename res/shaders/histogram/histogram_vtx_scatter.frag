#version 450

layout(location = 0) in vec4 fs_colour;

//layout(location = 0) out vec4 outColour;
layout(location = 0) out float outHist;

void main() {
    outHist = 1.0;
//    outColour = fs_colour;
}