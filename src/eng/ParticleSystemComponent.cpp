#include "eng/ParticleSystemComponent.h"
#include "eng/GameObject.h"
#include "eng/World.h"

namespace eng
{

	void ParticleSystemComponent::OnAttach()
	{
		gameObject->GetWorld().RegisterParticles(this);
	}

	void ParticleSystemComponent::OnDestroy()
	{
		gameObject->GetWorld().UnregisterParticles(this);
	}

	ParticleSystem &ParticleSystemComponent::AddSystem()
	{
		m_systems.push_back(std::make_unique<ParticleSystem>(1234u + static_cast<uint32_t>(m_systems.size())));
		return *m_systems.back();
	}

	void ParticleSystemComponent::OnUpdate(float dt)
	{
		if (!enabled)
			return;

		World &world = gameObject->GetWorld();
		const glm::vec3 cam = world.GetCamera().position;
		const glm::vec3 origin = followCamera ? cam : gameObject->GetTransform().position;
		const PhysicsWorld &physics = world.Physics();
		const ParticleSystem::GroundQuery ground = [&physics](float x, float z) { return physics.GroundHeight(x, z, -1e9f); };

		for (auto &s : m_systems)
		{
			s->position = origin;
			s->focusPoint = cam;
		}
		if (!m_prewarmed)
		{
			m_prewarmed = true;
			if (prewarmSeconds > 0.0f)
				for (auto &s : m_systems)
					s->Prewarm(prewarmSeconds, ground);
		}
		for (auto &s : m_systems)
			s->Update(dt, ground);
	}

	void ParticleSystemComponent::Collect(gfx::ParticleRenderer &renderer)
	{
		if (!enabled)
			return;
		for (auto &s : m_systems)
			s->Collect(renderer);
	}

	void ConfigureRain(ParticleSystemComponent &c, float amount, const glm::vec3 &wind)
	{
		c.followCamera = true;
		c.prewarmSeconds = 2.0f;

		ParticleSystem &splash = c.AddSystem();
		splash.main.startLifetime = 0.35f;
		splash.main.startSize = 0.06f;
		splash.main.startColor = {0.7f, 0.78f, 0.9f, 0.4f};
		splash.main.maxParticles = 500;
		splash.emission.enabled = false;
		splash.sizeOverLifetime = {true, 1.0f, 6.0f};
		splash.colorOverLifetime.enabled = true;
		splash.colorOverLifetime.keys = {{0.0f, glm::vec4(1.0f)}, {1.0f, {1.0f, 1.0f, 1.0f, 0.0f}}};
		splash.renderer.mode = gfx::ParticleMode::Ring;

		ParticleSystem &drops = c.AddSystem();
		drops.main.startLifetime = 2.0f;
		drops.main.startSpeed = {16.0f, 24.0f};
		drops.main.startSize = 0.001f;
		drops.main.startColor = {0.55f, 0.62f, 0.75f, 0.22f};
		drops.main.maxParticles = 6000;
		drops.emission.rateOverTime = 5000.0f;
		drops.emission.rateMultiplier = amount;
		drops.shape.type = ParticleSystem::ShapeModule::Type::Box;
		drops.shape.offset = {0.0f, 19.0f, 0.0f};
		drops.shape.boxSize = {52.0f, 14.0f, 52.0f};
		drops.shape.direction = {0.0f, -1.0f, 0.0f};
		drops.velocityOverLifetime = {true, wind};
		drops.collision.enabled = true;
		drops.collision.minHeight = -0.2f; // water level
		drops.collision.subEmitter = &splash;
		drops.collision.subEmitterChance = 0.35f;
		drops.collision.subEmitterRange = 22.0f;
		drops.renderer.mode = gfx::ParticleMode::Streak;
	}

	void ConfigureBlood(ParticleSystemComponent &c)
	{
		ParticleSystem &blood = c.AddSystem();
		blood.main.startLifetime = {1.2f, 2.5f};
		blood.main.startSpeed = {1.5f, 5.0f};
		blood.main.startSize = {0.04f, 0.1f};
		blood.main.startColor = {0.32f, 0.01f, 0.01f, 0.9f};
		blood.main.gravityModifier = 1.0f;
		blood.main.maxParticles = 1500;
		blood.emission.enabled = false;
		blood.shape.randomVelocityMin = {-1.6f, 0.3f, -1.6f};
		blood.shape.randomVelocityMax = {1.6f, 3.5f, 1.6f};
		blood.collision.enabled = true;
		blood.collision.response = ParticleSystem::CollisionModule::Response::Stick;
		blood.collision.stickSizeMultiplier = 2.2f;
		blood.collision.stickLifetime = 4.0f;
		blood.colorOverLifetime.enabled = true;
		blood.colorOverLifetime.keys = {{0.0f, glm::vec4(1.0f)}, {0.625f, glm::vec4(1.0f)}, {1.0f, {1.0f, 1.0f, 1.0f, 0.0f}}};
	}

} // namespace eng
