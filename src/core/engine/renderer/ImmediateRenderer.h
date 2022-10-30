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

        ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, const uint8_t& r = 255, const uint8_t& g = 255, const uint8_t& b = 255, const uint8_t& a = 255);

        ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, const float& r = 1.0F, const float& g = 1.0F, const float& b = 1.0F, const float& a = 1.0F);

        bool equalsEpsilon(const ColouredVertex& vertex, const float& epsilon) const;
    };

private:
    struct UniformBufferData {
        glm::mat4 modelViewMatrix;
        glm::mat4 projectionMatrix;
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

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    void begin(const MeshPrimitiveType& primitiveType);

    void end();

    void vertex(const glm::vec3& position);
    void vertex(const float& x, const float& y, const float& z);

    void normal(const glm::vec3& normal);
    void normal(const float& x, const float& y, const float& z);

    void texture(const glm::vec2& texture);
    void texture(const float& x, const float& y);

    void colour(const glm::u8vec4& colour);
    void colour(const glm::u8vec3& colour);
    void colour(const glm::uvec4& colour);
    void colour(const glm::uvec3& colour);
    void colour(const glm::vec4& colour);
    void colour(const glm::vec3& colour);
    void colour(const float& r, const float& g, const float& b, const float& a);
    void colour(const float& r, const float& g, const float& b);

    void pushMatrix(const MatrixMode& matrixMode);
    void pushMatrix();

    void popMatrix(const MatrixMode& matrixMode);
    void popMatrix();

    void translate(const glm::vec3& translation);
    void translate(const float& x, const float& y, const float& z);

    void rotate(const glm::vec3& axis, const float& angle);
    void rotate(const float& x, const float& y, const float& z, const float& angle);

    void scale(const glm::vec3& scale);
    void scale(const float& x, const float& y, const float& z);
    void scale(const float& scale);

    void loadIdentity();
    void loadMatrix(const glm::mat4& matrix);
    void multMatrix(const glm::mat4& matrix);

    void matrixMode(const MatrixMode& matrixMode);

    void setDepthTestEnabled(const bool& enabled);

    void setCullMode(const vk::CullModeFlags& cullMode);

    void setBlendEnabled(const bool& enabled);
    void setColourBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op);
    void setAlphaBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op);

    void setLineWidth(const float& lineWidth);

    ImageView* getOutputFrameImageView() const;

private:
    void addVertex(const ColouredVertex& vertex);

    void addIndex(const uint32_t& index);

    glm::mat4& currentMatrix(const MatrixMode& matrixMode);
    std::stack<glm::mat4>& matrixStack(const MatrixMode& matrixMode);

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

    Buffer* m_vertexBuffer;
    Buffer* m_indexBuffer;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<DescriptorSetLayout> m_descriptorSetLayout;

    std::shared_ptr<RenderPass> m_renderPass;
    std::unordered_map<size_t, GraphicsPipeline*> m_graphicsPipelines;
};

#endif //WORLDENGINE_IMMEDIATERENDERER_H
