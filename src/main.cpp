
#include "core/Application.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

int main(int argc, char* argv[]) {

    Application::create();
    Application::instance()->start();
    Application::destroy();
    return 0;
}