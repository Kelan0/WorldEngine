
#include "core/engine/renderer/Material.h"

void MaterialConfiguration::setAlbedo(const glm::uvec3& albedo) {
    this->albedoMap.reset();
    this->albedo = glm::u8vec3(glm::clamp(albedo, 0u, 255u));
}

void MaterialConfiguration::setAlbedo(const glm::vec3& albedo) {
    setAlbedo(glm::uvec3(albedo * 255.0F));
}

void MaterialConfiguration::setAlbedoMap(std::weak_ptr<Texture> albedoMap) {
    this->albedoMap = albedoMap.lock();
    this->albedo = glm::u8vec3(255, 255, 255);
}

void MaterialConfiguration::setAlbedoMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(image, sampler)));
}

void MaterialConfiguration::setAlbedoMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration)));
}

void MaterialConfiguration::setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler)));
}

void MaterialConfiguration::setAlbedoMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration) {
    setAlbedoMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration)));
}

void MaterialConfiguration::setRoughness(const uint32_t& roughness) {
    this->roughnessMap.reset();
    this->roughness = uint8_t(glm::clamp(roughness, 0u, 255u));
}

void MaterialConfiguration::setRoughness(const float& roughness) {
    setRoughness((uint32_t)(roughness * 255.0F));
}

void MaterialConfiguration::setRoughnessMap(std::weak_ptr<Texture> roughnessMap) {
    this->roughnessMap = roughnessMap.lock();
    this->roughness = 255;
}

void MaterialConfiguration::setRoughnessMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(image, sampler)));
}

void MaterialConfiguration::setRoughnessMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration)));
}

void MaterialConfiguration::setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler)));
}

void MaterialConfiguration::setRoughnessMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration) {
    setRoughnessMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration)));
}

void MaterialConfiguration::setMetallic(const uint32_t& metallic) {
    this->metallicMap.reset();
    this->metallic = uint8_t(glm::clamp(metallic, 0u, 255u));
}

void MaterialConfiguration::setMetallic(const float& metallic) {
    setMetallic((uint32_t)(metallic * 255.0F));
}

void MaterialConfiguration::setMetallicMap(std::weak_ptr<Texture> metallicMap) {
    this->metallicMap = metallicMap.lock();
    this->metallic = 0;
}

void MaterialConfiguration::setMetallicMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(image, sampler)));
}

void MaterialConfiguration::setMetallicMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration)));
}

void MaterialConfiguration::setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler)));
}

void MaterialConfiguration::setMetallicMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration) {
    setMetallicMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration)));
}

void MaterialConfiguration::setNormalMap(std::weak_ptr<Texture> normalMap) {
    this->normalMap = normalMap.lock();
}

void MaterialConfiguration::setNormalMap(std::weak_ptr<ImageView> image, std::weak_ptr<Sampler> sampler) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(image, sampler)));
}

void MaterialConfiguration::setNormalMap(std::weak_ptr<ImageView> image, const SamplerConfiguration& samplerConfiguration) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(image, samplerConfiguration)));
}

void MaterialConfiguration::setNormalMap(const ImageViewConfiguration& imageViewConfiguration, std::weak_ptr<Sampler> sampler) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, sampler)));
}

void MaterialConfiguration::setNormalMap(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration) {
    setNormalMap(std::shared_ptr<Texture>(Texture::create(imageViewConfiguration, samplerConfiguration)));
}




Material::Material() {

}

Material::~Material() {

}

Material* Material::create(const MaterialConfiguration& materialConfiguration) {
    Material* material = new Material();
    material->m_albedoMap = materialConfiguration.albedoMap;
    material->m_albedo = materialConfiguration.albedo;
    material->m_roughnessMap = materialConfiguration.roughnessMap;
    material->m_roughness = materialConfiguration.roughness;
    material->m_metallicMap = materialConfiguration.metallicMap;
    material->m_metallic = materialConfiguration.metallic;
    material->m_normalMap = materialConfiguration.normalMap;
    return material;
}

std::shared_ptr<Texture> Material::getAlbedoMap() const {
    return m_albedoMap;
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

const glm::u8vec3& Material::getAlbedo() const {
    return m_albedo;
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

bool Material::hasRoughnessMap() const {
    return m_roughnessMap != nullptr;
}

bool Material::hasMetallicMap() const {
    return m_metallicMap != nullptr;
}

bool Material::hasNormalMap() const {
    return m_normalMap != nullptr;
}