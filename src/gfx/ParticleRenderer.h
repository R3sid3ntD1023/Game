#pragma once
#include "gfx/Buffer.h"
#include "gfx/Shader.h"
#include "gfx/VertexArray.h"
#include <glm/glm.hpp>
#include <vector>

namespace gfx {

struct GpuParticle { // matches `Particle` in particle.vert
    glm::vec4 posSize; // xyz position, w size (width for streaks, diameter for rings)
    glm::vec4 vel;     // streaks: xyz direction, w length
    glm::vec4 color;
};

enum class ParticleMode : int {
    Billboard = 0, // camera-facing round droplet
    Streak = 1,    // rain streak along its velocity
    Ring = 2,      // flat ring on the ground (rain splash)
};

// Draws particle batches submitted by ParticleEmitter components as instanced quads,
// depth-tested and without depth writes.
class ParticleRenderer {
public:
    ParticleRenderer();

    void Begin() { m_batches.clear(); }
    void Submit(const std::vector<GpuParticle>& particles, ParticleMode mode);
    void Render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos);

private:
    struct Batch {
        std::vector<GpuParticle> particles;
        ParticleMode mode;
    };

    Shader m_shader;
    VertexArray m_vao; // empty; quads are generated from gl_VertexID
    GpuBuffer m_buffer;
    std::vector<Batch> m_batches;
};

} // namespace gfx
