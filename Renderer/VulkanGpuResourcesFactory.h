#pragma once
#include "IGpuResourcesFactory.h"

class VulkanGpuResourcesFactory : public IGpuResourcesFactory {
public:
    explicit VulkanGpuResourcesFactory(GpuContext gpuContext) : gpuContext(gpuContext) {}

    BufferResource createBuffer(VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memProps) override;

    ImageResource createImage2D(uint32_t width, uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usageFlags,
        VkMemoryPropertyFlags propertyFlags) override;

    VkImageView createImageView(VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags) override;

    TextureResource createTexture2DFromRGBA8(const void* rgbaPixels,
        uint32_t width, uint32_t height) override;

private:
    GpuContext gpuContext;
};