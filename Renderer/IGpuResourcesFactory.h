#pragma once
#include "GpuResources.h"

// Container for core Vulkan handles required for resource creation
struct GpuContext {
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
};

// Abstract class for Factory pattern
class IGpuResourcesFactory {
public:
    virtual ~IGpuResourcesFactory() = default;

    // Function to create buffers
    virtual BufferResource createBuffer(VkDeviceSize size,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags propertyFlags) = 0;

    // Function to create 2D images
    virtual ImageResource createImage2D(uint32_t width, uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usageFlags,
        VkMemoryPropertyFlags propertyFlags) = 0;

    // Function to create image view
    virtual VkImageView createImageView(VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags) = 0;

    // Function to create fully initialized 2D texture from pixel data
    virtual TextureResource createTexture2DFromRGBA8(const void* rgbaPixels,
        uint32_t width, uint32_t height) = 0;
};
