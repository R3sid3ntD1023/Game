#pragma once
#include "audio/AudioEngine.h"
#include "eng/GameObject.h"
#include "eng/DayNight.h"
#include "eng/IWorldQuery.h"
#include "eng/NavigationGrid.h"
#include "eng/ParticleSystemComponent.h"
#include "eng/Physics.h"
#include "gfx/Camera.h"
#include "gfx/EnvironmentMap.h"
#include "gfx/ForwardPlus.h"
#include "gfx/LineRenderer.h"
#include "gfx/QuadRenderer.h"
#include "gfx/MeshManager.h"
#include "gfx/ParticleRenderer.h"
#include "gfx/RenderSystem.h"
#include "gfx/ShadowMap.h"
#include "gfx/LocalShadowMap.h"
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

namespace eng
{

	// Owns game objects, the ECS registry, physics and the Forward+ renderer.
	class World
	{
	public:
		World();
		~World();
		World(const World &) = delete;
		World &operator=(const World &) = delete;

		GameObject &CreateGameObject(const std::string &name, const glm::vec3 &position = glm::vec3(0.0f));

		// Variable-rate component updates, then fixed-rate component updates + physics, then deferred destruction.
		void Update(float dt, const gfx::Camera &camera);
		// Depth prepass, Forward+ light culling, shading, sky and particles into the currently bound framebuffer.
		// `depthTexture` is that framebuffer's depth attachment (read by the culling pass).
		void Render(float aspect, GLuint depthTexture, int width, int height, const gfx::Camera &camera);

		void SetWorldQuery(IWorldQuery *query);

		// Sprays blood from a shared one-shot emitter object (created on first use).
		void EmitBlood(const glm::vec3 &position, const glm::vec3 &direction, int count);

		void RegisterParticles(ParticleSystemComponent *e) { m_particleComponents.push_back(e); }
		void UnregisterParticles(ParticleSystemComponent *e);

		entt::registry &Registry() { return m_registry; }
		PhysicsWorld &Physics() { return m_physics; }
		gfx::MeshManager &Meshes() { return m_meshes; }
		size_t DrawCalls() const { return m_renderer.LastDrawCalls(); }
		size_t ObjectCount() const { return m_objects.size(); }
		const std::vector<std::unique_ptr<GameObject>> &Objects() const { return m_objects; }
		GameObject *Find(const std::string &name) const; // first live object with this name

		std::shared_ptr<gfx::EnvironmentMap> environment; // optional; enables IBL and the sky
		audio::AudioEngine *audio = nullptr;			  // optional; the listener follows `camera`

		gfx::Camera &GetCamera();

		IWorldQuery *Query() { return m_worldQuery; }

		NavigationGrid &NavigationGrid() { return m_navigationGrid; }

		float GetTerrainHeight(float x, float z) const;

		bool IsWalkable(float x, float z) const;

		bool FindRandomReachablePoint(glm::vec3 &out, const glm::vec3 &center, float radius) const;

		void AddNavigationObstacle(const glm::vec3 &position, const glm::vec3 &halfExtents);

		gfx::LineRenderer &Lines() { return m_lineRenderer; }
		gfx::QuadRenderer &Quads() { return m_quadRenderer; }

		// Debug visualisation of the navigation data (cached GPU batches, rebuilt when obstacles change).
		void SetNavGridVisible(bool visible);

		// 0 off, 1 walkable nav mesh (green, dark outline), 2 slope heat map, 3 grid lines.
		void SetNavDebugMode(int mode);
		int NavDebugMode() const { return m_navDebugMode; }

		// Day/night clock. While enabled, the sun and moon replace any directional lights in the scene.
		DayNightCycle &DayNight() { return m_dayNight; }
		const DayNightCycle &DayNight() const { return m_dayNight; }
		void SetTimeOfDay(float hours) { m_dayNight.SetTime(hours); }
		void SetNavMeshVisible(bool visible);
		// Foliage wind: direction on the XZ plane, length is the strength (0..1).
		void SetWind(const glm::vec2 &direction) { m_windDirection = direction; }
		bool IsNavGridVisible() const { return m_navGridVisible; }
		bool IsNavMeshVisible() const { return m_navMeshVisible; }

	private:
		void RebuildNavDebug();
		void UpdateNavDebugVisibility(const glm::vec3 &cameraPos);

		// The navigation overlay is split into square chunks so only the ones near the camera are drawn.
		struct NavChunk
		{
			uint32_t grid = gfx::LineRenderer::kInvalidBatch;
			glm::vec2 center{0.0f};
		};
		static constexpr int kNavChunkCells = 32;
		static constexpr float kNavDrawRadius = 90.0f;
		std::vector<NavChunk> m_navChunks;
		bool m_navGridVisible = false;
		bool m_navMeshVisible = false;
		bool m_navDebugDirty = false;
		int m_navDebugMode = 0;
		float m_windTime = 0.0f;
		glm::vec2 m_windDirection{0.85f, 0.35f};

		IWorldQuery *m_worldQuery = nullptr;
		static constexpr float kFixedStep = 1.0f / 60.0f;

		entt::registry m_registry;
		gfx::MeshManager m_meshes;
		gfx::RenderSystem m_renderer;
		gfx::ForwardPlus m_forwardPlus;
		gfx::ShadowMap m_shadows;
		gfx::LocalShadowMap m_localShadows;
		DayNightCycle m_dayNight;
		gfx::ParticleRenderer m_particleRenderer;
		gfx::LineRenderer m_lineRenderer;
		gfx::QuadRenderer m_quadRenderer;
		PhysicsWorld m_physics;
		std::vector<std::unique_ptr<GameObject>> m_objects;
		std::vector<ParticleSystemComponent *> m_particleComponents;
		ParticleSystemComponent *m_bloodEmitter = nullptr;
		float m_accumulator = 0.0f;
		float m_deltaTime = 0.0f;
		eng::NavigationGrid m_navigationGrid;
	};

} // namespace eng
