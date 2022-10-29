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
    std::string entryPoint;
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

    LoadedShaderInfo* notifyShaderLoaded(const LoadedShaderInfo& shaderInfo, const bool& reloaded);

    LoadedShaderInfo* getLoadedShaderInfo(const std::string& filePath, const std::string& entryPoint);

private:
    void checkModifiedShaders();

    std::string getShaderKey(const std::string& filePath, const std::string& entryPoint);

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

LoadedShaderInfo* ShaderLoadingUpdater::notifyShaderLoaded(const LoadedShaderInfo& shaderInfo, const bool& reloaded) {
    std::string key = getShaderKey(shaderInfo.filePath, shaderInfo.entryPoint);
    auto [it, inserted] = m_loadedShaders.insert(std::make_pair(key, shaderInfo));
    LoadedShaderInfo* newShaderInfo = &it->second;
    if (!inserted) {
        // This shouldn't happen, but if it does, the existing entry is updated with the new data.
        newShaderInfo->filePath = shaderInfo.filePath;
        newShaderInfo->entryPoint = shaderInfo.entryPoint;
        newShaderInfo->bytecode = shaderInfo.bytecode;
        newShaderInfo->stage = shaderInfo.stage;
        newShaderInfo->fileLoadedTime = shaderInfo.fileLoadedTime;
    }
    ShaderLoadedEvent event{};
    event.filePath = newShaderInfo->filePath;
    event.entryPoint = newShaderInfo->entryPoint;
    event.reloaded = reloaded;
//    printf("======== ======== DISPATCH ShaderLoadedEvent %s@%s (%s)======== ========\n\n", event.filePath.c_str(), event.entryPoint.c_str(), reloaded ? "reloaded" : "first loaded");
    Engine::eventDispatcher()->trigger(&event);

    newShaderInfo->shouldReload = false;
    return newShaderInfo;
}

LoadedShaderInfo* ShaderLoadingUpdater::getLoadedShaderInfo(const std::string& filePath, const std::string& entryPoint) {
    std::string key = getShaderKey(filePath, entryPoint);
    auto it = m_loadedShaders.find(key);
    return it == m_loadedShaders.end() ? nullptr : &it->second;
}

void ShaderLoadingUpdater::checkModifiedShaders() {
    for (auto& [key, shaderInfo] : m_loadedShaders) {
        if (std::filesystem::last_write_time(shaderInfo.filePath) < shaderInfo.fileLoadedTime) {
            continue; // Shader was previously loaded after it was previously modified
        }

        printf("Reloading shader %s@%s\n", shaderInfo.filePath.c_str(), shaderInfo.entryPoint.c_str());
        shaderInfo.shouldReload = true;

        if (!ShaderUtils::loadShaderStage(shaderInfo.stage, shaderInfo.filePath, shaderInfo.entryPoint, nullptr)) {
            printf("Failed to reload shader %s@%s\n", shaderInfo.filePath.c_str(), shaderInfo.entryPoint.c_str());
            continue;
        }
    }
}

std::string ShaderLoadingUpdater::getShaderKey(const std::string& filePath, const std::string& entryPoint) {
    return filePath + '@' + entryPoint;
}







bool ShaderUtils::loadShaderStage(const ShaderStage& shaderStage, std::string filePath, std::string entryPoint, std::vector<char>* bytecode) {

    Util::trim(filePath);
    Util::trim(entryPoint);
    if (entryPoint.empty()) {
        printf("Cannot compile shader \"%s\": Entry point is not specified\n", filePath.c_str());
        return false;
    }

    if (entryPoint.find(' ') != std::string::npos) {
        printf("Cannot compile shader \"%s\" with specified entry point \"%s\": The entry point must not contain spaces\n", filePath.c_str(), entryPoint.c_str());
        return false;
    }

    LoadedShaderInfo* loadedShaderInfo = ShaderLoadingUpdater::instance()->getLoadedShaderInfo(filePath, entryPoint);
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
            printf("Compiling shader: %s@%s\n", filePath.c_str(), entryPoint.c_str());

            std::string command(GLSL_COMPILER_EXECUTABLE);

            command +=
                    shaderStage == ShaderStage_VertexShader ? " -fshader-stage=vert" :
                    shaderStage == ShaderStage_FragmentShader ? " -fshader-stage=frag" :
                    shaderStage == ShaderStage_TessellationControlShader ? " -fshader-stage=tesc" :
                    shaderStage == ShaderStage_TessellationEvaluationShader ? " -fshader-stage=tese" :
                    shaderStage == ShaderStage_GeometryShader ? " -fshader-stage=geom" :
                    shaderStage == ShaderStage_ComputeShader ? " -fshader-stage=comp" : "";

            command += " -D" + entryPoint + "=main";
//            command += " -fentry-point=" + entryPoint;

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
        newShaderInfo.entryPoint = entryPoint;
        newShaderInfo.fileLoadedTime = std::chrono::file_clock::now();
        newShaderInfo.bytecode.resize(file.tellg());
        newShaderInfo.isValidShader = isValidShader;
        file.seekg(0);
        file.read(newShaderInfo.bytecode.data(), (std::streamsize) newShaderInfo.bytecode.size());
        file.close();

        if (bytecode != nullptr)
            *bytecode = newShaderInfo.bytecode; // copy assignment

        ShaderLoadingUpdater::instance()->notifyShaderLoaded(newShaderInfo, loadedShaderInfo != nullptr && loadedShaderInfo->shouldReload);

    } else if (loadedShaderInfo != nullptr) {
        // This shader was reloaded, but was not valid. Don't keep trying to reload it.
        loadedShaderInfo->fileLoadedTime = std::chrono::file_clock::now();
        loadedShaderInfo->shouldReload = false;
        loadedShaderInfo->isValidShader = false;
    }

    return isValidShader;
}

bool ShaderUtils::loadShaderModule(const ShaderStage& shaderStage, const vk::Device& device, const std::string& filePath, const std::string& entryPoint, vk::ShaderModule* outShaderModule) {

    std::vector<char> bytecode;
    if (!ShaderUtils::loadShaderStage(shaderStage, filePath, entryPoint, &bytecode)) {
        printf("Failed to load shader stage bytecode from file \"%s\" with entry point \"%s\"\n", filePath.c_str(), entryPoint.c_str());
        return false;
    }

    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.setCodeSize(bytecode.size());
    shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(bytecode.data()));
    vk::Result result = device.createShaderModule(&shaderModuleCreateInfo, nullptr, outShaderModule);
    if (result != vk::Result::eSuccess) {
        printf("Failed to load shader module (file %s, entryPoint %s): %s\n", filePath.c_str(), entryPoint.c_str(), vk::to_string(result).c_str());
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