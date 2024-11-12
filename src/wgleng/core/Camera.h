#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    void Update(int32_t viewportWidth, int32_t viewportHeight);

    const glm::mat4& GetProjectionMatrix() const { return m_projectionMatrix; }
    const glm::mat4& GetViewMatrix() const { return m_viewMatrix; }

    void SetNearPlane(float near) { m_near = near; }
    void SetFarPlane(float far) { m_far = far; }
    float GetNearPlane() const { return m_near; }
    float GetFarPlane() const { return m_far; }

    void SetFov(float fov) { m_fov = fov; }
    float GetFov() const { return m_fov; }

    float GetAspectRatio() const { return m_aspectRatio; }

    void SetRotation(float yaw, float pitch);
    void Rotate(float yaw, float pitch);
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }

    const glm::vec3& GetFront() const { return m_front; }
    const glm::vec3& GetUp() const { return m_up; }

    glm::vec3 position{0, 0, 0};

private:
    float m_fov = 60;
    float m_yaw = 0;
    float m_pitch = 0;

    float m_near = 1.0f;
    float m_far = 10000.f;
    float m_aspectRatio = 1;

    glm::vec3 m_front{1, 0, 0};
    glm::vec3 m_up{0, 1, 0};

    glm::mat4 m_projectionMatrix{};
    glm::mat4 m_viewMatrix{};
};