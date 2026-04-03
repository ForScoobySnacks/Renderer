#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <array>
#include <memory>

#include "stb_image.h"

#include "Mesh.h"
#include "VulkanValidation.h"
#include "Utilities.h"
#include "GpuResources.h"
#include "IGpuResourcesFactory.h"
#include "VulkanGpuResourcesFactory.h"
#include "GLTFLoader.h"
#include "Camera.h"

class VulkanRenderer
{
public:
	// Destructor which ensures that all Vulkan resources are released in the correct order
	~VulkanRenderer();

	// Accessor for the Singleton instance
	static VulkanRenderer* getInstance(GLFWwindow* window);

	// Method to update the transormation matrix for a specific mesh
	void updateModel(int modelId, glm::mat4 newModel);

	void draw(); 
	// Method to destroy the renderer instance
	// IMPORTANT: The Developer has to call this function before window termination
	static void deleteInstance();

	// Deleted copy constructor to prevent multiple instances
	VulkanRenderer(const VulkanRenderer&) = delete;
	// Deleted assignment operator to prevent copying
	VulkanRenderer& operator=(const VulkanRenderer&) = delete;

private:
	// vpUniform, lightUniform and texture factory
	std::unique_ptr<IGpuResourcesFactory> gpuFactory = nullptr;
	// Singleton instance
	inline static std::unique_ptr<VulkanRenderer> renderer = nullptr;

	// Window
	GLFWwindow* window = nullptr;

	// Camera
	Camera camera;

	bool firstMouse = true;
	double lastMouseX = 0.0;
	double lastMouseY = 0.0;

	float lastFrameTime = 0.0f;

	// Method to stup inputs
	void setupInput();
	// Method to process keyboard inputs
	void processInput(float deltaTime);
	// Method to process mouse movement
	static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

	// Scene objects
	std::vector<Mesh> meshList = {};

	// Scene settings
	struct UboViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection = {};

	// Light
// It secures the memory from the CPU, because of the std140
	struct alignas(16) LightUBO {
		glm::vec4 lightDir;
		glm::vec4 lightCol;
	};

	std::vector<BufferResource> lightUniforms;

	// Vulkan Components
	// Main
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
	struct {
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice logicalDevice = VK_NULL_HANDLE;
	} mainDevice = {};
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentationQueue = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;

	std::vector<SwapChainImage> swapChainImages = {};
	std::vector<VkFramebuffer> swapChainFramebuffers = {};
	std::vector<VkCommandBuffer> commandBuffers = {};

	VkImage depthBufferImage = VK_NULL_HANDLE;
	VkDeviceMemory depthBufferImageMemory = VK_NULL_HANDLE;
	VkImageView depthBufferImageView = {};

	// Descriptors
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout samplerSetLayout = VK_NULL_HANDLE;
	VkPushConstantRange pushConstantRange = {};

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorPool samplerDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets = {};
	std::vector<VkDescriptorSet> samplerDescriptorSets = {};

	std::vector<BufferResource> vpUniforms = {};

	std::vector<VkBuffer> dynamicUniformBuffer = {};
	std::vector<VkDeviceMemory> dynamicUniformBufferMemory = {};

	// Assets
	VkSampler textureSampler = VK_NULL_HANDLE;
	std::vector<TextureResource> textures;

	// Pipeline
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;

	// Pools
	VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;

	// Utilities
	VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D swapChainExtent = {};

	// Synchronisation
	std::vector<VkSemaphore> imageAvailableSemaphores = {};
	std::vector<VkSemaphore> renderFinishedSemaphores = {};
	std::vector<VkFence> inFlightFences = {};
	std::vector<VkFence> imagesInFlight = {};

	size_t currentFrame = 0;

	// Renderer initialization functions
	void init();
	VulkanRenderer() = default;

	// Vulkan Functions
	// - Create Functions
	void createInstance();
	void createDebugCallback();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createPushConstantRange();
	void createGraphicsPipeline();
	void createDepthBufferImage();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronisation();
	void createTextureSampler();

	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffers(uint32_t imageIndex);

	// Record Functions
	void recordCommands(uint32_t currentImage);

	// - Get Functions
	void getPhysicalDevice();

	// - Support Functions
	// -- Checker Functions
	bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkValidationLayerSupport();
	bool checkDeviceSuitable(VkPhysicalDevice device);

	// -- Getter Functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

	// -- Choose functions
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkFormat chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

	// -- Create functions
	VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags,
		VkMemoryPropertyFlags propFlags, VkDeviceMemory *imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);

	int createTextureImage(std::string fileName);
	int createTexture(std::string fileName);
	int createTextureDescriptor(VkImageView textureImage);

	// -- Loader functions
	stbi_uc* loadTextureFile(std::string fileName, int* width, int* height, VkDeviceSize* imageSize);
};


