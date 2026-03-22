#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "GLTFLoader.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
    // Helper function for replacing the missing UV coordinates
    // p == Vertex position, n == NormalVector, pmin and pmax == Two side corners of the Bounding Box 
    static glm::vec2 cubeProjectUV(const glm::vec3& p, const glm::vec3& n,
        const glm::vec3& pmin, const glm::vec3& pmax)
    {
        // Calculate the object size
        glm::vec3 size = pmax - pmin;
        // 1e-6f is a very small number, I use it to avoid division with 0, If the object is flat
        size = glm::max(size, glm::vec3(1e-6f));

        // Check where the normalVector is pointing to
        glm::vec3 an = glm::abs(n);

        glm::vec2 uv(0.0f);

        // If the X axis is dominant, the surface is looking to the side
        // In this case, the texture is stretched along the Z and Y axes
        // If the normalVector is pointing to a positive value, rotate the U axis, so the texture won't be mirrored
        if (an.x >= an.y && an.x >= an.z) {
            uv.x = (p.z - pmin.z) / size.z;
            uv.y = (p.y - pmin.y) / size.y;
            if (n.x > 0.0f) uv.x = 1.0f - uv.x;
        } // If the Y axis is dominant, the surface is looking up or down
        else if (an.y >= an.x && an.y >= an.z) {
            uv.x = (p.x - pmin.x) / size.x;
            uv.y = (p.z - pmin.z) / size.z;
            if (n.y < 0.0f) uv.y = 1.0f - uv.y;
        } // If the Z axis is dominant, the surface is looking ahead or behind
        else {
            uv.x = (p.x - pmin.x) / size.x;
            uv.y = (p.y - pmin.y) / size.y;
            if (n.z < 0.0f) uv.x = 1.0f - uv.x;
        }

        return uv;
    }

    // Helper function to check the "s" string if it ends with the second parameter suffix
	static bool endsWith(const std::string& file, const std::string& suffix) {
		return std::equal(suffix.rbegin(), suffix.rend(), file.rbegin());
	}
    // Helper function which returns how many bytes will be needed for a componentType
    static size_t componentTypeSizeInBytes(int componentType) {
        switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:           return 1;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return 1;
        case TINYGLTF_COMPONENT_TYPE_SHORT:          return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
        case TINYGLTF_COMPONENT_TYPE_INT:            return 4;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return 4;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:          return 4;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return 8;
        default: throw std::runtime_error("Unsupported glTF componentType");
        }
    }

    // Helper function which returns how many base components in "type"
    static size_t typeNumComponents(int type) {
        switch (type) {
        case TINYGLTF_TYPE_SCALAR: return 1;
        case TINYGLTF_TYPE_VEC2:   return 2;
        case TINYGLTF_TYPE_VEC3:   return 3;
        case TINYGLTF_TYPE_VEC4:   return 4;
        case TINYGLTF_TYPE_MAT2:   return 4;
        case TINYGLTF_TYPE_MAT3:   return 9;
        case TINYGLTF_TYPE_MAT4:   return 16;
        default: throw std::runtime_error("Unsupported glTF accessor type");
        }
    }

    // Helper function to calculate the physical size in bytes
    static size_t packedElementSize(const tinygltf::Accessor& accessor) {
        return componentTypeSizeInBytes(accessor.componentType) * typeNumComponents(accessor.type);
    }
}

GLTFLoader::GLTFLoader(VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue transferQueue,
    VkCommandPool transferCommandPool,
    TextureCreatefn textureCreatefn,
    int defaultTexId)
    : m_physicalDevice(physicalDevice)
    , m_device(device)
    , m_transferQueue(transferQueue)
    , m_transferPool(transferCommandPool)
    , m_textureCreatefn(std::move(textureCreatefn))
    , m_defaultTexId(defaultTexId) {
}
// Loads the .gltf (JSON) or .glb (binary) file to memory
std::vector<Mesh> GLTFLoader::loadFromFile(const std::string& gltfPath,
    const std::string& textureRootDirectory,
    const LoadOptions& options) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string err;
    std::string warn;

    bool ok = false;
    if (endsWith(gltfPath, ".glb") || endsWith(gltfPath, ".GLB")) {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, gltfPath);
    }
    else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, gltfPath);
    }

    if (options.verbose) {
        if (!warn.empty()) 
            std::cout << "[glTF warn] " << warn << "\n";
        if (!err.empty())  
            std::cout << "[glTF err ] " << err << "\n";
    }

    if (!ok) {
        throw std::runtime_error("Failed to load glTF: " + gltfPath);
    }

    return buildMeshes(model, textureRootDirectory, options);
}

