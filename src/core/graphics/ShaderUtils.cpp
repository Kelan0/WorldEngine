#include "core/graphics/ShaderUtils.h"

#include <fstream>
#include <filesystem>
#include "core/application/Application.h"

#ifndef GLSL_COMPILER_EXECUTABLE
#define GLSL_COMPILER_EXECUTABLE "D:/Code/VulkanSDK/1.2.198.1/Bin/glslc.exe"
#endif

bool ShaderUtils::loadShaderStage(const ShaderStage& shaderStage, std::string filePath, std::vector<char>& bytecode) {

#if defined(ALWAYS_RELOAD_SHADERS)
    constexpr bool alwaysReloadShaders = true;
#else
    constexpr bool alwaysReloadShaders = true;
#endif

    // TODO: determine if source file is GLSL or HLSL and call correct compiler

    if (!filePath.ends_with(".spv")) {

        std::string outputFilePath = filePath + ".spv";

        bool shouldCompile = false;

        if (alwaysReloadShaders) {
            shouldCompile = true;
        } else {
            if (!std::filesystem::exists(outputFilePath)) {
                // Compiled file does not exist.

                if (!std::filesystem::exists(filePath)) {
                    // Source file does not exist
                    printf("Shader source file \"%s\" was not found\n", filePath.c_str());
                    return false;
                }

                shouldCompile = true;

            } else if (std::filesystem::exists(filePath)) {
                // Compiled file and source file both exist

                auto lastModifiedSource = std::filesystem::last_write_time(filePath);
                auto lastCompiled = std::filesystem::last_write_time(outputFilePath);

                if (lastModifiedSource > lastCompiled) {
                    // Source file was modified after the shader was last compiled
                    shouldCompile = true;
                }
            }
        }

        if (shouldCompile) {
            printf("Compiling shader file \"%s\"\n", filePath.c_str());

            std::string command(GLSL_COMPILER_EXECUTABLE);

            command +=
                    shaderStage == ShaderStage_VertexShader ? " -fshader-stage=vert" :
                    shaderStage == ShaderStage_FragmentShader ? " -fshader-stage=frag" :
                    shaderStage == ShaderStage_TessellationControlShader ? " -fshader-stage=tesc" :
                    shaderStage == ShaderStage_TessellationEvaluationShader ? " -fshader-stage=tese" :
                    shaderStage == ShaderStage_GeometryShader ? " -fshader-stage=geom" :
                    shaderStage == ShaderStage_ComputeShader ? " -fshader-stage=comp" : "";

            command += std::string(" \"") + filePath + "\" -o \"" + outputFilePath + "\"";

            command += std::string(" -I \"") + Application::instance()->getExecutionDirectory() + "\"";

            if (!runCommand(command)) {
                printf("Could not execute SPIR-V compile command. Provide a pre-compiled .spv file instead\n");
                return false;
            }
        }

        filePath = outputFilePath;
    }

    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        printf("Shader file \"%s\" was not found\n", filePath.c_str());
        return false;
    }

    bytecode.resize(file.tellg());
    file.seekg(0);
    file.read(bytecode.data(), bytecode.size());
    file.close();

    return true;
}

bool ShaderUtils::loadShaderModule(const ShaderStage& shaderStage, const vk::Device& device, const std::string& filePath, vk::ShaderModule* outShaderModule) {
    std::vector<char> shaderBytecode;
    if (!ShaderUtils::loadShaderStage(shaderStage, filePath, shaderBytecode)) {
        printf("Failed to load shader stage bytecode from file \"%s\"\n", filePath.c_str());
        return false;
    }
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.setCodeSize(shaderBytecode.size());
    shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(shaderBytecode.data()));
    vk::Result result = device.createShaderModule(&shaderModuleCreateInfo, nullptr, outShaderModule);
    if (result != vk::Result::eSuccess) {
        printf("Failed to load shader module (file %s): %s\n", filePath.c_str(), vk::to_string(result).c_str());
        return false;
    }
    return true;
}

bool ShaderUtils::runCommand(const std::string& command) {
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
    int error = system(command.c_str());
    if (error != 0) {
        return false;
    }

    return true;
#else
    printf("Unable to execute commands on unsupported platform\n");
#endif

    return false;
}