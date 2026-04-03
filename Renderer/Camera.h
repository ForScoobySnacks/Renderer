#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    // Camera position
    glm::vec3 position{ 3.0f, 5.0f, 2.0f };
    glm::vec3 worldUp{ 0.0f, 1.0f, 0.0f };

    // Euler angles in degrees
    float yaw = -90.0f;  
    float pitch = 0.0f;

    // Movement and look sensitivity
    float moveSpeed = 5.0f;          
    float mouseSensitivity = 0.10f; 

    // Constructor for initializing the camera direction vectors based on default Euler angles
    Camera() { updateVectors(); }

    // Function to return the camera's coordinate space
    glm::mat4 getViewMatrix() const {
        // position == eye position, position + front == target position, up  == up vector
        return glm::lookAt(position, position + front, up);
    }

    // Method to translate the camera position
    void processKeyboard(bool w, bool a, bool s, bool d, float deltaTime, bool fast = false);
    // Method to update the camera look direction
    void processMouse(float xoffset, float yoffset);

private:
    glm::vec3 front{ 0.0f, 0.0f, -1.0f }; // Forward direction
    glm::vec3 right{ 1.0f, 0.0f, 0.0f }; // Rightward direction
    glm::vec3 up{ 0.0f, 1.0f, 0.0f }; // Local up direction

    // Method to recalculate direction vectors
    void updateVectors();
};