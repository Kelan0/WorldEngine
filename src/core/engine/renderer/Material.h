
#ifndef WORLDENGINE_MATERIAL_H
#define WORLDENGINE_MATERIAL_H

#include "core/core.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Texture.h"

struct MaterialConfiguration {
    std::shared_ptr<Texture2D> albedoMap;
    glm::u8vec3 albedo = glm::u8vec3(255, 255, 255);

    std::shared_ptr<Texture2D> roughnessMap;
    uint8_t roughness = 255;

    std::shared_ptr<Texture2D> metallicMap;
    uint8_t metallic = 0;

    std::shared_ptr<Texture2D> normalMap;


    void setAlbedo(const glm::uvec3& albedo);
    void setAlbedo(const glm::vec3& albedo);
    void setAlbedoMap(std::weak_ptr<Texture2D> albedoMap);
    void setAlbedoMap(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler);
    void setAlbedoMap(std::weak_ptr<ImageView2D> image, const SamplerConfiguration& samplerConfiguration);
    void setAlbedoMap(const ImageView2DConfiguration& imageView2DConfiguration, std::weak_ptr<Sampler> sampler);
    void setAlbedoMap(const ImageView2DConfiguration& imageView2DConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setRoughness(const uint32_t& roughness);
    void setRoughness(const float& roughness);
    void setRoughnessMap(std::weak_ptr<Texture2D> roughnessMap);
    void setRoughnessMap(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler);
    void setRoughnessMap(std::weak_ptr<ImageView2D> image, const SamplerConfiguration& samplerConfiguration);
    void setRoughnessMap(const ImageView2DConfiguration& imageView2DConfiguration, std::weak_ptr<Sampler> sampler);
    void setRoughnessMap(const ImageView2DConfiguration& imageView2DConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setMetallic(const uint32_t& metallic);
    void setMetallic(const float& metallic);
    void setMetallicMap(std::weak_ptr<Texture2D> metallicMap);
    void setMetallicMap(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler);
    void setMetallicMap(std::weak_ptr<ImageView2D> image, const SamplerConfiguration& samplerConfiguration);
    void setMetallicMap(const ImageView2DConfiguration& imageView2DConfiguration, std::weak_ptr<Sampler> sampler);
    void setMetallicMap(const ImageView2DConfiguration& imageView2DConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setNormalMap(std::weak_ptr<Texture2D> metallicMap);
    void setNormalMap(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler);
    void setNormalMap(std::weak_ptr<ImageView2D> image, const SamplerConfiguration& samplerConfiguration);
    void setNormalMap(const ImageView2DConfiguration& imageView2DConfiguration, std::weak_ptr<Sampler> sampler);
    void setNormalMap(const ImageView2DConfiguration& imageView2DConfiguration, const SamplerConfiguration& samplerConfiguration);
};

class Material {
private:
    Material();

public:
    ~Material();

    static Material* create(const MaterialConfiguration& materialConfiguration);

    std::shared_ptr<Texture2D> getAlbedoMap() const;

    std::shared_ptr<Texture2D> getRoughnessMap() const;

    std::shared_ptr<Texture2D> getMetallicMap() const;

    std::shared_ptr<Texture2D> getNormalMap() const;

    const glm::u8vec3& getAlbedo() const;

    const glm::uint8_t& getRoughness() const;

    const glm::uint8_t& getMetallic() const;

    bool hasAlbedoMap() const;

    bool hasRoughnessMap() const;

    bool hasMetallicMap() const;

    bool hasNormalMap() const;

private:
    std::shared_ptr<Texture2D> m_albedoMap;
    std::shared_ptr<Texture2D> m_roughnessMap;
    std::shared_ptr<Texture2D> m_metallicMap;
    std::shared_ptr<Texture2D> m_normalMap;
    glm::u8vec3 m_albedo;
    uint8_t m_roughness;
    uint8_t m_metallic;
};


#endif //WORLDENGINE_MATERIAL_H
