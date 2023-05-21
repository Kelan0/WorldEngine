
#include "demo/BloomTestApplication.h"
#include "demo/RenderStressTestApplication.h"
#include "demo/TerrainTestApplication.h"
#include <iostream>

int main(int argc, char* argv[]) {
    char* buffer = new char[1024 * 1024];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

//    return Application::create<RenderStressTestApplication>(argc, argv);
//    return Application::create<BloomTestApplication>(argc, argv);
    return Application::create<TerrainTestApplication>(argc, argv);
}