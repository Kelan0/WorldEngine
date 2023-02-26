#include "core/graphics/ShaderUtils.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/util/Util.h"

struct DependencyFileInfo {
    std::string filePath;
    std::filesystem::file_time_type lastCheckTime;
    std::set<std::string> dependentShaderKeys;
};

struct LoadedShaderInfo {
    std::string filePath;
    std::string entryPoint;
    std::vector<char> bytecode;
    ShaderUtils::ShaderStage stage;
    std::filesystem::file_time_type fileLoadedTime;
    std::vector<std::string> dependencyFilePaths;
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
    std::unordered_map<std::string, DependencyFileInfo> m_loadedDependencies;
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
    auto [it0, inserted0] = m_loadedShaders.insert(std::make_pair(key, shaderInfo));
    LoadedShaderInfo* newShaderInfo = &it0->second;
    if (!inserted0) {
        // This shouldn't happen, but if it does, the existing entry is updated with the new data.
        newShaderInfo->filePath = shaderInfo.filePath;
        newShaderInfo->entryPoint = shaderInfo.entryPoint;
        newShaderInfo->bytecode = shaderInfo.bytecode; // vector copy
        newShaderInfo->stage = shaderInfo.stage;
        newShaderInfo->fileLoadedTime = shaderInfo.fileLoadedTime;
        newShaderInfo->dependencyFilePaths = shaderInfo.dependencyFilePaths; // vector copy
    }

