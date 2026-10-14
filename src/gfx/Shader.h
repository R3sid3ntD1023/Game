#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace gfx
{

    class Shader
    {
    public:
        Shader(const std::string &vertexPath, const std::string &fragmentPath);
        explicit Shader(const std::string &computePath);
        ~Shader();
        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;

        void Bind() const { glUseProgram(m_program); }

        void SetInt(const char *name, int v) const { glProgramUniform1i(m_program, Loc(name), v); }
        void SetFloat(const char *name, float v) const { glProgramUniform1f(m_program, Loc(name), v); }
        void SetVec2(const char *name, const glm::vec2 &v) const { glProgramUniform2fv(m_program, Loc(name), 1, &v[0]); }
        void SetVec3(const char *name, const glm::vec3 &v) const { glProgramUniform3fv(m_program, Loc(name), 1, &v[0]); }
        void SetVec4Array(const char *name, const glm::vec4 *v, int count) const { glProgramUniform4fv(m_program, Loc(name), count, &v[0][0]); }
        void SetVec4(const char *name, const glm::vec4 &v) const { glProgramUniform4fv(m_program, Loc(name), 1, &v[0]); }
        void SetMat4(const char *name, const glm::mat4 &v) const { glProgramUniformMatrix4fv(m_program, Loc(name), 1, GL_FALSE, &v[0][0]); }

    private:
        GLint Loc(const char *name) const;

        GLuint m_program = 0;
        mutable std::unordered_map<std::string, GLint> m_locations;
    };

} // namespace gfx
