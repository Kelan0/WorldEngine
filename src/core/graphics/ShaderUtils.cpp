#include "core/graphics/ShaderUtils.h"

#include <fstream>
#include <filesystem>
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/util/Util.h"

#ifndef GLSL_COMPILER_EXECUTABLE
// TODO: define this as a program argument, or define in CMakeLists.txt
// Switching Vulkan version and forgetting to update this caused a real headache.
#define GLSL_COMPILER_EXECUTABLE "C:/VulkanSDK/1.3.224.1/Bin/glslc.exe"
#endif


struct LoadedShaderInfo {
    std::string filePath;
    std::vector<char> bytecode;
    ShaderUtils::ShaderStage stage;
    std::filesystem::file_time_type fileLoadedTime;
    bool shouldReload;
    bool isValidShader;
};


class ShaderLoadingUpdater {
private:
    ShaderLoadingUpdater();

    ~ShaderLoadingUpdater();

public:
    static ShaderLoadingUpdater* instance();

    LoadedShaderInfo* notifyShaderLoaded(const LoadedShaderInfo& shaderInfo);

    LoadedShaderInfo* getLoadedShaderInfo(const std::string& filePath);

private:
    void checkModifiedShaders();

private:
    std::unordered_map<std::string, LoadedShaderInfo> m_loadedShaders;
    TimerId m_checkShadersInterval;
};

ShaderLoadingUpdater::ShaderLoadingUpdater() {
    m_checkShadersInterval = Engine::eventDispatcher()->setInterval([this](IntervalEvent* interval) {
        this->checkModifiedShaders();
    }, 1000);
}

ShaderLoadingUpdater::~ShaderLoadingUpdater() {

}

ShaderLoadingUpdater* ShaderLoadingUpdater::instance() {
    static auto s_instance = new ShaderLoadingUpdater();
    return s_instance;
}

LoadedShaderInfo* ShaderLoadingUpdater::notifyShaderLoaded(const LoadedShaderInfo& shaderInfo) {
    auto [it, inserted] = m_loadedShaders.insert(std::make_pair(shaderInfo.filePath, shaderInfo));
    LoadedShaderInfo* newShaderInfo = &it->second;
    if (!inserted) {
        // This shouldn't happen, but if it does, the existing entry is updated with the new data.
        newShaderInfo->filePath = shaderInfo.filePath;
        newShaderInfo->bytecode = shaderInfo.bytecode;
        newShaderInfo->stage = shaderInfo.stage;
        newShaderInfo->fileLoadedTime = shaderInfo.fileLoadedTime;
    }
    newShaderInfo->shouldReload = false;
    ShaderLoadedEvent event{};
    event.filePath = newShaderInfo->filePath;
    Engine::eventDispatcher()->trigger(&event);
    return newShaderInfo;
}

LoadedShaderInfo* ShaderLoadingUpdater::getLoadedShaderInfo(const std::string& filePath) {
    auto it = m_loadedShaders.find(filePath);
    return it == m_loadedShaders.end() ? nullptr : &it->second;
}

void ShaderLoadingUpdater::checkModifiedShaders() {
    for (auto& [filePath, shaderInfo] : m_loadedShaders) {
        if (std::filesystem::last_write_time(filePath) < shaderInfo.fileLoadedTime) {
            continue; // Shader was previously loaded after it was previously modified
        }

        printf("Reloading shader %s\n", filePath.c_str());
        shaderInfo.shouldReload = true;

        if (!ShaderUtils::loadShaderStage(shaderInfo.stage, filePath, nullptr)) {
            printf("Failed to reload shader %s\n", filePath.c_str());
            continue;
        }
    }
}







bool ShaderUtils::loadShaderStage(const ShaderStage& shaderStage, std::string filePath, std::vector<char>* bytecode) {

    Util::trim(filePath);
    LoadedShaderInfo* loadedShaderInfo = ShaderLoadingUpdater::instance()->getLoadedShaderInfo(filePath);
    if (loadedShaderInfo != nullptr && !loadedShaderInfo->shouldReload) {
        if (bytecode != nullptr)
            *bytecode = loadedShaderInfo->bytecode; // copy assignment
        return true;
    }

#if defined(ALWAYS_RELOAD_SHADERS)
    constexpr bool alwaysReloadShaders = true;
#else
    constexpr bool alwaysReloadShaders = true;
#endif

    // TODO: determine if source file is GLSL or HLSL and call correct compiler

    std::string outputFilePath = filePath;

    bool isValidShader = true; // By default, we assume the shader is valid. This is overwritten if the shader gets recompiled.

    if (!outputFilePath.ends_with(".spv")) {

        outputFilePath += ".spv";

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

            isValidShader = runCommand(command);
            if (!isValidShader) {
                printf("SPIR-V compile command failed\n");
            }
        }
    }

    if (isValidShader) {
        std::ifstream file(outputFilePath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            printf("Shader file \"%s\" was not found\n", outputFilePath.c_str());
            return false;
        }

        LoadedShaderInfo newShaderInfo{};
        newShaderInfo.stage = shaderStage;
        newShaderInfo.filePath = filePath;
        newShaderInfo.fileLoadedTime = std::chrono::file_clock::now();
        newShaderInfo.bytecode.resize(file.tellg());
        newShaderInfo.isValidShader = isValidShader;
        file.seekg(0);
        file.read(newShaderInfo.bytecode.data(), (std::streamsize) newShaderInfo.bytecode.size());
        file.close();

        if (bytecode != nullptr)
            *bytecode = newShaderInfo.bytecode; // copy assignment

        ShaderLoadingUpdater::instance()->notifyShaderLoaded(newShaderInfo);

    } else if (loadedShaderInfo != nullptr) {
        // This shader was reloaded, but was not valid. Don't keep trying to reload it.
        loadedShaderInfo->fileLoadedTime = std::chrono::file_clock::now();
        loadedShaderInfo->shouldReload = false;
        loadedShaderInfo->isValidShader = false;
    }

    return isValidShader;
}

bool ShaderUtils::loadShaderModule(const ShaderStage& shaderStage, const vk::Device& device, const std::string& filePath, vk::ShaderModule* outShaderModule) {

    std::vector<char> bytecode;
    if (!ShaderUtils::loadShaderStage(shaderStage, filePath, &bytecode)) {
        printf("Failed to load shader stage bytecode from file \"%s\"\n", filePath.c_str());
        return false;
    }

    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.setCodeSize(bytecode.size());
    shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(bytecode.data()));
    vk::Result result = device.createShaderModule(&shaderModuleCreateInfo, nullptr, outShaderModule);
    if (result != vk::Result::eSuccess) {
        printf("Failed to load shader module (file %s): %s\n", filePath.c_str(), vk::to_string(result).c_str());
        return false;
    }
    Engine::graphics()->setObjectName(device, (uint64_t)(VkShaderModule)(*outShaderModule), vk::ObjectType::eShaderModule, filePath.c_str());

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