// Entry point: Finds the starting scene and initiates the recursive traversal of the node hierarchy
std::vector<Mesh> GLTFLoader::buildMeshes(const tinygltf::Model& model,
    const std::string& textureRootDirectory,
    const LoadOptions& options) {
    std::vector<Mesh> out;

    // Determine which scene to load:
    // - Use default or fallback to the first one
    int sceneIndex = model.defaultScene;
    if (sceneIndex < 0) {
        sceneIndex = 0;
    }

    //Validate scene index
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(model.scenes.size())) {
        return out;
    }

    const tinygltf::Scene& scene = model.scenes[sceneIndex];

    // Start processing from the root nodes of the scene
    // I pass an identity matrix, because root nodes start at the world origin
    const glm::mat4 identity(1.0f);
    for (int nodeIndex : scene.nodes) {
        processNode(nodeIndex, model, identity, textureRootDirectory, options, out);
    }

    return out;
}

// Recursive function that processes a single node, calculates its world-space transformation, extracts mesh data, and moves to child nodes
void GLTFLoader::processNode(int nodeIndex,
    const tinygltf::Model& model,
    const glm::mat4& parentWorld, // The transform of the parent in world space
    const std::string& textureRootDirectory,
    const LoadOptions& options,
    std::vector<Mesh>& outMeshes) {
    // Validation for node index
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) return;

    const tinygltf::Node& node = model.nodes[nodeIndex];

    // Calculate Transformation:
    // Local matrix is the node's own translation, rotation, scale
    const glm::mat4 local = getNodeLocalMatrix(node);

    // World matrix combines the parent's world transform with the local transform
    // Order matters: Parent * Local ensures the child moves relative to the parent
    const glm::mat4 world = parentWorld * local;

    // Process Mesh
    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
        const tinygltf::Mesh& gltfMesh = model.meshes[node.mesh];

        // A glTF Mesh is composed of Primitives (sub-meshes with different materials)
        for (const tinygltf::Primitive& prim : gltfMesh.primitives) {
            // Filtering only supports triangles
            // Skip lines, points or triangle strips
            if (prim.mode != -1 && prim.mode != TINYGLTF_MODE_TRIANGLES) {
                if (options.verbose) {
                    std::cout << "Skipping primitive: unsupported mode (not TRIANGLES)\n";
                }
                continue;
            }

            // Data Extraction
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // Convert raw binary buffer data into arrays of Vertex and Index structures
            primitiveToMeshData(model, prim, options, vertices, indices);

            // Material Extraction
            // Get the texture ID associated with this primitive
            int texId = getPrimitiveTexId(model, prim, textureRootDirectory);

            // Create a new Mesh object
            Mesh mesh(m_physicalDevice, m_device, m_transferQueue, m_transferPool,
                &vertices, &indices, texId);

            // Apply Transform
            // Store the calculated world matrix inside the mesh for the shader to use
            mesh.setModel(world);
            outMeshes.push_back(mesh);
        }
    }

    // Recursion to process all children
    for (int childIndex : node.children) {
        processNode(childIndex, model, world, textureRootDirectory, options, outMeshes);
    }
}

// Function to transform a vector of doubles to a 4x4 matrix, which will contain the position, rotation and scale
glm::mat4 GLTFLoader::toMat4(const std::vector<double>& m44) {
    // If the vector doesn't contain 16 doubles then the function returns an identity matrix
    if (m44.size() != 16) 
        return glm::mat4(1.0f);

    glm::mat4 m(1.0f);
    // Column-major sequence
    // Convert doubles into float numbers for better speed and memory usage
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            m[col][row] = static_cast<float>(m44[col * 4 + row]);
        }
    }
    return m;
}

// Function to calculate the position, scale and where the object is facing (Local Transformation Matrix)
glm::mat4 GLTFLoader::getNodeLocalMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        return toMat4(node.matrix);
    }
    // If there is no ready-made matrix, the transformation must be assembled from Translation, Rotation, Scale
    glm::vec3 t(0.0f); // Origo
    glm::vec3 s(1.0f); // Basic size
    glm::quat r(1.0f, 0.0f, 0.0f, 0.0f); // No rotation (unit quaternion)

    // If the Node contains the values for Translation, I copy it
    if (node.translation.size() == 3) {
        t = glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
    }
    if (node.scale.size() == 3) {
        s = glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
    } // Sequence for quaternion [x,y,z,w], but the glm::quat contructor's sequence for it is (w,x,y,z)
    if (node.rotation.size() == 4) {
        r = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
    }

    glm::mat4 M(1.0f);
    // Compute the local transformation matrix using the TRS (Translation, Rotation, Scale) order.
    // In GLM/OpenGL, matrix multiplication is evaluated from RIGHT to LEFT.
    M = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
    return M;
}

