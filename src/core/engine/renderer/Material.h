
#ifndef WORLDENGINE_MATERIAL_H
#define WORLDENGINE_MATERIAL_H

#include "core/core.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"

struct MaterialConfiguration {
    WeakResource<vkr::Device> device;

    std::shared_ptr<Texture> albedoMap;
    glm::u8vec3 albedo = glm::u8vec3(255, 255, 255);

    std::shared_ptr<Texture> emissionMap;
    glm::u16vec3 emission = glm::u16vec3(0, 0, 0);

    std::shared_ptr<Texture> roughnessMap;
    uint8_t roughness = 255;

    std::shared_ptr<Texture> metallicMap;
    uint8_t metallic = 0;

    std::shared_ptr<Texture> normalMap;

    std::shared_ptr<Texture> displacementMap;


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

    void setRoughness(uint32_t roughness);
    void setRoughness(float roughness);
    void setRoughnessMap(const std::weak_ptr<Texture>& roughnessMap);
    void setRoughnessMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setRoughnessMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);

    void setMetallic(uint32_t metallic);
    void setMetallic(float metallic);
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

    void setDisplacementMap(const std::weak_ptr<Texture>& displacementMap);
    void setDisplacementMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setDisplacementMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name);
    void setDisplacementMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name);
    void setDisplacementMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name);
};

class Material : public GraphicsResource {
private:
    Material(const WeakResource<vkr::Device>& device, const std::string& name);

public:
    ~Material() override;

    static Material* create(const MaterialConfiguration& materialConfiguration, const std::string& name);

    std::shared_ptr<Texture> getAlbedoMap() const;

    std::shared_ptr<Texture> getEmissionMap() const;

    std::shared_ptr<Texture> getRoughnessMap() const;

    std::shared_ptr<Texture> getMetallicMap() const;

    std::shared_ptr<Texture> getNormalMap() const;

    std::shared_ptr<Texture> getDisplacementMap() const;

    const glm::u8vec3& getAlbedo() const;

    const glm::u16vec3& getEmission() const;

    const glm::uint8_t& getRoughness() const;

    const glm::uint8_t& getMetallic() const;

    bool hasAlbedoMap() const;

    bool hasEmissionMap() const;

    bool hasRoughnessMap() const;

    bool hasMetallicMap() const;

    bool hasNormalMap() const;

    bool hasDisplacementMap() const;

private:
    std::shared_ptr<Texture> m_albedoMap;
    std::shared_ptr<Texture> m_emissionMap;
    std::shared_ptr<Texture> m_roughnessMap;
    std::shared_ptr<Texture> m_metallicMap;
    std::shared_ptr<Texture> m_normalMap;
    std::shared_ptr<Texture> m_displacementMap;
    glm::u8vec3 m_albedo;
    glm::u16vec3 m_emission;
    uint8_t m_roughness;
    uint8_t m_metallic;
};


#endif //WORLDENGINE_MATERIAL_H
