#include "gfx/ShaderLibrary.h"

namespace gfx
{

    std::shared_ptr<Shader> ShaderLibrary::Get(const std::string &name)
    {
        const std::string key = name.empty() ? "Standard" : name;
        auto &slot = m_shaders[key];
        if (!slot)
            slot = std::make_shared<Shader>(m_dir + key + ".vert", m_dir + key + ".frag");
        return slot;
    }

} // namespace gfx
