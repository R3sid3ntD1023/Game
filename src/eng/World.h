#pragma once
#include "audio/AudioEngine.h"
#include "eng/GameObject.h"
#include "eng/IWorldQuery.h"
#include "eng/NavigationGrid.h"
#include "eng/ParticleEmitter.h"
#include "eng/Physics.h"
#include "gfx/Camera.h"
#include "gfx/EnvironmentMap.h"
#include "gfx/ForwardPlus.h"
#include "gfx/LineRenderer.h"
#include "gfx/MeshManager.h"
#include "gfx/ParticleRenderer.h"
#include "gfx/RenderSystem.h"
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

		void RegisterEmitter(ParticleEmitter *e) { m_emitters.push_back(e); }
		void UnregisterEmitter(ParticleEmitter *e);

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

		gfx::LineRenderer &Lines() { return m_lineRenderer; }

	private:
		IWorldQuery *m_worldQuery = nullptr;
		static constexpr float kFixedStep = 1.0f / 60.0f;

		entt::registry m_registry;
		gfx::MeshManager m_meshes;
		gfx::RenderSystem m_renderer;
		gfx::ForwardPlus m_forwardPlus;
		gfx::ParticleRenderer m_particleRenderer;
		gfx::LineRenderer m_lineRenderer;
		PhysicsWorld m_physics;
		std::vector<std::unique_ptr<GameObject>> m_objects;
		std::vector<ParticleEmitter *> m_emitters;
		ParticleEmitter *m_bloodEmitter = nullptr;
		float m_accumulator = 0.0f;
		float m_deltaTime = 0.0f;
		eng::NavigationGrid m_navigationGrid;
	};

} // namespace eng
