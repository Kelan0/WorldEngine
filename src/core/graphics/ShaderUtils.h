#ifndef WORLDENGINE_SHADERUTILS_H
#define WORLDENGINE_SHADERUTILS_H

#include "core/core.h"

struct ShaderLoadedEvent {
    std::string filePath;
    std::string entryPoint;
    bool reloaded;
};

namespace ShaderUtils {
    enum ShaderStage {
        ShaderStage_Auto = 0,
        ShaderStage_VertexShader = 1,
        ShaderStage_FragmentShader = 2,
        ShaderStage_TessellationControlShader = 3,
        ShaderStage_TessellationEvaluationShader = 4,
        ShaderStage_GeometryShader = 5,
        ShaderStage_ComputeShader = 6,
    };
    bool loadShaderStage(const ShaderStage& shaderStage, std::string filePath, std::string entryPoint, std::vector<char>* bytecode);

    bool loadShaderModule(const ShaderStage& shaderStage, const vk::Device& device, const std::string& filePath, const std::string& entryPoint, vk::ShaderModule* outShaderModule);
};


#endif //WORLDENGINE_SHADERUTILS_H
