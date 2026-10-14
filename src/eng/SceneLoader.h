#pragma once
#include "eng/GameObject.h"
#include "eng/Scene.h"
#include "gfx/Mesh.h"
#include "gfx/Shader.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eng
{

    class World;

    // Instantiates a SceneData into a World. Meshes and scripts are resolved by name, so games register their own.
    class SceneLoader
    {
    public:
        using MeshFactory = std::function<gfx::MeshData(const MeshDesc &desc)>;
        // Called after every object exists, so scripts can look other objects up by name.
        using ScriptFactory = std::function<void(GameObject &object, const std::vector<float> &params)>;

        // Registers the built-in "cube" (halfSize), "sphere" (radius), "plane" (halfSize, uvScale) and
        // "model" (path via MeshDesc::path, params[0] = uniform scale; loaded with assimp) meshes.
        explicit SceneLoader(std::shared_ptr<gfx::Shader> shader);

        // Every kind is replaced by assets/models/<kind>.(glb|gltf|obj|fbx|dae) when that file exists, unless
        // registered with overridable = false (the factory then handles overrides itself).
        void RegisterMesh(const std::string &kind, MeshFactory factory, bool overridable = true)
        {
            m_meshes[kind] = std::move(factory);
            if (!overridable)
                m_fixedMeshes.insert(kind);
        }
        void RegisterScript(const std::string &name, ScriptFactory factory) { m_scripts[name] = std::move(factory); }

        void Instantiate(World &world, const SceneData &scene) const;

    private:
        std::shared_ptr<gfx::Shader> m_shader;
        std::unordered_map<std::string, MeshFactory> m_meshes;
        std::unordered_set<std::string> m_fixedMeshes;
        std::unordered_map<std::string, ScriptFactory> m_scripts;
    };

} // namespace eng
