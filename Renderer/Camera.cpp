#include "Camera.h"
#include <algorithm>
#include <cmath>

void Camera::processKeyboard(bool w, bool a, bool s, bool d, float deltaTime, bool fast)
{
    float speed = moveSpeed * (fast ? 3.0f : 1.0f);
    float v = speed * deltaTime;

    // Apply translation along the local axes
    if (w) position += front * v;
    if (s) position -= front * v;
    if (a) position -= right * v;
    if (d) position += right * v;
}

void Camera::processMouse(float xoffset, float yoffset)
{
    // Apply sensitivity scaling
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Prevent Gimbal lock
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    // Update the 3D vectors
    updateVectors();
}

void Camera::updateVectors()
{
    // Convert degrees to radians
    float cosYaw = std::cos(glm::radians(yaw));
    float sinYaw = std::sin(glm::radians(yaw));
    float cosPitch = std::cos(glm::radians(pitch));
    float sinPitch = std::sin(glm::radians(pitch));

    // Calculate new front vector components
    glm::vec3 frontLocal;
    frontLocal.x = cosYaw * cosPitch;
    frontLocal.y = sinPitch;
    frontLocal.z = sinYaw * cosPitch;

    // Normalize to ensure constant speed
    front = glm::normalize(frontLocal);
    // Right is calculated to be at a 90 degree angle to both front and worldUp
    right = glm::normalize(glm::cross(front, worldUp));
    // Up is calculated to be at a 90 degree angle to both right and front
    up = glm::normalize(glm::cross(right, front));
}