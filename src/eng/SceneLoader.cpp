#include "eng/SceneLoader.h"
#include "eng/BuiltinComponents.h"
#include "eng/RigidBody.h"
#include "eng/World.h"
#include "gfx/ModelLoader.h"
#include "gfx/Primitives.h"
#include <stdexcept>
#include <unordered_map>

namespace eng
{

    namespace
    {

        float Param(const std::vector<float> &p, size_t i, float fallback) { return i < p.size() ? p[i] : fallback; }

    } // namespace

    SceneLoader::SceneLoader(std::shared_ptr<gfx::Shader> shader) : m_shader(std::move(shader))
    {
        RegisterMesh("cube", [](const MeshDesc &d)
                     { return gfx::primitives::Cube(Param(d.params, 0, 0.5f)); });
        RegisterMesh("sphere", [](const MeshDesc &d)
                     { return gfx::primitives::Sphere(Param(d.params, 0, 0.5f)); });
        RegisterMesh("plane", [](const MeshDesc &d)
                     { return gfx::primitives::Plane(Param(d.params, 0, 1.0f), Param(d.params, 1, 1.0f)); });
        RegisterMesh("model", [](const MeshDesc &d)
                     { return gfx::LoadModel(std::string(ASSETS_DIR) + d.path, Param(d.params, 0, 1.0f)); });
    }

    void SceneLoader::Instantiate(World &world, const SceneData &scene) const
    {
        std::vector<gfx::MeshHandle> meshes;
        for (const MeshDesc &d : scene.meshes)
        {
            auto it = m_meshes.find(d.kind);
            if (it == m_meshes.end())
                throw std::runtime_error("Unknown mesh kind '" + d.kind + "' (mesh " + d.name + ")");
            meshes.push_back(world.Meshes().Create(it->second(d)));
        }

        std::unordered_map<std::string, std::shared_ptr<gfx::Texture>> textures;
        auto loadTexture = [&](const std::string &rel, bool srgb) -> std::shared_ptr<gfx::Texture>
        {
            if (rel.empty())
                return nullptr;
            auto &slot = textures[rel + (srgb ? "#srgb" : "#linear")];
            if (!slot)
                slot = gfx::Texture::FromFile(std::string(ASSETS_DIR) + rel, srgb);
            return slot;
        };

        std::vector<std::shared_ptr<gfx::Material>> materials;
        for (const MaterialDesc &d : scene.materials)
        {
            auto m = std::make_shared<gfx::Material>(m_shader);
            m->baseColor = glm::vec4(d.color, 1.0f);
            m->metallic = d.metallic;
            m->roughness = d.roughness;
            m->emissive = d.emissive;
            if (auto t = loadTexture(d.albedoMap, true))
                m->albedoMap = t;
            if (auto t = loadTexture(d.normalMap, false))
                m->normalMap = t;
            if (auto t = loadTexture(d.metalRoughMap, false))
                m->metalRoughMap = t;
            materials.push_back(std::move(m));
        }

        std::vector<GameObject *> created;
        created.reserve(scene.objects.size());
        for (const ObjectDesc &d : scene.objects)
        {
            GameObject &go = world.CreateGameObject(d.name, d.position);
            Transform &t = go.GetTransform();
            t.rotation = d.rotation;
            t.scale = d.scale;

            if (d.mesh >= 0 || d.material >= 0)
            {
                if (d.mesh < 0 || d.mesh >= static_cast<int32_t>(meshes.size()) ||
                    d.material < 0 || d.material >= static_cast<int32_t>(materials.size()))
                    throw std::runtime_error("Object '" + d.name + "' has an invalid mesh/material index");
                go.AddComponent<MeshRendererComponent>(meshes[d.mesh], materials[d.material]);
            }
            if (d.hasBody)
                go.AddComponent<RigidBody>(d.halfExtents, d.bodyStatic);
            if (d.health > 0.0f)
                go.AddComponent<Health>(d.health);
            created.push_back(&go);
        }

        for (size_t i = 0; i < scene.objects.size(); ++i)
        {
            const ObjectDesc &d = scene.objects[i];
            if (d.script.empty())
                continue;
            auto it = m_scripts.find(d.script);
            if (it == m_scripts.end())
                throw std::runtime_error("Unknown script '" + d.script + "' on object " + d.name);
            it->second(*created[i], d.scriptParams);
        }
    }

} // namespace eng