// Returns a raw pointer to the start of the data described by a glTF Accessor
// glTF data hierarchy: Accessor -> BufferView -> Buffer
const unsigned char* GLTFLoader::getAccessorDataPtr(const tinygltf::Model& model,
    const tinygltf::Accessor& accessor) const {
    // Validate that the Accessor points to a valid BufferView index
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        throw std::runtime_error("Accessor has invalid bufferView");
    }

    // Get a reference to the BufferView (the specific slice of the binary file)
    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];

    // Validate that the BufferView points to a valid Buffer index(the actual file data)
    if (bv.buffer < 0 || bv.buffer >= static_cast<int>(model.buffers.size())) {
        throw std::runtime_error("BufferView has invalid buffer index");
    }

    // Get a reference to the Buffer (the raw byte array)
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];

    // Calculate the total offset from the start of the Buffer:
    // - bv.byteOffset: where the slice starts in the file
    // - accessor.byteOffset: where the specific attribute starts within that slice
    const size_t baseOffset = static_cast<size_t>(bv.byteOffset) + static_cast<size_t>(accessor.byteOffset);

    // Safety check : Ensure the calculated offset doesn't point outside the actual data size
    if (baseOffset >= buf.data.size()) {
        throw std::runtime_error("Accessor points outside buffer");
    }

    // Return the memory address : Start of Buffer Data + Calculated Offset
    return buf.data.data() + baseOffset;
}

// Function to transform binary bytes to a vector of glm::vec2
void GLTFLoader::readAccessorVec2f(const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<glm::vec2>& out) const {
    // Validation to only accept vec2
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2) {
        throw std::runtime_error("Expected VEC2 float accessor");
    }

    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    // Raw pointer to get the exact starting place in the RAM
    const unsigned char* base = getAccessorDataPtr(model, accessor);

    // The stride variable tells how much bytes should it skip to get to the next element
    // If byteStride is not 0: the data may be interleaved with other data.
    // If 0: the data is packed, then we calculate its size.
    const size_t stride = (bv.byteStride != 0) ? static_cast<size_t>(bv.byteStride) : packedElementSize(accessor);

    // Pre-allocate memory for efficiency
    out.resize(static_cast<size_t>(accessor.count));

    // Iterate through the binary data and convert bytes to glm::vec2
    for (size_t i = 0; i < out.size(); ++i) {
        // Calculate the pointer to the current element in the buffer
        const float* f = reinterpret_cast<const float*>(base + i * stride);
        // Extract the two float values into the output vector
        out[i] = glm::vec2(f[0], f[1]);
    }
}

void GLTFLoader::readAccessorVec3f(const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<glm::vec3>& out) const {
    // Validation to only accept vec3
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3) {
        throw std::runtime_error("Expected VEC3 float accessor");
    }

    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    const unsigned char* base = getAccessorDataPtr(model, accessor);

    const size_t stride = (bv.byteStride != 0) ? static_cast<size_t>(bv.byteStride) : packedElementSize(accessor);
    out.resize(static_cast<size_t>(accessor.count));

    for (size_t i = 0; i < out.size(); ++i) {
        const float* f = reinterpret_cast<const float*>(base + i * stride);
        out[i] = glm::vec3(f[0], f[1], f[2]);
    }
}

