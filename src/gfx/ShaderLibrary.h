#pragma once
#include "gfx/Shader.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace gfx
{

    // Material shader types by name: "Standard" loads shaders/materials/Standard.vert/.frag, and so on.
    // Each is compiled once and shared by every material that names it.
    class ShaderLibrary
    {
    public:
        explicit ShaderLibrary(std::string directory) : m_dir(std::move(directory)) {}

        std::shared_ptr<Shader> Get(const std::string &name);

    private:
        std::string m_dir;
        std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
    };

} // namespace gfx
