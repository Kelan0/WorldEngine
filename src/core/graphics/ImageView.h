#ifndef WORLDENGINE_IMAGEVIEW_H
#define WORLDENGINE_IMAGEVIEW_H

#include "core/core.h"

class Image2D;
class ImageCube;

struct ImageViewConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::ImageViewType imageViewType = vk::ImageViewType::e2D;
    vk::Image image;
    vk::Format format;
    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayerCount = 1;
    vk::ComponentSwizzle redSwizzle = vk::ComponentSwizzle::eIdentity;
    vk::ComponentSwizzle greenSwizzle = vk::ComponentSwizzle::eIdentity;
    vk::ComponentSwizzle blueSwizzle = vk::ComponentSwizzle::eIdentity;
    vk::ComponentSwizzle alphaSwizzle = vk::ComponentSwizzle::eIdentity;

    void setImage(const vk::Image& image, const vk::ImageViewType& imageViewType);

    void setImage(const Image2D* image);

    void setImage(const ImageCube* image);

    void setSwizzle(vk::ComponentSwizzle redSwizzle, vk::ComponentSwizzle greenSwizzle, vk::ComponentSwizzle blueSwizzle, vk::ComponentSwizzle alphaSwizzle);
};

class ImageView {
    NO_COPY(ImageView)
private:
    ImageView(std::weak_ptr<vkr::Device> device, const vk::ImageView& imageView, const vk::Image& image, const vk::ImageViewType& type);

public:
    ~ImageView();

    static ImageView* create(const ImageViewConfiguration& imageViewConfiguration, const char* name);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::ImageView& getImageView() const;

    const vk::Image& getImage() const;

    const vk::ImageViewType& getType() const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::ImageView m_imageView;
    vk::Image m_image;
    vk::ImageViewType m_type;
    GraphicsResource m_resourceId;
};


#endif //WORLDENGINE_IMAGEVIEW_H
