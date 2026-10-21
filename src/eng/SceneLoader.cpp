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

		float Param(const std::vector<float> &p, size_t i, float fallback)
		{
			return i < p.size() ? p[i] : fallback;
		}

	} // namespace

	SceneLoader::SceneLoader(std::shared_ptr<gfx::ShaderLibrary> shaders)
		: m_shaders(std::move(shaders))
	{
		RegisterMesh("cube", [](const MeshDesc &d) { return gfx::primitives::Cube(Param(d.params, 0, 0.5f)); });
		RegisterMesh("sphere", [](const MeshDesc &d) { return gfx::primitives::Sphere(Param(d.params, 0, 0.5f)); });
		RegisterMesh("plane", [](const MeshDesc &d) { return gfx::primitives::Plane(Param(d.params, 0, 1.0f), Param(d.params, 1, 1.0f)); });
		RegisterMesh("model", [](const MeshDesc &d) { return gfx::LoadModel(std::string(ASSETS_DIR) + d.path, Param(d.params, 0, 1.0f)); });
	}

	void SceneLoader::Instantiate(World &world, const SceneData &scene) const
	{
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
		auto loadSource = [&](const gfx::TextureSource &src, bool srgb) -> std::shared_ptr<gfx::Texture>
		{
			if (src.Empty())
				return nullptr;
			auto &slot = textures[src.key + (srgb ? "#srgb" : "#linear")];
			if (!slot)
				slot = src.encoded.empty() ? gfx::Texture::FromFile(src.path, srgb)
										   : gfx::Texture::FromMemory(src.encoded.data(), src.encoded.size(), srgb);
			return slot;
		};

		// One renderable piece of a mesh entry. Imported models have one part per material.
		struct Part
		{
			gfx::MeshHandle mesh;
			std::shared_ptr<gfx::Material> material; // from the model file; null for generated meshes
		};
		std::vector<std::vector<Part>> meshParts;
		for (const MeshDesc &d : scene.meshes)
		{
			auto it = m_meshes.find(d.kind);
			if (it == m_meshes.end())
				throw std::runtime_error("Unknown mesh kind '" + d.kind + "' (mesh " + d.name + ")");

			if (d.kind == "model" && d.importMaterials)
			{
				gfx::ModelAsset asset = gfx::LoadModelAsset(std::string(ASSETS_DIR) + d.path, Param(d.params, 0, 1.0f));
				std::vector<std::shared_ptr<gfx::Material>> imported;
				for (const gfx::ModelMaterial &mm : asset.materials)
				{
					auto m = std::make_shared<gfx::Material>(m_shaders->Get("Standard"));
					m->baseColor = mm.baseColor;
					m->metallic = mm.metallic;
					m->roughness = mm.roughness;
					m->emissive = mm.emissive;
					m->alphaCutoff = mm.alphaCutoff;
					m->doubleSided = mm.doubleSided;
					m->windStrength = mm.alphaCutoff > 0.0f ? 0.25f : 0.0f; // leaf cards sway; trunks and rocks stay rigid
					if (auto t = loadSource(mm.albedo, true))
						m->albedoMap = t;
					if (auto t = loadSource(mm.normal, false))
						m->normalMap = t;
					if (auto t = loadSource(mm.metalRough, false))
						m->metalRoughMap = t;
					imported.push_back(std::move(m));
				}
				std::vector<Part> parts;
				for (gfx::ModelPart &p : asset.parts)
					parts.push_back({world.Meshes().Create(p.mesh), p.material >= 0 && p.material < static_cast<int>(imported.size()) ? imported[p.material] : nullptr});
				meshParts.push_back(std::move(parts));
				continue;
			}

			if (d.kind != "model" && !m_fixedMeshes.count(d.kind))
			{
				const std::string external = gfx::FindModelOverride(d.kind);
				if (!external.empty())
				{
					meshParts.push_back({{world.Meshes().Create(gfx::LoadModel(external)), nullptr}});
					continue;
				}
			}
			meshParts.push_back({{world.Meshes().Create(it->second(d)), nullptr}});
		}

		std::vector<std::shared_ptr<gfx::Material>> materials;
		for (const MaterialDesc &d : scene.materials)
		{
			auto m = std::make_shared<gfx::Material>(m_shaders->Get(d.shader));
			m->baseColor = glm::vec4(d.color, 1.0f);
			m->metallic = d.metallic;
			m->roughness = d.roughness;
			m->emissive = d.emissive;
			m->windStrength = d.windStrength;
			m->doubleSided = d.doubleSided;
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
				if (d.mesh < 0 || d.mesh >= static_cast<int32_t>(meshParts.size()) || d.material >= static_cast<int32_t>(materials.size()))
					throw std::runtime_error("Object '" + d.name + "' has an invalid mesh/material index");

				// Extra parts become sibling objects with the same transform (static props; they don't follow later moves).
				const auto &parts = meshParts[d.mesh];
				for (size_t k = 0; k < parts.size(); ++k)
				{
					std::shared_ptr<gfx::Material> material = d.material >= 0 ? materials[d.material] : parts[k].material;
					if (!material)
						throw std::runtime_error("Object '" + d.name + "' has no material");

					GameObject *target = &go;
					if (k > 0)
					{
						target = &world.CreateGameObject(d.name + "_part" + std::to_string(k), d.position);
						target->GetTransform().rotation = d.rotation;
						target->GetTransform().scale = d.scale;
					}
					target->AddComponent<MeshRendererComponent>(parts[k].mesh, material);
				}
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

			if (d.bodyStatic)
			{
				// update navigation grid with the new static obstacle
				world.AddNavigationObstacle(d.position, d.halfExtents);
			}

			if (d.script.empty())
				continue;
			auto it = m_scripts.find(d.script);
			if (it == m_scripts.end())
				throw std::runtime_error("Unknown script '" + d.script + "' on object " + d.name);
			it->second(*created[i], d.scriptParams);
		}
	}

} // namespace eng