    for (const std::string& dependencyFilePath : shaderInfo.dependencyFilePaths) {
        auto [it1, inserted1] = m_loadedDependencies.insert(std::make_pair(dependencyFilePath, DependencyFileInfo{}));
        DependencyFileInfo& dependencyInfo = it1->second;
        if (inserted1) {
            dependencyInfo.filePath = dependencyFilePath;
            dependencyInfo.lastCheckTime = std::chrono::file_clock::now();
        }
        dependencyInfo.dependentShaderKeys.insert(key);
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
    for (auto& [filePath, dependencyInfo] : m_loadedDependencies) {
        std::string absDependencyFilePath = Application::instance()->getResourceDirectory() + dependencyInfo.filePath;
        if (std::filesystem::last_write_time(absDependencyFilePath) < dependencyInfo.lastCheckTime) {
            continue; // File was previously checked after it was previously modified
        }
        dependencyInfo.lastCheckTime = std::chrono::file_clock::now();

        if (dependencyInfo.dependentShaderKeys.empty()) {
            continue; // Nothing depends on this file. // TODO: remove it
        }

        printf("Reloading shader dependency %s with %llu dependent shaders\n", dependencyInfo.filePath.c_str(), dependencyInfo.dependentShaderKeys.size());

        for (auto it0 = dependencyInfo.dependentShaderKeys.begin(); it0 != dependencyInfo.dependentShaderKeys.end();) {
            const std::string& shaderKey = *it0;
            auto it1 = m_loadedShaders.find(shaderKey);
            if (it1 == m_loadedShaders.end()) {
                it0 = dependencyInfo.dependentShaderKeys.erase(it0);
                continue;
            }

            LoadedShaderInfo& shaderInfo = it1->second;
//            if (std::filesystem::equivalent(shaderInfo.filePath, dependencyInfo.filePath))
            shaderInfo.fileLoadedTime = std::filesystem::file_time_type::min(); // Reset loaded time to 0 so that the file is forced to be reloaded

            ++it0;
        }
    }

    for (auto& [key, shaderInfo] : m_loadedShaders) {
        std::string absFilePath = Application::instance()->getResourceDirectory() + shaderInfo.filePath;
        if (std::filesystem::last_write_time(absFilePath) < shaderInfo.fileLoadedTime) {
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




bool getShaderDependencies(const std::string shaderFilePath, const std::string& dependencyFilePath, std::vector<std::string>& outDependencyFiles) {
    std::ifstream fs(dependencyFilePath.c_str(), std::ifstream::in);
    if (!fs.is_open()) {
        printf("Failed to open shader dependencies file \"%s\"\n", dependencyFilePath.c_str());
        return false;
    }

    std::stringstream ss;
    ss << fs.rdbuf();
    std::string dependencies = ss.str();
    fs.close();

    Util::trim(dependencies);
    size_t startSize = outDependencyFiles.size();
    if (!Util::splitString(dependencies, ' ', outDependencyFiles)) {
        return true;
    }

    auto it = outDependencyFiles.begin() + startSize;
    assert(it != outDependencyFiles.end());
    it = outDependencyFiles.erase(it); // Remove the first item in the split, since it is the name of the compiled SPIR-V output file.

    const std::string& executionDir = Application::instance()->getExecutionDirectory();

    for (; it != outDependencyFiles.end();) {
        std::string& dependencyFile = *it;
        dependencyFile = std::filesystem::relative(dependencyFile, executionDir).string();
        if (std::filesystem::equivalent(dependencyFile, shaderFilePath)) {
            it = outDependencyFiles.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}

bool generateShaderDependencies(const std::string& command, const std::string& shaderFilePath, const std::string& dependencyFilePath) {

    std::string commandResponse;
    int error = Util::executeCommand(command + " -E -w -MD -MF \"" + dependencyFilePath + "\"", commandResponse);
    if (error != EXIT_SUCCESS) {
        Util::trim(commandResponse);
        commandResponse += "\n";
        printf("Failed to retrieve dependencies for shader \"%s\"\n%s", commandResponse.c_str(), shaderFilePath.c_str());
        return false;
    }

    return true;
}





bool ShaderUtils::loadShaderStage(const ShaderStage& shaderStage, std::string filePath, std::string entryPoint, std::vector<char>* bytecode) {

    Util::trim(filePath);
    Util::trim(entryPoint);


    std::string absFilePath = Application::instance()->getResourceDirectory() + filePath;

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

    std::string outputFilePath = absFilePath;
    std::string dependencyFilePath = outputFilePath + ".dep";

    bool isValidShader = true; // By default, we assume the shader is valid. This is overwritten if the shader gets recompiled.

    if (!outputFilePath.ends_with(".spv")) {

        outputFilePath += ".spv";
        dependencyFilePath = outputFilePath + ".dep";

        bool shouldCompile = false;

        if (alwaysReloadShaders) {
            shouldCompile = true;
        } else {
            if (!std::filesystem::exists(outputFilePath)) {
                // Compiled file does not exist.

                if (!std::filesystem::exists(absFilePath)) {
                    // Source file does not exist
                    printf("Shader source file \"%s\" was not found\n", filePath.c_str());
                    return false;
                }

                shouldCompile = true;

            } else if (std::filesystem::exists(absFilePath)) {
                // Compiled file and source file both exist

                auto lastModifiedSource = std::filesystem::last_write_time(absFilePath);
                auto lastCompiled = std::filesystem::last_write_time(outputFilePath);

                if (lastModifiedSource > lastCompiled) {
                    // Source file was modified after the shader was last compiled
                    shouldCompile = true;
                }
            }
        }

        if (shouldCompile) {
            printf("Compiling shader: %s@%s\n", filePath.c_str(), entryPoint.c_str());

            const std::string& glslcdir = Application::instance()->getShaderCompilerDirectory();
            if (!glslcdir.empty() && !std::filesystem::exists(glslcdir + "glslc.exe")) {
                printf("Unable to compile shader: GLSL compiler (glslc.exe) was not found in the directory \"%s\". Make sure the location of the GLSL compiler is specified correctly using the --spvcdir program argument\n",
                       Application::instance()->getShaderCompilerDirectory().c_str());
                isValidShader = false;
            } else {

                std::string command = glslcdir + "glslc.exe";
                command +=
                        shaderStage == ShaderStage_VertexShader ? " -fshader-stage=vert" :
                        shaderStage == ShaderStage_FragmentShader ? " -fshader-stage=frag" :
                        shaderStage == ShaderStage_TessellationControlShader ? " -fshader-stage=tesc" :
                        shaderStage == ShaderStage_TessellationEvaluationShader ? " -fshader-stage=tese" :
                        shaderStage == ShaderStage_GeometryShader ? " -fshader-stage=geom" :
                        shaderStage == ShaderStage_ComputeShader ? " -fshader-stage=comp" : "";

                command += " -D" + entryPoint + "=main";
//            command += " -fentry-point=" + entryPoint;

                std::string includeDirectory = Application::instance()->getResourceDirectory(); // Always includes trailing file separator
                includeDirectory = includeDirectory.substr(0, includeDirectory.size() - 1); // glsl compiler does not like trailing file separator on this file path

                command += std::string(" \"") + absFilePath + "\"";
                command += std::string(" -I \"") + includeDirectory + "\"";

                std::string commandResponse;
                int error = Util::executeCommand(command + " -o \"" + outputFilePath + "\"", commandResponse);
                isValidShader = error == EXIT_SUCCESS;
                if (!isValidShader) {
                    Util::trim(commandResponse);
                    printf("SPIR-V compile command failed\n%s\n", commandResponse.c_str());
                } else {
                    generateShaderDependencies(command, filePath, dependencyFilePath);
                }
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
        newShaderInfo.dependencyFilePaths.clear();

        if (!getShaderDependencies(absFilePath, dependencyFilePath, newShaderInfo.dependencyFilePaths)) {
            printf("Failed to get dependencies for shader \"%s\" - Modifications to any dependencies will not be reloaded\n", absFilePath.c_str());
        }

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