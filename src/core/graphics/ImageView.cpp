
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageCube.h"
#include "core/graphics/GraphicsManager.h"


void ImageViewConfiguration::setImage(const vk::Image& image, const vk::ImageViewType& imageViewType) {
    this->image = image;
    this->imageViewType = imageViewType;
}

void ImageViewConfiguration::setImage(const Image2D* image) {
    setImage(image->getImage(), vk::ImageViewType::e2D);
}

void ImageViewConfiguration::setImage(const ImageCube* image) {
    setImage(image->getImage(), vk::ImageViewType::eCube);
}

void ImageViewConfiguration::setSwizzle(vk::ComponentSwizzle redSwizzle, vk::ComponentSwizzle greenSwizzle, vk::ComponentSwizzle blueSwizzle, vk::ComponentSwizzle alphaSwizzle) {
    this->redSwizzle = redSwizzle;
    this->greenSwizzle = greenSwizzle;
    this->blueSwizzle = blueSwizzle;
    this->alphaSwizzle = alphaSwizzle;
}

ImageView::ImageView(std::weak_ptr<vkr::Device> device, const vk::ImageView& imageView, const vk::Image& image, const vk::ImageViewType& type):
        m_device(device),
        m_imageView(imageView),
        m_image(image),
        m_type(type),
        m_resourceId(GraphicsManager::nextResourceId()) {
}

ImageView::~ImageView() {
    (**m_device).destroyImageView(m_imageView);
}

ImageView* ImageView::create(const ImageViewConfiguration& imageViewConfiguration, const char* name) {
    const vk::Device& device = **imageViewConfiguration.device.lock();

    if (!imageViewConfiguration.image) {
        printf("Unable to create %s ImageView: Image is NULL\n", vk::to_string(imageViewConfiguration.imageViewType).c_str());
        return nullptr;
    }


    vk::ImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.setViewType(imageViewConfiguration.imageViewType);
    imageViewCreateInfo.setImage(imageViewConfiguration.image);
    imageViewCreateInfo.setFormat(imageViewConfiguration.format);
    imageViewCreateInfo.components.setR(imageViewConfiguration.redSwizzle);
    imageViewCreateInfo.components.setG(imageViewConfiguration.greenSwizzle);
    imageViewCreateInfo.components.setB(imageViewConfiguration.blueSwizzle);
    imageViewCreateInfo.components.setA(imageViewConfiguration.alphaSwizzle);
    imageViewCreateInfo.subresourceRange.setAspectMask(imageViewConfiguration.aspectMask);
    imageViewCreateInfo.subresourceRange.setBaseMipLevel(imageViewConfiguration.baseMipLevel);
    imageViewCreateInfo.subresourceRange.setLevelCount(imageViewConfiguration.mipLevelCount);
    imageViewCreateInfo.subresourceRange.setBaseArrayLayer(imageViewConfiguration.baseArrayLayer);
    imageViewCreateInfo.subresourceRange.setLayerCount(imageViewConfiguration.arrayLayerCount);

    vk::ImageView imageView = VK_NULL_HANDLE;

    vk::Result result;
    result = device.createImageView(&imageViewCreateInfo, nullptr, &imageView);

    if (result != vk::Result::eSuccess) {
        printf("Failed to create %s ImageView: %s\n", vk::to_string(imageViewConfiguration.imageViewType).c_str(), vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkImageView)imageView, vk::ObjectType::eImageView, name);

    return new ImageView(imageViewConfiguration.device, imageView, imageViewConfiguration.image, imageViewConfiguration.imageViewType);
}

std::shared_ptr<vkr::Device> ImageView::getDevice() const {
    return m_device;
}

const vk::ImageView& ImageView::getImageView() const {
    return m_imageView;
}

const vk::Image& ImageView::getImage() const {
    return m_image;
}

const vk::ImageViewType& ImageView::getType() const {
    return m_type;
}

const GraphicsResource& ImageView::getResourceId() const {
    return m_resourceId;
}