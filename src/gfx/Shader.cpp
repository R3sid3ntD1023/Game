#include "gfx/Shader.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>

namespace gfx
{

    namespace
    {

        std::string ReadFile(const std::string &path)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f)
                throw std::runtime_error("Cannot open shader file: " + path);
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }

        GLuint Compile(GLenum type, const std::string &path)
        {
            const std::string src = ReadFile(path);
            const char *c = src.c_str();
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &c, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char log[2048];
                glGetShaderInfoLog(s, sizeof(log), nullptr, log);
                glDeleteShader(s);
                throw std::runtime_error("Shader compile failed (" + path + "):\n" + log);
            }
            return s;
        }

    } // namespace

    Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath)
    {
        GLuint vs = Compile(GL_VERTEX_SHADER, vertexPath);
        GLuint fs = Compile(GL_FRAGMENT_SHADER, fragmentPath);
        m_program = glCreateProgram();
        glAttachShader(m_program, vs);
        glAttachShader(m_program, fs);
        glLinkProgram(m_program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint ok = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[2048];
            glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("Program link failed:\n") + log);
        }
    }

    Shader::Shader(const std::string &computePath)
    {
        GLuint cs = Compile(GL_COMPUTE_SHADER, computePath);
        m_program = glCreateProgram();
        glAttachShader(m_program, cs);
        glLinkProgram(m_program);
        glDeleteShader(cs);
        GLint ok = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[2048];
            glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("Compute program link failed:\n") + log);
        }
    }

    Shader::~Shader()
    {
        if (m_program)
            glDeleteProgram(m_program);
    }

    GLint Shader::Loc(const char *name) const
    {
        auto it = m_locations.find(name);
        if (it != m_locations.end())
            return it->second;
        GLint loc = glGetUniformLocation(m_program, name);
        m_locations.emplace(name, loc);
        return loc;
    }

} // namespace gfx