// Reads mesh indices from a glTF accessor and converts them into a uniform 32-bit format (uint32_t)
// glTF allows indices to be stored as Bytes (8-bit), Shorts (16-bit), or Unsigned Ints (32-bit)
// to save file space. This function normalizes them all into a single 32-bit vector for the GPU
void GLTFLoader::readIndicesU32(const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<uint32_t>& out) const {
    // Validate that the accessor contains single numerical values
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        throw std::runtime_error("Index accessor must be SCALAR");
    }

    // Access the BufferView and get the raw memory pointer where the index data begins
    const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
    const unsigned char* base = getAccessorDataPtr(model, accessor);

    // The stride variable tells how much bytes should it skip to get to the next element
    // If byteStride is not 0: the data may be interleaved with other data.
    // If 0: the data is packed, then we calculate its size.
    const size_t stride = (bv.byteStride != 0) ? static_cast<size_t>(bv.byteStride) : packedElementSize(accessor);

    // Pre-allocate memory for efficiency
    out.resize(static_cast<size_t>(accessor.count));

    // glTF supports different bit-depths for indices. I have to handle each case
    // and cast the values to a uniform 32-bit unsigned integer
    switch (accessor.componentType) {
    // Case A: Indices are stored as 16-bit unsigned shorts (This is the common for most models)
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        for (size_t i = 0; i < out.size(); ++i) {
            // Locate the 16-bit value in memory
            const uint16_t* v = reinterpret_cast<const uint16_t*>(base + i * stride);
            // Convert 16-bit to 32-bit
            out[i] = static_cast<uint32_t>(*v);
        }
        break;
    // Case B: Indices are already stored as 32-bit unsigned integers (Large meshes)
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        for (size_t i = 0; i < out.size(); ++i) {
            // Locate the 32-bit value in memory
            const uint32_t* v = reinterpret_cast<const uint32_t*>(base + i * stride);
            // Direct copy since types match
            out[i] = *v;
        }
        break;
    // Case C: Indices are stored as 8-bit unsigned bytes (Small/Optimized meshes)
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        for (size_t i = 0; i < out.size(); ++i) {
            // Locate the 8-bit value in memory
            const uint8_t* v = reinterpret_cast<const uint8_t*>(base + i * stride);
            // Convert 8-bit to 32-bit
            out[i] = static_cast<uint32_t>(*v);
        }
        break;
    // Handle cases when the type is invalid
    default:
        throw std::runtime_error("Unsupported index componentType (expected UBYTE/USHORT/UINT)");
    }
}

// Function to convert glTF primitive data into a format suitable for the GPU
// If UV coordinates are missing it will call this function: "cubeProjectUV", which generates them using Box Projecion
void GLTFLoader::primitiveToMeshData(const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const LoadOptions& options,
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices) const
{
    // Position Data
    // Every 3D objects must have positions
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
        throw std::runtime_error("Primitive missing POSITION attribute");

    const tinygltf::Accessor& posAcc = model.accessors[posIt->second];
    std::vector<glm::vec3> positions;
    readAccessorVec3f(model, posAcc, positions);

    // Normal Data
    // Normals define how light interacts with the surface
    std::vector<glm::vec3> normals;
    bool hasNormals = false;
    auto nIt = primitive.attributes.find("NORMAL");
    if (nIt != primitive.attributes.end()) {
        const tinygltf::Accessor& nAcc = model.accessors[nIt->second];
        readAccessorVec3f(model, nAcc, normals);
        hasNormals = (normals.size() == positions.size());
    }

    // Texture Coordinates
    // Defines how a 2D image is wrapped around the 3D shape
    std::vector<glm::vec2> uvs;
    bool hasUVs = false;
    auto uvIt = primitive.attributes.find("TEXCOORD_0");
    if (uvIt != primitive.attributes.end()) {
        const tinygltf::Accessor& uvAcc = model.accessors[uvIt->second];
        readAccessorVec2f(model, uvAcc, uvs);
        hasUVs = (uvs.size() == positions.size());
    }

    // Index Data
    // If the mesh is indexed, read the indices 
    // If not, generate a linear sequence based on position count
    if (primitive.indices >= 0) {
        const tinygltf::Accessor& idxAcc = model.accessors[primitive.indices];
        readIndicesU32(model, idxAcc, outIndices);
    }
    else {
        outIndices.resize(positions.size());
        for (uint32_t i = 0; i < (uint32_t)positions.size(); ++i) outIndices[i] = i;
    }

    // Safety check, if it has valid geometry
    if (positions.empty() || outIndices.empty()) {
        outVertices.clear();
        outIndices.clear();
        return;
    }

    // Fallback, if no UV-s exists
    if (!hasUVs) {
        // Calculate the Bounding Box of the object for scaling the projection
        glm::vec3 pmin(std::numeric_limits<float>::max());
        glm::vec3 pmax(-std::numeric_limits<float>::max());
        for (auto& p : positions) { pmin = glm::min(pmin, p); pmax = glm::max(pmax, p); }

        std::vector<Vertex> expandedVerts;
        std::vector<uint32_t> expandedIdx;
        expandedVerts.reserve(outIndices.size());
        expandedIdx.reserve(outIndices.size());

        // Process triangles one by one (3 indices at a time)
        for (size_t i = 0; i + 2 < outIndices.size(); i += 3) {
            uint32_t i0 = outIndices[i + 0];
            uint32_t i1 = outIndices[i + 1];
            uint32_t i2 = outIndices[i + 2];
            if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) continue;

            glm::vec3 p0 = positions[i0];
            glm::vec3 p1 = positions[i1];
            glm::vec3 p2 = positions[i2];

            // Calculate a Flat Face Normal using the Cross Product of two edges
            glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(fn) > 0.0f) fn = glm::normalize(fn);
            else fn = glm::vec3(0, 1, 0); // Default up if triangle is degenerate

            uint32_t base = (uint32_t)expandedVerts.size();

            // Internal helper to create a vertex with generated UV-s
            auto pushV = [&](const glm::vec3& p) {
                Vertex v{};
                v.pos = p;
                v.col = glm::vec3(1, 1, 1); // Default white color

                // Call our Cube Projection function to create UVs based on position and normal
                glm::vec2 uv = cubeProjectUV(p, fn, pmin, pmax);
                if (options.flipV) uv.y = 1.0f - uv.y; // Apply vertical flip if requested
                v.tex = uv;

                v.normal = fn; // Use the calculated face normal
                expandedVerts.push_back(v);
                };

            pushV(p0); pushV(p1); pushV(p2);

            expandedIdx.push_back(base + 0);
            expandedIdx.push_back(base + 1);
            expandedIdx.push_back(base + 2);
        }

        if (expandedVerts.empty() || expandedIdx.empty()) {
            outVertices.clear();
            outIndices.clear();
            return;
        }

        // Replace original data with the procedurally generated one
        outVertices = std::move(expandedVerts);
        outIndices = std::move(expandedIdx);
        return;
    }

    // Mapping Existing Data
    // If the file already has UV-s, simply map everything into the Vertex structure
    outVertices.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        Vertex v{};
        v.pos = positions[i];
        v.col = glm::vec3(1.0f); // Default white color

        glm::vec2 uv = uvs[i];
        if (options.flipV) uv.y = 1.0f - uv.y; // Apply vertical flip if requested
        v.tex = uv;

        // Use provided normals, or fallback to a default Z-forward normal
        v.normal = hasNormals ? normals[i] : glm::vec3(0, 0, 1);
        outVertices[i] = v;
    }
}

