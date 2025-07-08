#pragma once

#include <vector>
#include <iostream>
#include <vulkan/vulkan.h>

extern const std::vector<const char*> validationLayers;
extern const bool enableValidationLayers;

VkResult CreateDebugReportCallbackEXT(
VkInstance instance,
const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
const VkAllocationCallbacks* pAllocator,
VkDebugReportCallbackEXT* pCallback);

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback,
const VkAllocationCallbacks* pAllocator);

bool checkValidationLayerSupport();

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
VkDebugReportFlagsEXT      flags,
VkDebugReportObjectTypeEXT objectType,
uint64_t                   object,
size_t                     location,
int32_t                    messageCode,
const char* pLayerPrefix,
const char* pMessage,
void* pUserData);