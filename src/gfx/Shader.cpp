#include "gfx/Shader.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>

namespace gfx
{

    namespace
    {

        // Supports `#include "file"` (relative to the including file) since GLSL has no include of its own.
        std::string ReadFile(const std::string &path, int depth = 0)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f)
                throw std::runtime_error("Cannot open shader file: " + path);
            std::stringstream in;
            in << f.rdbuf();
            if (depth > 8)
                throw std::runtime_error("Shader include nesting too deep: " + path);

            const std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
            std::string out, line;
            while (std::getline(in, line))
            {
                const size_t p = line.find("#include");
                const size_t q1 = line.find('"');
                const size_t q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
                if (p != std::string::npos && q2 != std::string::npos)
                    out += ReadFile(dir + line.substr(q1 + 1, q2 - q1 - 1), depth + 1) + "\n";
                else
                    out += line + "\n";
            }
            return out;
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
