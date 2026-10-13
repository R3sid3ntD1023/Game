#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx {

struct Camera {
    glm::vec3 position{ 0.0f };
    glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
    float fovDegrees = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

    glm::mat4 View() const { return glm::lookAt(position, position + forward, glm::vec3(0, 1, 0)); }
    glm::mat4 Projection(float aspect) const {
        return glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
    }
};

} // namespace gfx
