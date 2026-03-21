#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <glm/glm.hpp>

#include "tiny_gltf.h"
#include "Utilities.h"
#include "Mesh.h"
#include "vulkan/vulkan.h"

class GLTFLoader {
public:
	struct LoadOptions {
		bool flipV = false;
		bool generateNormals = false;
		bool verbose = true;
	};
	// Callback fn for creating a Vulkan Image
	using TextureCreatefn = std::function<int(const std::string& texturePath)>;

	GLTFLoader(VkPhysicalDevice physicalDevice, VkDevice device,
		VkQueue transferQueue, VkCommandPool transferCommandPool,
		TextureCreatefn textureCreatefn, int defaultTexId);

	std::vector<Mesh> loadFromFile(const std::string& gltfPath, const std::string& textureRootDirectory,
		const LoadOptions& options = {});

private:
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_transferQueue = VK_NULL_HANDLE;
	VkCommandPool m_transferPool = VK_NULL_HANDLE;

	TextureCreatefn m_textureCreatefn;

	std::unordered_map<int, int> m_imageToTexId;

	int m_defaultTexId = -1;

	std::vector<Mesh> buildMeshes(const tinygltf::Model& model,
		const std::string& textureRootDirectory,
		const LoadOptions& options);

	void processNode(int nodeIndex, const tinygltf::Model& model,
		const glm::mat4& parentWorld, const std::string& textureRootDirectory,
		const LoadOptions& options, std::vector<Mesh>& outMeshes);

	static glm::mat4 getNodeLocalMatrix(const tinygltf::Node& node);
	static glm::mat4 toMat4(const std::vector<double>& m44);

	void primitiveToMeshData(const tinygltf::Model& model, const tinygltf::Primitive& primitive,
		const LoadOptions& options, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices) const;

	const unsigned char* getAccessorDataPtr(const tinygltf::Model& model,
		const tinygltf::Accessor& accessor) const;

	void readAccessorVec2f(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
		std::vector<glm::vec2>& out) const;

	void readAccessorVec3f(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
		std::vector<glm::vec3>& out) const;

	void readIndicesU32(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
		std::vector<uint32_t>& out) const;

	int getPrimitiveTexId(const tinygltf::Model& model, const tinygltf::Primitive& primitive,
		const std::string& textureRootDirectory);

	static std::string joinPath(const std::string& root, const std::string& rel);
};