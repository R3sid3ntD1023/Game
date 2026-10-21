#pragma once
#include "gfx/Lights.h"
#include <array>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gfx
{

    // Shadow maps for a few point and spot lights. Every slot owns six layers of one 2D depth array:
    // a spot uses layer 0, a point light uses all six faces (+X -X +Y -Y +Z -Z) and the shader picks
    // the face from the dominant axis of the light-to-pixel vector.
    class LocalShadowMap
    {
    public:
        static constexpr int kSlots = 4;
        static constexpr int kFaces = 6;

        LocalShadowMap();
        ~LocalShadowMap();
        LocalShadowMap(const LocalShadowMap &) = delete;
        LocalShadowMap &operator=(const LocalShadowMap &) = delete;

        void Reset() { m_used = 0; }
        bool Full() const { return m_used >= kSlots; }
        // Fits the matrices for `light`, returns its slot and writes the number of faces to render.
        int Assign(const LightData &light, int &faceCount);

        // Render one face of a slot: Begin, draw depth with ViewProj, End.
        void Begin(int slot, int face);
        void End();
        const glm::mat4 &ViewProj(int slot, int face) const { return m_viewProj[slot * kFaces + face]; }

        // Uploads the matrices (UBO binding 1) and binds the depth array to texture unit 8.
        void Bind() const;

    private:
        static constexpr int kSize = 512;

        GLuint m_tex = 0;
        GLuint m_fbo = 0;
        GLuint m_ubo = 0;
        GLint m_prevFbo = 0;
        GLint m_prevViewport[4] = {};
        int m_used = 0;

        std::array<glm::mat4, kSlots * kFaces> m_viewProj{};
        std::array<glm::vec4, kSlots> m_info{};
    };

} // namespace gfx
