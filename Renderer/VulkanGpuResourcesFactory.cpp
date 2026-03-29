#include "VulkanGpuResourcesFactory.h"
#include "Utilities.h"
#include <stdexcept>
#include <cstring>

BufferResource VulkanGpuResourcesFactory::createBuffer(VkDeviceSize size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags)
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    ::createBuffer(gpuContext.physical, gpuContext.device, size, usageFlags, propertyFlags, &buffer, &memory);

    BufferResource outBuffer{};
    outBuffer.buffer = buffer;
    outBuffer.memory = memory;
    outBuffer.size = size;
    return outBuffer;
}

ImageResource VulkanGpuResourcesFactory::createImage2D(uint32_t width, uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags)
{
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = { width, height, 1 };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = format;
    info.tiling = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usageFlags;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(gpuContext.device, &info, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(gpuContext.device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryTypeIndex(gpuContext.physical, memRequirements.memoryTypeBits, propertyFlags);

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(gpuContext.device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(gpuContext.device, image, nullptr);
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(gpuContext.device, image, memory, 0);

    ImageResource out{};
    out.image = image;
    out.memory = memory;
    return out;
}

VkImageView VulkanGpuResourcesFactory::createImageView(VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo v{};
    v.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    v.image = image;
    v.viewType = VK_IMAGE_VIEW_TYPE_2D;
    v.format = format;
    v.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    v.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    v.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    v.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    v.subresourceRange.aspectMask = aspectFlags;
    v.subresourceRange.baseMipLevel = 0;
    v.subresourceRange.levelCount = 1;
    v.subresourceRange.baseArrayLayer = 0;
    v.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(gpuContext.device, &v, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }
    return view;
}

TextureResource VulkanGpuResourcesFactory::createTexture2DFromRGBA8(const void* rgbaPixels,
    uint32_t width, uint32_t height)
{
    const VkDeviceSize imageSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    BufferResource staging = createBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data = nullptr;
    vkMapMemory(gpuContext.device, staging.memory, 0, imageSize, 0, &data);
    std::memcpy(data, rgbaPixels, (size_t)imageSize);
    vkUnmapMemory(gpuContext.device, staging.memory);

    ImageResource image = createImage2D(width, height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transitionImageLayout(gpuContext.device, gpuContext.graphicsQueue, gpuContext.graphicsCommandPool,
        image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyImageBuffer(gpuContext.device, gpuContext.graphicsQueue, gpuContext.graphicsCommandPool,
        staging.buffer, image.image, width, height);

    transitionImageLayout(gpuContext.device, gpuContext.graphicsQueue, gpuContext.graphicsCommandPool,
        image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(gpuContext.device, staging.buffer, nullptr);
    vkFreeMemory(gpuContext.device, staging.memory, nullptr);

    TextureResource texture{};
    texture.image = image;
    texture.view = createImageView(image.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    return texture;
}