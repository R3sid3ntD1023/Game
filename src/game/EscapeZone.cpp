#include "game/EscapeZone.h"
#include "eng/World.h"
#include <cmath>

namespace game
{

    void EscapeZone::OnUpdate(float dt)
    {
        m_time += dt;
        // Pulsing beacon; the HDR emissive values feed the bloom pass.
        auto &registry = gameObject->GetWorld().Registry();
        if (auto *r = registry.try_get<MeshRenderer>(gameObject->Entity()); r && r->material)
            r->material->emissive = glm::vec3(0.2f, 1.0f, 1.2f) * (4.0f + 2.0f * std::sin(m_time * 3.0f));

        glm::vec3 d = m_player.GetTransform().position - gameObject->GetTransform().position;
        d.y = 0.0f;
        if (glm::length(d) < m_radius && m_state.status == Status::Playing)
            m_state.status = Status::Won;
    }

} // namespace game
