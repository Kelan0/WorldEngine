#ifndef WORLDENGINE_IMMEDIATERENDERER_H
#define WORLDENGINE_IMMEDIATERENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/engine/geometry/MeshData.h"

#define IMMEDIATE_MODE_VALIDATION 1

class DescriptorSet;
class DescriptorSetLayout;
struct RecreateSwapchainEvent;
class GraphicsPipeline;
class RenderPass;
class Framebuffer;
class ImageView;
class Image2D;

enum MatrixMode {
    MatrixMode_ModelView = 0,
    MatrixMode_Projection = 1,
};

class ImmediateRenderer {
public:
    struct ColouredVertex {
        union {
            glm::vec3 position;
            struct { float px, py, pz; };
        };
        union {
            glm::vec3 normal;
            struct { float nx, ny, nz; };
        };
        union {
            glm::vec2 texture;
            struct { float tx, ty; };
        };
        union {
            glm::u8vec4 colour;
            struct { uint8_t r, g, b, a; };
            uint32_t rgba;
        };

        ColouredVertex();

        ColouredVertex(const ColouredVertex& copy);

        ColouredVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture, const glm::u8vec4& colour = glm::u8vec4(255));

        ColouredVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture, const glm::vec4& colour = glm::vec4(1.0F));

        ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);

        ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, float r = 1.0F, float g = 1.0F, float b = 1.0F, float a = 1.0F);

        bool equalsEpsilon(const ColouredVertex& vertex, float epsilon) const;
    };

private:
    struct UniformBufferData {
        glm::mat4 modelViewMatrix;
        glm::mat4 projectionMatrix;
        glm::vec2 resolution;
        bool depthTestEnabled;
    };

    struct RenderState {
        bool depthTestEnabled = false;
        vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
        bool blendEnabled = false;
        BlendMode colourBlendMode;
        BlendMode alphaBlendMode;
        float lineWidth = 1.0F;
    };

    struct RenderCommand {
        MeshPrimitiveType primitiveType;
        RenderState state;
        uint32_t vertexOffset;
        uint32_t indexOffset;
        uint32_t vertexCount;
        uint32_t indexCount;
    };

    struct RenderResources {
        Buffer* vertexBuffer;
        Buffer* indexBuffer;
        Buffer* uniformBuffer = nullptr;
        DescriptorSet* descriptorSet = nullptr;
        Framebuffer* framebuffer = nullptr;
        Image2D* frameColourImage = nullptr;
        ImageView* frameColourImageView = nullptr;
        Image2D* frameDepthImage = nullptr;
        ImageView* frameDepthImageView = nullptr;
        bool updateDescriptors = true;
    };

public:
    ImmediateRenderer();

    ~ImmediateRenderer();

    bool init();

    void render(double dt, const vk::CommandBuffer& commandBuffer);

    void begin(MeshPrimitiveType primitiveType);

    void end();

    void vertex(const glm::vec3& position);
    void vertex(float x, float y, float z);

    void normal(const glm::vec3& normal);
    void normal(float x, float y, float z);

    void texture(const glm::vec2& texture);
    void texture(float x, float y);

    void colour(const glm::u8vec4& colour);
    void colour(const glm::u8vec3& colour);
    void colour(const glm::uvec4& colour);
    void colour(const glm::uvec3& colour);
    void colour(const glm::vec4& colour);
    void colour(const glm::vec3& colour);
    void colour(float r, float g, float b, float a);
    void colour(float r, float g, float b);

    void pushMatrix(MatrixMode matrixMode);
    void pushMatrix();

    void popMatrix(MatrixMode matrixMode);
    void popMatrix();

    void translate(const glm::vec3& translation);
    void translate(float x, float y, float z);

    void rotate(const glm::vec3& axis, float angle);
    void rotate(float x, float y, float z, float angle);

    void scale(const glm::vec3& scale);
    void scale(float x, float y, float z);
    void scale(float scale);

    void loadIdentity();
    void loadMatrix(const glm::mat4& matrix);
    void multMatrix(const glm::mat4& matrix);

    void matrixMode(MatrixMode matrixMode);

    void setDepthTestEnabled(bool enabled);

    void setCullMode(const vk::CullModeFlags& cullMode);

    void setBlendEnabled(bool enabled);
    void setColourBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op);
    void setAlphaBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op);

    void setLineWidth(float lineWidth);

    ImageView* getOutputFrameImageView() const;

private:
    void addVertex(const ColouredVertex& vertex);

    void addIndex(uint32_t index);

    glm::mat4& currentMatrix(MatrixMode matrixMode);
    std::stack<glm::mat4>& matrixStack(MatrixMode matrixMode);

    glm::mat4& currentMatrix();
    std::stack<glm::mat4>& matrixStack();

    void uploadBuffers();

    GraphicsPipeline* getGraphicsPipeline(const RenderCommand& renderCommand);

    bool createRenderPass();

    void recreateSwapchain(RecreateSwapchainEvent* event);

    void validateCompleteCommand() const;

private:
    std::vector<ColouredVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    uint32_t m_vertexCount;
    uint32_t m_indexCount;
    uint32_t m_firstChangedVertex;
    uint32_t m_firstChangedIndex;

    std::vector<RenderCommand> m_renderCommands;
    std::vector<UniformBufferData> m_uniformBufferData;
    RenderCommand* m_currentCommand;
    RenderState m_renderState;

    std::stack<glm::mat4> m_modelMatrixStack;

    std::stack<glm::mat4> m_projectionMatrixStack;

    MatrixMode m_matrixMode;

    glm::vec3 m_normal;
    glm::vec2 m_texture;
    glm::u8vec4 m_colour;

    FrameResource<RenderResources> m_resources;
    SharedResource<DescriptorSetLayout> m_descriptorSetLayout;

    SharedResource<RenderPass> m_renderPass;
    std::unordered_map<size_t, GraphicsPipeline*> m_graphicsPipelines;
};

#endif //WORLDENGINE_IMMEDIATERENDERER_H
