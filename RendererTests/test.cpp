#include "pch.h"
#include "../Renderer/Utilities.h"
#define private public
#include "../Renderer/Mesh.cpp"
#include "../Renderer/GLTFLoader.cpp"
#include "../Renderer/Camera.cpp"
#undef private

class CameraTest : public ::testing::Test {
protected:
    Camera cam;

    void SetUp() override {
        cam.position = glm::vec3(0.0f, 0.0f, 0.0f);
        cam.yaw = -90.0f;
        cam.pitch = 0.0f;
        cam.updateVectors();
    }
};

class GLTFLoaderTest : public ::testing::Test {
protected:
    GLTFLoader loader{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, 0 };
};

TEST_F(CameraTest, InitialOrientation) {
    EXPECT_NEAR(cam.front.x, 0.0f, 0.0001f);
    EXPECT_NEAR(cam.front.y, 0.0f, 0.0001f);
    EXPECT_NEAR(cam.front.z, -1.0f, 0.0001f);
}

TEST_F(CameraTest, KeyboardMovementForward) {
    float deltaTime = 1.0f;
    float expectedDistance = cam.moveSpeed * deltaTime;

    cam.processKeyboard(true, false, false, false, deltaTime, false);

    EXPECT_NEAR(cam.position.z, -expectedDistance, 0.0001f);
    EXPECT_NEAR(cam.position.x, 0.0f, 0.0001f);
    EXPECT_NEAR(cam.position.y, 0.0f, 0.0001f);
}

TEST_F(CameraTest, KeyboardFastMovement) {
    float deltaTime = 1.0f;
    float expectedDistance = cam.moveSpeed * 3.0f * deltaTime;

    cam.processKeyboard(true, false, false, false, deltaTime, true);

    EXPECT_NEAR(cam.position.z, -expectedDistance, 0.0001f);
}

TEST_F(CameraTest, MouseRotationYawAndPitch) {
    float xOff = 100.0f;
    float yOff = 50.0f;

    float expectedYaw = -90.0f + (xOff * cam.mouseSensitivity);
    float expectedPitch = 0.0f + (yOff * cam.mouseSensitivity);

    cam.processMouse(xOff, yOff);

    EXPECT_NEAR(cam.yaw, expectedYaw, 0.0001f);
    EXPECT_NEAR(cam.pitch, expectedPitch, 0.0001f);
}

TEST_F(CameraTest, MouseLookClamping) {
    cam.processMouse(0.0f, 1000.0f);
    EXPECT_LE(cam.pitch, 89.0f);

    cam.processMouse(0.0f, -2000.0f);
    EXPECT_GE(cam.pitch, -89.0f);
}

TEST_F(CameraTest, VectorOrthogonality) {
    cam.processMouse(45.0f, 30.0f);

    float dotFrontRight = glm::dot(cam.front, cam.right);
    float dotFrontUp = glm::dot(cam.front, cam.up);
    float dotRightUp = glm::dot(cam.right, cam.up);

    EXPECT_NEAR(dotFrontRight, 0.0f, 0.0001f);
    EXPECT_NEAR(dotFrontUp, 0.0f, 0.0001f);
    EXPECT_NEAR(dotRightUp, 0.0f, 0.0001f);
}

TEST_F(CameraTest, ViewMatrixChangesWithPosition) {
    glm::mat4 viewInitial = cam.getViewMatrix();
    cam.position = glm::vec3(10.0f, 10.0f, 10.0f);
    glm::mat4 viewMoved = cam.getViewMatrix();

    bool isSame = true;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (std::abs(viewInitial[i][j] - viewMoved[i][j]) > 0.0001f) isSame = false;
        }
    }
    EXPECT_FALSE(isSame);
}

TEST_F(CameraTest, StrafeMovement) {
    float deltaTime = 1.0f;
    cam.processKeyboard(false, false, false, true, deltaTime, false);
    EXPECT_NEAR(cam.position.x, cam.moveSpeed, 0.0001f);
}

