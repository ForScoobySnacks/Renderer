#pragma once
#include <vulkan/vulkan.h>

// Struct to encapsulate buffer handle, allocated memory, and the size of the buffer
struct BufferResource {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

// Struct to encapsulate image handle and allocated memory
struct ImageResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

// Struct to encapsulate the "ImageResource" and the corresponding image view
struct TextureResource {
    ImageResource image;
    VkImageView view = VK_NULL_HANDLE;
};