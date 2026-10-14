#include "gfx/ParticleRenderer.h"

namespace gfx {

ParticleRenderer::ParticleRenderer() : m_shader(SHADER_DIR "particle.vert", SHADER_DIR "particle.frag") {
    m_buffer.Upload(nullptr, 0);
}

void ParticleRenderer::Submit(const std::vector<GpuParticle>& particles, ParticleMode mode) {
    if (!particles.empty()) m_batches.push_back({ particles, mode });
}

void ParticleRenderer::Render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) {
    if (m_batches.empty()) return;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.Bind();
    m_shader.SetMat4("uViewProj", proj * view);
    m_shader.SetVec3("uCameraPos", cameraPos);
    m_shader.SetVec3("uCameraRight", { view[0][0], view[1][0], view[2][0] });
    m_shader.SetVec3("uCameraUp", { view[0][1], view[1][1], view[2][1] });
    m_vao.Bind();

    for (const Batch& b : m_batches) {
        m_buffer.Upload(b.particles.data(), b.particles.size() * sizeof(GpuParticle));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_buffer.Id());
        m_shader.SetInt("uStreak", static_cast<int>(b.mode));
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(b.particles.size()));
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

} // namespace gfx
