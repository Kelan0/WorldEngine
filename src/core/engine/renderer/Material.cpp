
#include "core/engine/renderer/Material.h"

void MaterialConfiguration::setAlbedo(const glm::uvec3& albedo) {
    this->albedoMap.reset();
    this->albedo = glm::u8vec3(glm::clamp(albedo, 0u, 255u));
}

void MaterialConfiguration::setAlbedo(const glm::vec3& albedo) {
    setAlbedo(glm::uvec3(albedo * 255.0F));
}

void MaterialConfiguration::setAlbedoMap(const std::weak_ptr<Texture>& albedoMap) {
    this->albedoMap = albedoMap.lock();
    this->albedo = glm::u8vec3(255, 255, 255);
}

void MaterialConfiguration::setAlbedoMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(image, sampler, name)));
}

void MaterialConfiguration::setAlbedoMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration, name)));
}

void MaterialConfiguration::setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler, name)));
}

void MaterialConfiguration::setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration, name)));
}

void MaterialConfiguration::setEmission(const glm::uvec3& emission) {
    this->emissionMap.reset();
    this->emission = glm::u16vec3(glm::clamp(emission, 0u, 65535u));
}

void MaterialConfiguration::setEmission(const glm::vec3& emission) {
    setEmission(glm::uvec3(emission * 255.0F));
}

void MaterialConfiguration::setEmissionMap(const std::weak_ptr<Texture>& emissionMap) {
    this->emissionMap = emissionMap.lock();
    this->emission = glm::vec3(0.0F, 0.0F, 0.0F);
}

void MaterialConfiguration::setEmissionMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setEmissionMap(std::shared_ptr<Texture>(Texture::create(image, sampler, name)));
}

void MaterialConfiguration::setEmissionMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setEmissionMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration, name)));
}

void MaterialConfiguration::setEmissionMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setEmissionMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler, name)));
}

void MaterialConfiguration::setEmissionMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setEmissionMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration, name)));
}

void MaterialConfiguration::setRoughness(const uint32_t& roughness) {
    this->roughnessMap.reset();
    this->roughness = uint8_t(glm::clamp(roughness, 0u, 255u));
}

void MaterialConfiguration::setRoughness(const float& roughness) {
    setRoughness((uint32_t)(roughness * 255.0F));
}

void MaterialConfiguration::setRoughnessMap(const std::weak_ptr<Texture>& roughnessMap) {
    this->roughnessMap = roughnessMap.lock();
    this->roughness = 255;
}

void MaterialConfiguration::setRoughnessMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(image, sampler, name)));
}

void MaterialConfiguration::setRoughnessMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration, name)));
}

void MaterialConfiguration::setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler, name)));
}

void MaterialConfiguration::setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration, name)));
}

void MaterialConfiguration::setMetallic(const uint32_t& metallic) {
    this->metallicMap.reset();
    this->metallic = uint8_t(glm::clamp(metallic, 0u, 255u));
}

void MaterialConfiguration::setMetallic(const float& metallic) {
    setMetallic((uint32_t)(metallic * 255.0F));
}

void MaterialConfiguration::setMetallicMap(const std::weak_ptr<Texture>& metallicMap) {
    this->metallicMap = metallicMap.lock();
    this->metallic = 0;
}

void MaterialConfiguration::setMetallicMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(image, sampler, name)));
}

void MaterialConfiguration::setMetallicMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration, name)));
}

void MaterialConfiguration::setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler, name)));
}

void MaterialConfiguration::setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration, name)));
}

void MaterialConfiguration::setNormalMap(const std::weak_ptr<Texture>& normalMap) {
    this->normalMap = normalMap.lock();
}

void MaterialConfiguration::setNormalMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(image, sampler, name)));
}

void MaterialConfiguration::setNormalMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration, name)));
}

void MaterialConfiguration::setNormalMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler, name)));
}

void MaterialConfiguration::setNormalMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration, name)));
}

void MaterialConfiguration::setDisplacementMap(const std::weak_ptr<Texture>& displacementMap) {
    this->displacementMap = displacementMap.lock();
}

void MaterialConfiguration::setDisplacementMap(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setDisplacementMap(std::shared_ptr<Texture>(Texture::create(image, sampler, name)));
}

void MaterialConfiguration::setDisplacementMap(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setDisplacementMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration, name)));
}

void MaterialConfiguration::setDisplacementMap(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const char* name) {
    setDisplacementMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler, name)));
}

void MaterialConfiguration::setDisplacementMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const char* name) {
    setDisplacementMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration, name)));
}





Material::Material():
    m_albedo(uint8_t(0)),
    m_emission(uint16_t(0)),
    m_roughness(uint8_t(0)),
    m_metallic(uint8_t(0)) {
}

Material::~Material() = default;

Material* Material::create(const MaterialConfiguration& materialConfiguration) {
    Material* material = new Material();
    material->m_albedoMap = materialConfiguration.albedoMap;
    material->m_albedo = materialConfiguration.albedo;
    material->m_roughnessMap = materialConfiguration.roughnessMap;
    material->m_roughness = materialConfiguration.roughness;
    material->m_metallicMap = materialConfiguration.metallicMap;
    material->m_metallic = materialConfiguration.metallic;
    material->m_emissionMap = materialConfiguration.emissionMap;
    material->m_emission = materialConfiguration.emission;
    material->m_normalMap = materialConfiguration.normalMap;
    material->m_displacementMap = materialConfiguration.displacementMap;
    return material;
}

std::shared_ptr<Texture> Material::getAlbedoMap() const {
    return m_albedoMap;
}

std::shared_ptr<Texture> Material::getEmissionMap() const {
    return m_emissionMap;
}

std::shared_ptr<Texture> Material::getRoughnessMap() const {
    return m_roughnessMap;
}

std::shared_ptr<Texture> Material::getMetallicMap() const {
    return m_metallicMap;
}

std::shared_ptr<Texture> Material::getNormalMap() const {
    return m_normalMap;
}

std::shared_ptr<Texture> Material::getDisplacementMap() const {
    return m_displacementMap;
}

const glm::u8vec3& Material::getAlbedo() const {
    return m_albedo;
}

const glm::u16vec3& Material::getEmission() const {
    return m_emission;
}

const glm::uint8_t& Material::getRoughness() const {
    return m_roughness;
}

const glm::uint8_t& Material::getMetallic() const {
    return m_metallic;
}

bool Material::hasAlbedoMap() const {
    return m_albedoMap != nullptr;
}

bool Material::hasEmissionMap() const {
    return m_emissionMap != nullptr;
}

bool Material::hasRoughnessMap() const {
    return m_roughnessMap != nullptr;
}

bool Material::hasMetallicMap() const {
    return m_metallicMap != nullptr;
}

bool Material::hasNormalMap() const {
    return m_normalMap != nullptr;
}

bool Material::hasDisplacementMap() const {
    return m_displacementMap != nullptr;
}