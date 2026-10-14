#pragma once
#include "gfx/Material.h"
#include "gfx/Lights.h"
#include "gfx/Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

struct Transform {
    glm::vec3 position{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f };

    glm::mat4 Matrix() const {
        return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
    }
};

struct MeshRenderer {
    gfx::MeshHandle mesh = 0;
    std::shared_ptr<gfx::Material> material;
};

// Final skinning matrices (global * inverseBind) written by Animator and consumed by RenderSystem.
struct SkinnedPose {
    std::vector<glm::mat4> matrices;
};

// Dynamic light attached to an entity (gathered by World each frame). Direction comes from the Transform's -Z axis.
struct Light {
    gfx::LightType type = gfx::LightType::Point;
    glm::vec3 color{ 1.0f };
    float intensity = 10.0f;
    float range = 18.0f;
    float innerDegrees = 12.0f; // spot only
    float outerDegrees = 22.0f;
    bool enabled = true;
};

// Per-instance emissive multiplier, used by flickering lamps.
struct EmissiveScale {
    float value = 1.0f;
};
