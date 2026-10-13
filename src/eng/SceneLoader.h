#pragma once
#include "eng/GameObject.h"
#include "eng/Scene.h"
#include "gfx/Mesh.h"
#include "gfx/Shader.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eng
{

    class World;

    // Instantiates a SceneData into a World. Meshes and scripts are resolved by name, so games register their own.
    class SceneLoader
    {
    public:
        using MeshFactory = std::function<gfx::MeshData(const std::vector<float> &params)>;
        // Called after every object exists, so scripts can look other objects up by name.
        using ScriptFactory = std::function<void(GameObject &object, const std::vector<float> &params)>;

        // Registers the built-in "cube" (halfSize), "sphere" (radius) and "plane" (halfSize, uvScale) meshes.
        explicit SceneLoader(std::shared_ptr<gfx::Shader> shader);

        void RegisterMesh(const std::string &kind, MeshFactory factory) { m_meshes[kind] = std::move(factory); }
        void RegisterScript(const std::string &name, ScriptFactory factory) { m_scripts[name] = std::move(factory); }

        void Instantiate(World &world, const SceneData &scene) const;

    private:
        std::shared_ptr<gfx::Shader> m_shader;
        std::unordered_map<std::string, MeshFactory> m_meshes;
        std::unordered_map<std::string, ScriptFactory> m_scripts;
    };

} // namespace eng