TEST(UtilitiesTest, ReadFileContentValidation) {
    std::string testFile = "binary_test.bin";
    std::vector<char> expectedData = { 'H', 'e', 'l', 'l', 'o', 'V', 'u', 'l', 'k', 'a', 'n' };

    std::ofstream out(testFile, std::ios::binary);
    out.write(expectedData.data(), expectedData.size());
    out.close();

    auto buffer = readFile(testFile);

    EXPECT_EQ(buffer.size(), expectedData.size());

    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i], expectedData[i]);
    }
    std::remove(testFile.c_str());
}

TEST(UtilitiesTest, QueueFamilyIndicesLogic) {
    QueueFamilyIndices indices;

    EXPECT_FALSE(indices.isValid());

    indices.graphicsFamily = 0;
    EXPECT_FALSE(indices.isValid());

    indices.presentationFamily = 0;
    EXPECT_TRUE(indices.isValid());

    indices.graphicsFamily = -1;
    EXPECT_FALSE(indices.isValid());
}

TEST(UtilitiesTest, MemoryTypeSearchLogicSimulation) {
    VkPhysicalDeviceMemoryProperties memProps{};
    memProps.memoryTypeCount = 2;
    memProps.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    memProps.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t typeFilter = 0b11;
    VkMemoryPropertyFlags goal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t result = -1;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & goal) == goal) {
            result = i;
            break;
        }
    }
    EXPECT_EQ(result, 1);
}

TEST_F(GLTFLoaderTest, JoinPathNormalization) {
    EXPECT_EQ(GLTFLoader::joinPath("assets/models", "texture.png"), "assets/models/texture.png");
    EXPECT_EQ(GLTFLoader::joinPath("assets/models/", "texture.png"), "assets/models/texture.png");
    EXPECT_EQ(GLTFLoader::joinPath("assets/bin", "../textures/tex.png"), "assets/textures/tex.png");
    EXPECT_EQ(GLTFLoader::joinPath("", "standalone.gltf"), "standalone.gltf");
}

TEST_F(GLTFLoaderTest, ToMat4Conversion) {
    std::vector<double> testMatrix = {
        2.0, 0.0, 0.0, 0.0,
        0.0, 3.0, 0.0, 0.0,
        0.0, 0.0, 4.0, 0.0,
        1.0, 2.0, 3.0, 1.0
    };
    glm::mat4 result = GLTFLoader::toMat4(testMatrix);

    EXPECT_FLOAT_EQ(result[0][0], 2.0f);
    EXPECT_FLOAT_EQ(result[1][1], 3.0f);
    EXPECT_FLOAT_EQ(result[2][2], 4.0f);
    EXPECT_FLOAT_EQ(result[3][0], 1.0f);
    EXPECT_FLOAT_EQ(result[3][1], 2.0f);
    EXPECT_FLOAT_EQ(result[3][2], 3.0f);
}

TEST_F(GLTFLoaderTest, GetNodeLocalMatrixFromTRS) {
    tinygltf::Node node;
    node.translation = { 10.0, 20.0, 30.0 };
    node.scale = { 2.0, 2.0, 2.0 };
    node.rotation = { 0.0, 0.70710678, 0.0, 0.70710678 };

    glm::mat4 m = GLTFLoader::getNodeLocalMatrix(node);

    EXPECT_FLOAT_EQ(m[3][0], 10.0f);
    EXPECT_FLOAT_EQ(m[3][1], 20.0f);
    EXPECT_FLOAT_EQ(m[3][2], 30.0f);
    EXPECT_NEAR(glm::length(glm::vec3(m[0])), 2.0f, 0.0001f);
}

TEST_F(GLTFLoaderTest, ComponentTypeSizeMapping) {
    EXPECT_EQ(componentTypeSizeInBytes(TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT), 2);
    EXPECT_EQ(componentTypeSizeInBytes(TINYGLTF_COMPONENT_TYPE_FLOAT), 4);
    EXPECT_EQ(componentTypeSizeInBytes(TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT), 4);
}