// Function to resolve the texture ID for a given glTF primitive
// It traverses the material hierarchy and uses a callback to load the actual image file
// Includes a caching mechanism to avoid redundant texture uploads
int GLTFLoader::getPrimitiveTexId(const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const std::string& textureRootDirectory)
{
    // Validate texture callback fn, if not valid return the default texture
    if (!m_textureCreatefn)
        return m_defaultTexId;

    // Validate material index
    // If the primitive has no material or the index is out of bounds, use the fallback
    if (primitive.material < 0 ||
        primitive.material >= static_cast<int>(model.materials.size()))
    {
        return m_defaultTexId;
    }

    const tinygltf::Material& mat = model.materials[primitive.material];

    // Navigate the glTF Physically Based Rendering structure
    // I look for the "baseColorTexture"
    const int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;

    // Validate the texture index within the model's texture list
    if (texIndex < 0 || texIndex >= static_cast<int>(model.textures.size()))
        return m_defaultTexId;

  // Retrieve the texture and its source image index
    const tinygltf::Texture& tex = model.textures[texIndex];
    // Index pointing to the 'images' array
    const int imageIndex = tex.source;

    // Validate the image index
    if (imageIndex < 0 || imageIndex >= static_cast<int>(model.images.size()))
        return m_defaultTexId;

    // Caching: Check If this specific image has been already loaded
    auto it = m_imageToTexId.find(imageIndex);
    if (it != m_imageToTexId.end())
        return it->second; // Return the existing ID instead of loading again

    const tinygltf::Image& img = model.images[imageIndex];

    // Validate that the image has a valid file path
    if (img.uri.empty())
        return m_defaultTexId;

    // Path Resolution
    const std::string fullPath = joinPath(textureRootDirectory, img.uri);

    // Loading
    int texId = m_textureCreatefn(fullPath);

    // If the loader function failed, use the default texture ID
    if (texId < 0)
        texId = m_defaultTexId;

    // Store In Cache
    m_imageToTexId[imageIndex] = texId;

    return texId;
}

std::string GLTFLoader::joinPath(const std::string& root, const std::string& rel) {
    namespace fs = std::filesystem;

    // If the root is empty simply return the relative path as is to avoid unnecessary processing
    if (root.empty()) 
        return rel;

    // Create a filesystem path by joining root and rel.
    // The '/' operator is overloaded by std::filesystem::path to automatically 
    // insert the correct OS-specific separator (for example, '\' on Windows, '/' on Linux).
    fs::path p = fs::path(root) / fs::path(rel);

    // Normalize and return the path:
    // - lexically_normal(): Cleans up the path by resolving dots ('.') and 
    //   parent directories ('..'), and removing redundant slashes.
    return p.lexically_normal().string();
}