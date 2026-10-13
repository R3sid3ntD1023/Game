#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace gfx
{

    class Texture
    {
    public:
        static std::shared_ptr<Texture> FromFile(const std::string &path, bool srgb);
        static std::shared_ptr<Texture> FromColor(const glm::vec4 &color, bool srgb);

        Texture(const unsigned char *rgba, int width, int height, bool srgb);
        ~Texture();
        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        void Bind(GLuint unit) const { glBindTextureUnit(unit, m_id); }
        GLuint Id() const { return m_id; }

    private:
        GLuint m_id = 0;
    };

} // namespace gfx
