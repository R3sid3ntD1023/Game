#include "editor/SceneView3D.h"
#include "editor/PreviewMesh.h"
#include "eng/AssetMeta.h"
#include "eng/DayNight.h"
#include "eng/Weather.h"
#include "gfx/ModelLoader.h"
#include "game/IslandScene.h"
#include "gfx/Primitives.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <vector>

namespace editor
{

	namespace
	{

		GLuint MakeBuffer(GLenum target, const void *data, size_t bytes)
		{
			GLuint b = 0;
			glGenBuffers(1, &b);
			glBindBuffer(target, b);
			glBufferData(target, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
			return b;
		}

		void Upload(const gfx::MeshData &data, GLuint &vao, GLuint &vbo, GLuint &ebo)
		{
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
			vbo = MakeBuffer(GL_ARRAY_BUFFER, data.vertices.data(), data.vertices.size() * sizeof(gfx::Vertex));
			ebo = MakeBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indices.data(), data.indices.size() * sizeof(uint32_t));
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(gfx::Vertex), reinterpret_cast<void *>(offsetof(gfx::Vertex, position)));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(gfx::Vertex), reinterpret_cast<void *>(offsetof(gfx::Vertex, normal)));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(gfx::Vertex), reinterpret_cast<void *>(offsetof(gfx::Vertex, uv)));
			glBindVertexArray(0);
		}

		// Slab test; returns the ray parameter of the entry point, or a negative value on a miss.
		float RayBox(const glm::vec3 &origin, const glm::vec3 &dir, const glm::vec3 &lo, const glm::vec3 &hi)
		{
			float tMin = 0.0f, tMax = std::numeric_limits<float>::max();
			for (int i = 0; i < 3; ++i)
			{
				if (std::abs(dir[i]) < 1e-8f)
				{
					if (origin[i] < lo[i] || origin[i] > hi[i])
						return -1.0f;
					continue;
				}
				float t0 = (lo[i] - origin[i]) / dir[i];
				float t1 = (hi[i] - origin[i]) / dir[i];
				if (t0 > t1)
					std::swap(t0, t1);
				tMin = std::max(tMin, t0);
				tMax = std::min(tMax, t1);
				if (tMin > tMax)
					return -1.0f;
			}
			return tMin;
		}

		bool IsBackdrop(const eng::SceneData &scene, const eng::ObjectDesc &o)
		{
			if (o.mesh < 0 || o.mesh >= static_cast<int>(scene.meshes.size()))
				return false;
			const std::string &k = scene.meshes[o.mesh].kind;
			return k == game::kMeshTerrain || k == "plane";
		}

	} // namespace

	SceneView3D::SceneView3D()
		: m_shader(std::string(SHADER_DIR) + "editor_mesh.vert", std::string(SHADER_DIR) + "editor_mesh.frag"),
		  m_tonemap(std::string(SHADER_DIR) + "fullscreen.vert", std::string(SHADER_DIR) + "editor_tonemap.frag")
	{
		glGenVertexArrays(1, &m_emptyVao);
		const gfx::MeshData cube = gfx::primitives::Cube(0.5f);
		Upload(cube, m_cube.vao, m_cube.vbo, m_cube.ebo);
		m_cube.indexCount = static_cast<GLsizei>(cube.indices.size());
		m_cube.bounds = cube.ComputeBounds();
		m_cube.valid = true;

		const gfx::MeshData sphere = gfx::primitives::Sphere(1.0f, 24, 12);
		Upload(sphere, m_sphere.vao, m_sphere.vbo, m_sphere.ebo);
		m_sphere.indexCount = static_cast<GLsizei>(sphere.indices.size());
		m_sphere.bounds = sphere.ComputeBounds();
		m_sphere.valid = true;
	}

	SceneView3D::~SceneView3D()
	{
		Invalidate();
		glDeleteVertexArrays(1, &m_sphere.vao);
		glDeleteBuffers(1, &m_sphere.vbo);
		glDeleteBuffers(1, &m_sphere.ebo);
		glDeleteVertexArrays(1, &m_cube.vao);
		glDeleteBuffers(1, &m_cube.vbo);
		glDeleteBuffers(1, &m_cube.ebo);
		glDeleteFramebuffers(1, &m_fbo);
		glDeleteFramebuffers(1, &m_hdrFbo);
		glDeleteTextures(1, &m_color);
		glDeleteTextures(1, &m_hdrColor);
		glDeleteRenderbuffers(1, &m_depth);
		glDeleteVertexArrays(1, &m_emptyVao);
	}

	void SceneView3D::FreeMesh(GpuMesh &m)
	{
		for (GpuMesh &part : m.parts)
			FreeMesh(part);
		glDeleteVertexArrays(1, &m.vao);
		glDeleteBuffers(1, &m.vbo);
		glDeleteBuffers(1, &m.ebo);
	}

	void SceneView3D::Invalidate()
	{
		for (auto &[index, m] : m_meshes)
			FreeMesh(m);
		m_meshes.clear();
	}

	void SceneView3D::LoadModelParts(const eng::MeshDesc &desc, GpuMesh &gpu)
	{
		const float scale = desc.params.empty() ? 1.0f : desc.params[0];
		const gfx::ModelAsset asset = gfx::LoadModelAsset(eng::ResolveAssetPath(desc.path), scale);
		const auto loadTexture = [](const gfx::TextureSource &src) -> std::shared_ptr<gfx::Texture>
		{
			if (src.Empty())
				return nullptr;
			try
			{
				return src.encoded.empty() ? gfx::Texture::FromFile(src.path, true) : gfx::Texture::FromMemory(src.encoded.data(), src.encoded.size(), true);
			}
			catch (const std::exception &e)
			{
				spdlog::warn("Model texture '{}' failed: {}", src.key, e.what());
				return nullptr;
			}
		};
		std::unordered_map<std::string, std::shared_ptr<gfx::Texture>> textures;
		for (const gfx::ModelPart &part : asset.parts)
		{
			if (part.mesh.indices.empty())
				continue;
			GpuMesh p;
			Upload(part.mesh, p.vao, p.vbo, p.ebo);
			p.indexCount = static_cast<GLsizei>(part.mesh.indices.size());
			p.bounds = part.mesh.ComputeBounds();
			p.valid = true;
			if (part.material >= 0 && part.material < static_cast<int>(asset.materials.size()))
			{
				const gfx::ModelMaterial &mat = asset.materials[part.material];
				p.tint = glm::vec3(mat.baseColor);
				p.emissive = mat.emissive;
				if (!mat.albedo.Empty())
				{
					auto &slot = textures[mat.albedo.key];
					if (!slot)
						slot = loadTexture(mat.albedo);
					p.albedo = slot;
				}
			}
			gpu.parts.push_back(std::move(p));
		}
		gpu.bounds = asset.bounds;
		gpu.valid = !gpu.parts.empty() && gpu.bounds.Valid();
	}

	const SceneView3D::GpuMesh &SceneView3D::MeshFor(const eng::SceneData &scene, int index, const game::Terrain &terrain)
	{
		auto it = m_meshes.find(index);
		if (it != m_meshes.end())
			return it->second;

		GpuMesh gpu;
		try
		{
			if (scene.meshes[index].kind == "model")
				LoadModelParts(scene.meshes[index], gpu);
			else
			{
				const gfx::MeshData data = BuildPreviewMesh(scene.meshes[index], terrain);
				Upload(data, gpu.vao, gpu.vbo, gpu.ebo);
				gpu.indexCount = static_cast<GLsizei>(data.indices.size());
				gpu.bounds = data.ComputeBounds();
				gpu.valid = gpu.indexCount > 0 && gpu.bounds.Valid();
			}
		}
		catch (const std::exception &e)
		{
			spdlog::warn("Editor preview of mesh '{}' failed: {}", scene.meshes[index].name, e.what());
		}
		return m_meshes.emplace(index, gpu).first->second;
	}

	void SceneView3D::EnsureTarget(int width, int height)
	{
		if (m_fbo && width == m_width && height == m_height)
			return;
		if (!m_fbo)
		{
			glGenFramebuffers(1, &m_fbo);
			glGenFramebuffers(1, &m_hdrFbo);
			glGenTextures(1, &m_color);
			glGenTextures(1, &m_hdrColor);
			glGenRenderbuffers(1, &m_depth);
		}
		m_width = width;
		m_height = height;
		glBindTexture(GL_TEXTURE_2D, m_color);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, m_hdrColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
		glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrColor, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glm::mat4 SceneView3D::ModelMatrix(const eng::ObjectDesc &o, bool withScale)
	{
		glm::mat4 m = glm::translate(glm::mat4(1.0f), o.position) * glm::mat4_cast(o.rotation);
		return withScale ? glm::scale(m, o.scale) : m;
	}

	glm::vec3 SceneView3D::Forward() const
	{
		const float yaw = glm::radians(m_yaw), pitch = glm::radians(m_pitch);
		return glm::normalize(glm::vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)));
	}

	glm::mat4 SceneView3D::ViewMatrix() const
	{
		return glm::lookAt(m_position, m_position + Forward(), glm::vec3(0, 1, 0));
	}

	void SceneView3D::FocusOn(const eng::ObjectDesc &o)
	{
		const float radius = std::max(2.0f, glm::length(o.scale) * 2.0f);
		m_position = o.position - Forward() * radius * 2.5f;
	}

	gfx::Texture *SceneView3D::AlbedoFor(const std::string &path)
	{
		if (path.empty())
			return nullptr;
		auto it = m_albedoCache.find(path);
		if (it == m_albedoCache.end())
		{
			std::shared_ptr<gfx::Texture> texture;
			try
			{
				texture = gfx::Texture::FromFile(eng::ResolveAssetPath(path), true);
			}
			catch (const std::exception &e)
			{
				spdlog::warn("Editor texture '{}' failed: {}", path, e.what());
			}
			it = m_albedoCache.emplace(path, std::move(texture)).first;
		}
		return it->second.get();
	}

	void SceneView3D::UpdateEnvironment(const eng::EnvironmentDesc &env)
	{
		const std::string path = env.skyHdr.empty() ? std::string() : (std::filesystem::path(ASSETS_DIR) / env.skyHdr).string();
		if (!m_environment || path != m_environmentPath)
		{
			m_environment = std::make_unique<gfx::EnvironmentMap>(path);
			m_environmentPath = path;
		}
		eng::DayNightCycle cycle;
		cycle.timeOfDay = env.timeOfDay;
		const int type = std::clamp(env.weather, 0, static_cast<int>(eng::WeatherType::Count) - 1);
		const eng::WeatherProfile profile = eng::ProfileFor(static_cast<eng::WeatherType>(type));
		m_environment->SetSunDirection(cycle.SunDirection());
		m_environment->SetWeather(profile.cloud, profile.dark, 0.0f, 0.0f);
	}

	void SceneView3D::Render(const eng::SceneData &scene, int selected, const glm::mat4 &view, const glm::mat4 &proj, const game::Terrain &terrain)
	{
		const glm::mat4 viewProj = proj * view;
		eng::EnvironmentDesc env = scene.environment;
		if (m_previewTime >= 0.0f)
			env.timeOfDay = m_previewTime;
		if (m_lit)
			UpdateEnvironment(env);

		// Lights: the sun and moon when the day/night cycle is on, otherwise the scene's directional lights.
		constexpr int kMaxLights = 64;
		struct Gathered
		{
			glm::vec4 posRange, colorType, dirInner;
			float outer;
			float sortKey;
		};
		std::vector<Gathered> lights;
		const glm::vec3 cameraPos = m_position;
		eng::DayNightCycle cycle;
		cycle.timeOfDay = env.timeOfDay;
		const eng::WeatherProfile weather = eng::ProfileFor(static_cast<eng::WeatherType>(std::clamp(env.weather, 0, static_cast<int>(eng::WeatherType::Count) - 1)));
		const float daylight = cycle.Daylight();
		if (m_lit && env.dayNightCycle)
		{
			const glm::vec3 sunDir = cycle.SunDirection();
			const float elevation = sunDir.y;
			const float sunAmount = glm::smoothstep(-0.02f, 0.15f, elevation) * weather.light;
			const glm::vec3 sunColor = glm::mix(glm::vec3(1.0f, 0.55f, 0.3f), glm::vec3(1.0f, 0.96f, 0.85f), glm::smoothstep(0.0f, 0.4f, elevation));
			lights.push_back({glm::vec4(0.0f), glm::vec4(sunColor * 3.0f * sunAmount, 2.0f), glm::vec4(-sunDir, 0.0f), 0.0f, -1.0f});
			const float moonAmount = glm::smoothstep(-0.02f, 0.15f, -elevation) * glm::mix(1.0f, weather.light, 0.7f);
			lights.push_back({glm::vec4(0.0f), glm::vec4(glm::vec3(0.35f, 0.42f, 0.65f) * moonAmount, 2.0f), glm::vec4(sunDir, 0.0f), 0.0f, -1.0f});
		}
		if (m_lit)
		{
			for (const eng::ObjectDesc &o : scene.objects)
			{
				const auto &p = o.scriptParams;
				const auto at = [&](size_t i, float fallback) { return i < p.size() ? p[i] : fallback; };
				Gathered g{};
				g.posRange = glm::vec4(o.position, 0.0f);
				g.dirInner = glm::vec4(o.rotation * glm::vec3(0, 0, -1), 1.0f);
				if (o.script == game::kScriptLight)
				{
					const int type = std::clamp(static_cast<int>(at(0, 0.0f)), 0, 2);
					if (type == 2 && env.dayNightCycle)
						continue;
					g.posRange.w = at(5, 20.0f);
					g.colorType = glm::vec4(glm::vec3(at(1, 1.0f), at(2, 1.0f), at(3, 1.0f)) * at(4, 10.0f), static_cast<float>(type));
					g.dirInner.w = std::cos(glm::radians(at(6, 12.0f)));
					g.outer = std::cos(glm::radians(at(7, 22.0f)));
				}
				else if (o.script == game::kScriptLightPost)
				{
					g.posRange.w = at(1, 28.0f);
					g.colorType = glm::vec4(glm::vec3(at(4, 1.0f), at(5, 0.7f), at(6, 0.4f)) * at(0, 120.0f), 0.0f);
				}
				else
					continue;
				g.sortKey = glm::distance(o.position, cameraPos);
				lights.push_back(g);
			}
			std::stable_sort(lights.begin(), lights.end(), [](const Gathered &a, const Gathered &b) { return a.sortKey < b.sortKey; });
			if (lights.size() > kMaxLights)
				lights.resize(kMaxLights);
		}

		glBindVertexArray(m_emptyVao);
		glBindFramebuffer(GL_FRAMEBUFFER, m_lit ? m_hdrFbo : m_fbo);
		glViewport(0, 0, m_width, m_height);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glDepthMask(GL_TRUE);
		glClearColor(0.16f, 0.16f, 0.16f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		if (m_lit && m_showSky)
			m_environment->DrawSky(view, proj, cameraPos);

		m_shader.Bind();
		m_shader.SetMat4("uViewProj", viewProj);
		m_shader.SetVec3("uCameraPos", cameraPos);
		m_shader.SetInt("uLit", m_lit ? 1 : 0);
		if (m_lit)
		{
			m_environment->BindForLighting();
			std::vector<glm::vec4> posRange, colorType, dirInner;
			std::vector<float> outer;
			for (const Gathered &g : lights)
			{
				posRange.push_back(g.posRange);
				colorType.push_back(g.colorType);
				dirInner.push_back(g.dirInner);
				outer.push_back(g.outer);
			}
			m_shader.SetInt("uLightCount", static_cast<int>(lights.size()));
			if (!lights.empty())
			{
				m_shader.SetVec4Array("uLightPosRange", posRange.data(), static_cast<int>(lights.size()));
				m_shader.SetVec4Array("uLightColorType", colorType.data(), static_cast<int>(lights.size()));
				m_shader.SetVec4Array("uLightDirInner", dirInner.data(), static_cast<int>(lights.size()));
				glProgramUniform1fv(m_shader.Id(), glGetUniformLocation(m_shader.Id(), "uLightOuter"), static_cast<GLsizei>(lights.size()), outer.data());
			}
		}

		// Pass 0 draws the shaded surfaces (into the HDR target when lit); pass 1 draws unshaded overlays after tonemapping.
		int pass = 0;
		const auto draw = [&](const GpuMesh &mesh, const glm::mat4 &model, const glm::vec3 &color, const glm::vec3 &emissive, bool flat)
		{
			if ((pass == 0) == flat)
				return;
			m_shader.SetMat4("uModel", model);
			m_shader.SetVec3("uColor", color);
			m_shader.SetVec3("uEmissive", emissive);
			m_shader.SetInt("uFlat", flat ? 1 : 0);
			glBindVertexArray(mesh.vao);
			glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
		};
		const auto drawBox = [&](const glm::mat4 &model, const glm::vec3 &lo, const glm::vec3 &hi, const glm::vec3 &color)
		{
			const glm::mat4 m = model * glm::translate(glm::mat4(1.0f), (lo + hi) * 0.5f) * glm::scale(glm::mat4(1.0f), hi - lo);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			draw(m_cube, m, color, glm::vec3(0.0f), true);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		};

		const float cullDistance = 500.0f;
		const auto bindAlbedo = [&](const eng::MaterialDesc *material)
		{
			gfx::Texture *texture = (m_textures && material) ? AlbedoFor(material->albedoMap) : nullptr;
			if (texture)
				glBindTextureUnit(0, texture->Id());
			m_shader.SetInt("uHasTexture", texture ? 1 : 0);
		};
		m_shader.SetFloat("uSkyLight", m_skyLight ? m_skyLightIntensity : 0.0f);
		m_shader.SetFloat("uFogDensity", (m_lit && m_fog) ? m_fogDensity : 0.0f);
		for (pass = 0; pass < 2; ++pass)
		{
			if (pass == 1)
			{
				if (m_lit)
				{
					glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
					glDisable(GL_DEPTH_TEST);
					glDepthMask(GL_FALSE);
					m_tonemap.Bind();
					m_tonemap.SetFloat("uExposure", m_exposure * glm::mix(1.6f, 0.85f, daylight));
					m_tonemap.SetInt("uTonemap", 1);
					glBindTextureUnit(0, m_hdrColor);
					glBindVertexArray(m_emptyVao);
					glDrawArrays(GL_TRIANGLES, 0, 3);
					glEnable(GL_DEPTH_TEST);
					glDepthMask(GL_TRUE);
					m_shader.Bind();
				}
			}
		for (int i = 0; i < static_cast<int>(scene.objects.size()); ++i)
		{
			const eng::ObjectDesc &o = scene.objects[i];
			const bool isSelected = i == selected;
			if (!isSelected && glm::distance(o.position, cameraPos) > cullDistance && !IsBackdrop(scene, o))
				continue;

			const bool hasMesh = o.mesh >= 0 && o.mesh < static_cast<int>(scene.meshes.size());
			if (hasMesh)
			{
				const GpuMesh &mesh = MeshFor(scene, o.mesh, terrain);
				if (mesh.valid)
				{
					glm::vec3 color(0.65f), emissive(0.0f);
					const eng::MaterialDesc *material = nullptr;
					if (o.material >= 0 && o.material < static_cast<int>(scene.materials.size()))
					{
						material = &scene.materials[o.material];
						color = material->color;
						emissive = material->emissive;
					}
					if (pass == 0 && !mesh.parts.empty())
					{
						// Imported model: each part keeps its own colour and texture unless the object has a material override.
						for (const GpuMesh &part : mesh.parts)
						{
							gfx::Texture *texture = nullptr;
							if (m_textures)
								texture = (material && !material->albedoMap.empty()) ? AlbedoFor(material->albedoMap) : part.albedo.get();
							if (texture)
								glBindTextureUnit(0, texture->Id());
							m_shader.SetInt("uHasTexture", texture ? 1 : 0);
							draw(part, ModelMatrix(o, true), material ? color : part.tint, material ? emissive : part.emissive, false);
						}
					}
					else if (pass == 0)
					{
						bindAlbedo(material);
						draw(mesh, ModelMatrix(o, true), color, emissive, false);
					}
					if (isSelected)
						drawBox(ModelMatrix(o, true), mesh.bounds.min, mesh.bounds.max, {1.0f, 0.78f, 0.15f});
					if (o.hasBody && m_showColliders)
						drawBox(ModelMatrix(o, false), -o.halfExtents, o.halfExtents, {0.2f, 0.8f, 0.9f});
					continue;
				}
			}
			if (o.hasBody)
			{
				if (m_showColliders || isSelected)
					drawBox(ModelMatrix(o, false), -o.halfExtents, o.halfExtents, isSelected ? glm::vec3(1.0f, 0.78f, 0.15f) : glm::vec3(0.2f, 0.8f, 0.9f));
				continue;
			}
			// Mesh-less, body-less objects (lights, zones, the player) show as a small marker.
			glm::vec3 markerColor = isSelected ? glm::vec3(1.0f, 0.78f, 0.15f) : glm::vec3(0.95f, 0.9f, 0.3f);
			if (o.camera.enabled && m_showLights)
			{
				// Frustum outline: a wireframe box 4 units deep sized by the field of view, plus a lens marker.
				const float fov = glm::radians(std::clamp(o.camera.fov, 10.0f, 120.0f));
				const float depth = 4.0f, halfH = std::tan(fov * 0.5f) * depth, halfW = halfH * (16.0f / 9.0f);
				const glm::mat4 base = glm::translate(glm::mat4(1.0f), o.position) * glm::mat4_cast(o.rotation) * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -depth * 0.5f));
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				draw(m_cube, base * glm::scale(glm::mat4(1.0f), glm::vec3(halfW * 2.0f, halfH * 2.0f, depth)), markerColor, glm::vec3(0.0f), true);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			if (o.script == game::kScriptLight && m_showLights)
			{
				const auto &p = o.scriptParams;
				const auto at = [&](size_t i, float fallback) { return i < p.size() ? p[i] : fallback; };
				const glm::vec3 lightColor(at(1, 1.0f), at(2, 1.0f), at(3, 1.0f));
				const int type = static_cast<int>(at(0, 0.0f));
				const float range = at(5, 20.0f);
				if (!isSelected)
					markerColor = glm::clamp(lightColor, 0.0f, 1.0f);
				const glm::vec3 dir = o.rotation * glm::vec3(0, 0, -1);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				if (type == 0 && isSelected)
					draw(m_sphere, glm::translate(glm::mat4(1.0f), o.position) * glm::scale(glm::mat4(1.0f), glm::vec3(std::max(range, 0.1f))), lightColor, glm::vec3(0.0f), true);
				else if (type == 1)
				{
					// Cone wireframe: a cube stretched along the light direction approximates the volume.
					const float outer = glm::radians(std::clamp(at(7, 30.0f), 1.0f, 89.0f));
					const float radius = std::tan(outer) * range;
					const glm::mat4 base = glm::translate(glm::mat4(1.0f), o.position) * glm::mat4_cast(o.rotation) * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -range * 0.5f));
					draw(m_cube, base * glm::scale(glm::mat4(1.0f), glm::vec3(radius * 2.0f, radius * 2.0f, range)), lightColor, glm::vec3(0.0f), true);
				}
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				if (type != 0)
				{
					const glm::vec3 center = o.position + dir * 1.5f;
					draw(m_cube, glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), glm::vec3(0.25f)), lightColor, glm::vec3(0.0f), true);
				}
			}
			draw(m_cube, ModelMatrix(o, false) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)), markerColor, glm::vec3(0.0f), true);
		}
		}

		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void SceneView3D::ReloadTextures()
	{
		m_albedoCache.clear();
	}

	// Toolbar drawn at the top of the 3D View window: gizmo tools, snapping, camera speed and display options.
	void SceneView3D::DrawControls()
	{
		ImGui::PushID("viewbar");
		ImGui::RadioButton("Move", &m_op, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Rotate", &m_op, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Scale", &m_op, 2);
		ImGui::SameLine();
		ImGui::BeginDisabled(m_op == 2);
		ImGui::Checkbox("Local", &m_local);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Checkbox("Snap", &m_snap);
		ImGui::SameLine();
		float &snapValue = m_op == 0 ? m_snapMove : (m_op == 1 ? m_snapRotate : m_snapScale);
		ImGui::SetNextItemWidth(70.0f);
		ImGui::DragFloat("##snapstep", &snapValue, 0.05f, 0.01f, 360.0f, m_op == 1 ? "%.0f deg" : "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		ImGui::SliderFloat("Speed", &m_speed, 2.0f, 200.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
		ImGui::SameLine();
		if (ImGui::Button("Display"))
			ImGui::OpenPopup("displaypopup");
		if (ImGui::BeginPopup("displaypopup"))
		{
			DrawDisplayOptions();
			ImGui::EndPopup();
		}
		ImGui::PopID();
		ImGui::Separator();
	}

	void SceneView3D::DrawDisplayOptions()
	{
		ImGui::Checkbox("Colliders", &m_showColliders);
		ImGui::Checkbox("Light markers", &m_showLights);
		ImGui::Checkbox("Lit", &m_lit);
		ImGui::Checkbox("Sky", &m_showSky);
		ImGui::Checkbox("Textures", &m_textures);
		ImGui::Checkbox("Skylight", &m_skyLight);
		if (m_skyLight)
		{
			ImGui::Indent();
			ImGui::DragFloat("Intensity##skylight", &m_skyLightIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
			ImGui::Unindent();
		}
		ImGui::Checkbox("Fog", &m_fog);
		if (m_fog)
		{
			ImGui::Indent();
			ImGui::DragFloat("Density##fog", &m_fogDensity, 0.0002f, 0.0001f, 0.1f, "%.4f");
			ImGui::Unindent();
		}
		ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 4.0f, "%.2f");
	}

	void SceneView3D::SetViewFrom(const glm::vec3 &position, const glm::quat &rotation, float fovDegrees)
	{
		const glm::vec3 f = glm::normalize(rotation * glm::vec3(0, 0, -1));
		m_position = position;
		m_yaw = glm::degrees(std::atan2(f.z, f.x));
		m_pitch = std::clamp(glm::degrees(std::asin(std::clamp(f.y, -1.0f, 1.0f))), -89.0f, 89.0f);
		m_fov = std::clamp(fovDegrees, 10.0f, 120.0f);
	}

	void SceneView3D::GetView(glm::vec3 &position, glm::quat &rotation) const
	{
		position = m_position;
		rotation = glm::quatLookAt(Forward(), glm::vec3(0, 1, 0));
	}

	void SceneView3D::AttachSession(PlaySession *session, core::Window *window)
	{
		ReleasePlayInput();
		m_session = session;
		m_window = window;
		m_sessionPaused = false;
	}

	void SceneView3D::ReleasePlayInput()
	{
		if (m_playCaptured && m_window)
		{
			m_window->SetCursorCaptured(false);
			m_window->GetInput().SetReceiver(nullptr);
		}
		m_playCaptured = false;
	}

	bool SceneView3D::Draw(eng::SceneData &scene, int &selected, bool &dirty, const game::Terrain &terrain)
	{
		bool pickedNew = false;
		if (!ImGui::Begin("3D View", &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::End();
			return false;
		}
		float &snapValue = m_op == 0 ? m_snapMove : (m_op == 1 ? m_snapRotate : m_snapScale);

		DrawControls();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 avail = ImGui::GetContentRegionAvail();
		const int width = std::max(16, static_cast<int>(avail.x));
		const int height = std::max(16, static_cast<int>(avail.y));
		const ImVec2 size(static_cast<float>(width), static_cast<float>(height));
		// Dummy instead of a button: ImGuizmo refuses to start while any ImGui item is hovered or active.
		ImGui::Dummy(size);
		ImGuiIO &io = ImGui::GetIO();
		const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + size.x, origin.y + size.y));
		const bool anyButton = ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right);
		if (!anyButton)
			m_captured = false;
		else if (hovered && !m_captured && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
			m_captured = true;
		const bool playing = m_session && m_session->GetMode() == PlaySession::Mode::Play;
		const bool active = m_captured && !playing;

		const glm::vec3 forward = Forward();
		const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
		const glm::vec3 up = glm::cross(right, forward);

		if (active && ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			m_yaw += io.MouseDelta.x * 0.2f;
			m_pitch = std::clamp(m_pitch - io.MouseDelta.y * 0.2f, -89.0f, 89.0f);
			const float step = m_speed * (io.KeyShift ? 4.0f : 1.0f) * io.DeltaTime;
			if (ImGui::IsKeyDown(ImGuiKey_W))
				m_position += forward * step;
			if (ImGui::IsKeyDown(ImGuiKey_S))
				m_position -= forward * step;
			if (ImGui::IsKeyDown(ImGuiKey_D))
				m_position += right * step;
			if (ImGui::IsKeyDown(ImGuiKey_A))
				m_position -= right * step;
			if (ImGui::IsKeyDown(ImGuiKey_E))
				m_position += glm::vec3(0, 1, 0) * step;
			if (ImGui::IsKeyDown(ImGuiKey_Q))
				m_position -= glm::vec3(0, 1, 0) * step;
		}
		if (active && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
			m_position += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * (m_speed * 0.01f);
		if (hovered && io.MouseWheel != 0.0f)
			m_position += forward * (io.MouseWheel * m_speed * 0.4f);
		if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false) && selected >= 0 && selected < static_cast<int>(scene.objects.size()))
			FocusOn(scene.objects[selected]);

		const glm::mat4 view = ViewMatrix();
		const glm::mat4 proj = glm::perspective(glm::radians(m_fov), size.x / size.y, 0.1f, 3000.0f);
		const glm::mat4 viewProj = proj * view;
		const glm::mat4 inverseVP = glm::inverse(viewProj);

		if (m_session)
		{
			if (playing)
			{
				if (!m_playCaptured && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_sessionPaused && m_window)
				{
					m_window->GetInput().ResetMouse();
					m_window->GetInput().SetReceiver(m_session->Receiver());
					m_window->SetCursorCaptured(true);
					m_playCaptured = true;
				}
				else if (m_playCaptured && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || m_sessionPaused))
					ReleasePlayInput();
			}

			gfx::Camera flyCamera;
			flyCamera.position = m_position;
			flyCamera.forward = forward;
			flyCamera.fovDegrees = m_fov;
			flyCamera.farPlane = 1500.0f;
			if (!m_sessionPaused)
				m_session->Update(io.DeltaTime, flyCamera);
			const GLuint tex = m_session->Render(width, height, flyCamera, static_cast<float>(ImGui::GetTime()));
			ImDrawList *sessionList = ImGui::GetWindowDrawList();
			sessionList->AddImage(static_cast<ImTextureID>(tex), origin, ImVec2(origin.x + size.x, origin.y + size.y), ImVec2(0, 1), ImVec2(1, 0));

			const char *hint = !playing ? "Simulating physics | RMB+WASDQE fly | wheel dolly"
				: m_session->Status() == game::Status::Won	? "You escaped!"
				: m_session->Status() == game::Status::Lost ? "You died"
				: m_playCaptured							? "Esc releases the mouse"
															: "Click to capture the mouse";
			sessionList->AddText(ImVec2(origin.x + 8, origin.y + 6), IM_COL32(255, 255, 255, 220), hint);
			ImGui::End();
			return false;
		}

		const auto mouseRay = [&](glm::vec3 &rayOrigin, glm::vec3 &rayDir)
		{
			const float nx = (io.MousePos.x - origin.x) / size.x * 2.0f - 1.0f;
			const float ny = 1.0f - (io.MousePos.y - origin.y) / size.y * 2.0f;
			glm::vec4 a = inverseVP * glm::vec4(nx, ny, -1.0f, 1.0f);
			glm::vec4 b = inverseVP * glm::vec4(nx, ny, 1.0f, 1.0f);
			a /= a.w;
			b /= b.w;
			rayOrigin = glm::vec3(a);
			rayDir = glm::normalize(glm::vec3(b - a));
		};

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
		{
			glm::vec3 ro, rd;
			mouseRay(ro, rd);
			int hit = -1;
			float best = std::numeric_limits<float>::max();
			for (int i = 0; i < static_cast<int>(scene.objects.size()); ++i)
			{
				const eng::ObjectDesc &o = scene.objects[i];
				if (IsBackdrop(scene, o))
					continue;

				glm::vec3 lo(-0.25f), hi(0.25f);
				glm::mat4 model = ModelMatrix(o, false);
				if (o.mesh >= 0 && o.mesh < static_cast<int>(scene.meshes.size()))
				{
					const GpuMesh &mesh = MeshFor(scene, o.mesh, terrain);
					if (mesh.valid)
					{
						lo = mesh.bounds.min;
						hi = mesh.bounds.max;
						model = ModelMatrix(o, true);
					}
				}
				else if (o.hasBody)
				{
					lo = -o.halfExtents;
					hi = o.halfExtents;
				}

				const glm::mat4 inv = glm::inverse(model);
				const glm::vec3 lro = glm::vec3(inv * glm::vec4(ro, 1.0f));
				const glm::vec3 lrd = glm::vec3(inv * glm::vec4(rd, 0.0f));
				const float t = RayBox(lro, lrd, lo, hi);
				if (t >= 0.0f && t < best)
				{
					best = t;
					hit = i;
				}
			}
			if (hit >= 0)
			{
				pickedNew = hit != selected;
				selected = hit;
			}
		}

		if (hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.KeyCtrl)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W, false))
				m_op = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_E, false))
				m_op = 1;
			if (ImGui::IsKeyPressed(ImGuiKey_R, false))
				m_op = 2;
		}

		EnsureTarget(width, height);
		Render(scene, selected, view, proj, terrain);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddImage(static_cast<ImTextureID>(m_color), origin, ImVec2(origin.x + size.x, origin.y + size.y), ImVec2(0, 1), ImVec2(1, 0));
		dl->AddText(ImVec2(origin.x + 8, origin.y + 6), IM_COL32(255, 255, 255, 200), "RMB+WASDQE fly | MMB pan | wheel dolly | LMB select | W/E/R gizmo | F focus");

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(dl);
		ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
		ImGuizmo::PushID(0);

		if (selected >= 0 && selected < static_cast<int>(scene.objects.size()))
		{
			eng::ObjectDesc &o = scene.objects[selected];
			glm::mat4 model = ModelMatrix(o, true);

			static const ImGuizmo::OPERATION ops[] = {ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE};
			const ImGuizmo::MODE mode = (m_local || m_op == 2) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
			const bool snap = m_snap != io.KeyCtrl;
			const float snapValues[3] = {snapValue, snapValue, snapValue};

			glm::mat4 viewCopy = view;
			if (ImGuizmo::Manipulate(glm::value_ptr(viewCopy), glm::value_ptr(proj), ops[m_op], mode, glm::value_ptr(model), nullptr, snap ? snapValues : nullptr))
			{
				float t[3], r[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, r, s);
				o.position = {t[0], t[1], t[2]};
				if (m_op == 1)
				{
					glm::mat3 rot(model);
					for (int c = 0; c < 3; ++c)
						rot[c] = glm::normalize(rot[c]);
					o.rotation = glm::normalize(glm::quat_cast(rot));
				}
				if (m_op == 2)
					o.scale = glm::max({s[0], s[1], s[2]}, glm::vec3(0.001f));
				dirty = true;
			}
		}

		// View cube: dragging it reorients the camera around a point ahead of it.
		{
			constexpr float kOrbitDistance = 10.0f;
			glm::mat4 v = view;
			const float cubeSize = 110.0f;
			// The overload with projection recomputes ImGuizmo's context; the plain one reuses a stale view when nothing is selected.
			const glm::mat4 identity(1.0f);
			ImGuizmo::ViewManipulate(glm::value_ptr(v), glm::value_ptr(proj), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, const_cast<float *>(glm::value_ptr(identity)), kOrbitDistance, ImVec2(origin.x + size.x - cubeSize - 4.0f, origin.y + 4.0f), ImVec2(cubeSize, cubeSize), 0x30101010);
			if (v != view)
			{
				const glm::mat4 inv = glm::inverse(v);
				m_position = glm::vec3(inv[3]);
				const glm::vec3 f = glm::normalize(-glm::vec3(inv[2]));
				m_yaw = glm::degrees(std::atan2(f.z, f.x));
				m_pitch = std::clamp(glm::degrees(std::asin(std::clamp(f.y, -1.0f, 1.0f))), -89.0f, 89.0f);
			}
		}

		ImGuizmo::PopID();
		ImGui::End();
		return pickedNew;
	}

} // namespace editor
