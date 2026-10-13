#pragma once
#include "eng/Component.h"
#include <glm/glm.hpp>

namespace eng {

// Axis-aligned box body. Static bodies act as colliders; dynamic ones are moved by PhysicsWorld.
// The GameObject's Transform position is the box center.
class RigidBody : public Component {
public:
    RigidBody(const glm::vec3& halfExtents, bool isStatic = false) : halfExtents(halfExtents), isStatic(isStatic) {}

    void OnAttach() override;
    void OnDestroy() override;

    glm::vec3 velocity{ 0.0f };
    glm::vec3 halfExtents;
    bool isStatic;
    bool useGravity = true;
    bool grounded = false;
};

} // namespace eng
