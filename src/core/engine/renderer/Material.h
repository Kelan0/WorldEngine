
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
    void setAlbedoMap(const std::weak_ptr<Texture>& albedoMap);
    void setAlbedoMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setAlbedoMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);

    void setEmission(const glm::uvec3& emission);
    void setEmission(const glm::vec3& emission);
    void setEmissionMap(const std::weak_ptr<Texture>& emissionMap);
    void setEmissionMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setEmissionMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setEmissionMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setEmissionMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);

    void setRoughness(const uint32_t& roughness);
    void setRoughness(const float& roughness);
    void setRoughnessMap(const std::weak_ptr<Texture>& roughnessMap);
    void setRoughnessMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setRoughnessMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);

    void setMetallic(const uint32_t& metallic);
    void setMetallic(const float& metallic);
    void setMetallicMap(const std::weak_ptr<Texture>& metallicMap);
    void setMetallicMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setMetallicMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);

    void setNormalMap(const std::weak_ptr<Texture>& metallicMap);
    void setNormalMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setNormalMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setNormalMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setNormalMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);
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
