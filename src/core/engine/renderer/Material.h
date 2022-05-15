
#ifndef WORLDENGINE_MATERIAL_H
#define WORLDENGINE_MATERIAL_H

#include "core/core.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Texture.h"

struct MaterialConfiguration {
    std::shared_ptr<Texture> albedoMap;
    glm::u8vec3 albedo = glm::u8vec3(255, 255, 255);

    std::shared_ptr<Texture> emissionMap;
    glm::u16vec3 emission = glm::u16vec3(0, 0, 0);

    std::shared_ptr<Texture> roughnessMap;
    uint8_t roughness = 255;

    std::shared_ptr<Texture> metallicMap;
    uint8_t metallic = 0;

    std::shared_ptr<Texture> normalMap;


    void setAlbedo(const glm::uvec3& albedo);
    void setAlbedo(const glm::vec3& albedo);
    void setAlbedoMap(std::weak_ptr<Texture> albedoMap);
    void setAlbedoMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler);
    void setAlbedoMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration);
    void setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler);
    void setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setEmission(const glm::uvec3& emission);
    void setEmission(const glm::vec3& emission);
    void setEmissionMap(std::weak_ptr<Texture> emissionMap);
    void setEmissionMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler);
    void setEmissionMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration);
    void setEmissionMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler);
    void setEmissionMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setRoughness(const uint32_t& roughness);
    void setRoughness(const float& roughness);
    void setRoughnessMap(std::weak_ptr<Texture> roughnessMap);
    void setRoughnessMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler);
    void setRoughnessMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration);
    void setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler);
    void setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setMetallic(const uint32_t& metallic);
    void setMetallic(const float& metallic);
    void setMetallicMap(std::weak_ptr<Texture> metallicMap);
    void setMetallicMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler);
    void setMetallicMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration);
    void setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler);
    void setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration);

    void setNormalMap(std::weak_ptr<Texture> metallicMap);
    void setNormalMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler);
    void setNormalMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration);
    void setNormalMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler);
    void setNormalMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration);
};

class Material {
private:
    Material();

public:
    ~Material();

    static Material* create(const MaterialConfiguration& materialConfiguration);

    [[nodiscard]] std::shared_ptr<Texture> getAlbedoMap() const;

    [[nodiscard]] std::shared_ptr<Texture> getEmissionMap() const;

    [[nodiscard]] std::shared_ptr<Texture> getRoughnessMap() const;

    [[nodiscard]] std::shared_ptr<Texture> getMetallicMap() const;

    [[nodiscard]] std::shared_ptr<Texture> getNormalMap() const;

    [[nodiscard]] const glm::u8vec3& getAlbedo() const;

    [[nodiscard]] const glm::u16vec3& getEmission() const;

    [[nodiscard]] const glm::uint8_t& getRoughness() const;

    [[nodiscard]] const glm::uint8_t& getMetallic() const;

    [[nodiscard]] bool hasAlbedoMap() const;

    [[nodiscard]] bool hasEmissionMap() const;

    [[nodiscard]] bool hasRoughnessMap() const;

    [[nodiscard]] bool hasMetallicMap() const;

    [[nodiscard]] bool hasNormalMap() const;

private:
    std::shared_ptr<Texture> m_albedoMap;
    std::shared_ptr<Texture> m_emissionMap;
    std::shared_ptr<Texture> m_roughnessMap;
    std::shared_ptr<Texture> m_metallicMap;
    std::shared_ptr<Texture> m_normalMap;
    glm::u8vec3 m_albedo;
    glm::u16vec3 m_emission;
    uint8_t m_roughness;
    uint8_t m_metallic;
};


#endif //WORLDENGINE_MATERIAL_H