TEST_F(GLTFLoaderTest, ReadIndicesNormalization) {
    tinygltf::Model model;
    tinygltf::Buffer buffer;
    buffer.data = { 0, 0, 5, 0, 10, 0 };
    model.buffers.push_back(buffer);

    tinygltf::BufferView bufferView;
    bufferView.buffer = 0;
    bufferView.byteOffset = 0;
    bufferView.byteStride = 0;
    model.bufferViews.push_back(bufferView);

    tinygltf::Accessor acc;
    acc.bufferView = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    acc.type = TINYGLTF_TYPE_SCALAR;
    acc.count = 3;

    std::vector<uint32_t> outIndices;
    loader.readIndicesU32(model, acc, outIndices);

    ASSERT_EQ(outIndices.size(), 3);
    EXPECT_EQ(outIndices[0], 0);
    EXPECT_EQ(outIndices[1], 5);
    EXPECT_EQ(outIndices[2], 10);
}

TEST_F(GLTFLoaderTest, GetAccessorDataPtrMath) {
    tinygltf::Model model;
    tinygltf::Buffer buffer;

    for (int i = 0; i < 100; ++i) 
        buffer.data.push_back((unsigned char)i);

    model.buffers.push_back(buffer);

    tinygltf::BufferView bufferView;
    bufferView.buffer = 0;
    bufferView.byteOffset = 20;
    model.bufferViews.push_back(bufferView);

    tinygltf::Accessor accessor;
    accessor.bufferView = 0;
    accessor.byteOffset = 5;

    const unsigned char* ptr = loader.getAccessorDataPtr(model, accessor);

    EXPECT_EQ(*ptr, 25);
}

TEST_F(GLTFLoaderTest, CubeProjectUV_TopFace) {
    glm::vec3 pos(0.5f, 1.0f, 0.8f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 pmin(0.0f, 0.0f, 0.0f);
    glm::vec3 pmax(1.0f, 1.0f, 1.0f);

    glm::vec2 uv = cubeProjectUV(pos, normal, pmin, pmax);

    EXPECT_FLOAT_EQ(uv.x, 0.5f);
    EXPECT_FLOAT_EQ(uv.y, 0.8f);
}

TEST_F(GLTFLoaderTest, CubeProjectUV_SideFaceInversion) {
    glm::vec3 pos(1.0f, 0.5f, 0.5f);
    glm::vec3 normal(1.0f, 0.0f, 0.0f);
    glm::vec3 pmin(0.0f, 0.0f, 0.0f);
    glm::vec3 pmax(1.0f, 1.0f, 1.0f);

    glm::vec2 uv = cubeProjectUV(pos, normal, pmin, pmax);

    EXPECT_FLOAT_EQ(uv.x, 0.5f);
    EXPECT_FLOAT_EQ(uv.y, 0.5f);
}

TEST_F(GLTFLoaderTest, TextureCachingMechanism) {
    int callbackCount = 0;
    auto mockLoader = [&](const std::string& path) -> int {
        callbackCount++;
        return 500;
    };

    loader.m_textureCreatefn = mockLoader;

    tinygltf::Model model;

    tinygltf::Image img; 
    img.uri = "test.png";

    model.images.push_back(img);

    tinygltf::Texture tex; 
    tex.source = 0;

    model.textures.push_back(tex);

    tinygltf::Material mat; 
    mat.pbrMetallicRoughness.baseColorTexture.index = 0;

    model.materials.push_back(mat);

    tinygltf::Primitive primitive; 
    primitive.material = 0;

    int id1 = loader.getPrimitiveTexId(model, primitive, "root/");
    EXPECT_EQ(id1, 500);
    EXPECT_EQ(callbackCount, 1);

    int id2 = loader.getPrimitiveTexId(model, primitive, "root/");
    EXPECT_EQ(id2, 500);
    EXPECT_EQ(callbackCount, 1);
}

TEST(MeshTest, TransformationState) {
    Mesh mesh;
    glm::mat4 testMatrix = glm::rotate(glm::mat4(1.0f), 1.0f, glm::vec3(1, 0, 0));

    mesh.setModel(testMatrix);
    EXPECT_EQ(mesh.getModel().model, testMatrix);
}

TEST(MeshTest, PropertyGetters) {
    Mesh mesh;
    mesh.vertexCount = 42;
    mesh.indexCount = 128;
    mesh.texId = 7;

    EXPECT_EQ(mesh.getVertexCount(), 42);
    EXPECT_EQ(mesh.getIndexCount(), 128);
    EXPECT_EQ(mesh.getTexId(), 7);
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}