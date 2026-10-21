#include "eng/ParticleSystem.h"
#include <algorithm>
#include <cmath>

namespace eng
{

	void ParticleSystem::Spawn(const glm::vec3 &pos, const glm::vec3 &dir)
	{
		if (m_particles.size() >= main.maxParticles)
			return;
		Particle p;
		p.pos = pos;
		p.vel = dir * Rand(main.startSpeed) +
				glm::vec3(Rand(shape.randomVelocityMin.x, shape.randomVelocityMax.x),
						  Rand(shape.randomVelocityMin.y, shape.randomVelocityMax.y),
						  Rand(shape.randomVelocityMin.z, shape.randomVelocityMax.z));
		p.age = 0.0f;
		p.lifetime = std::max(Rand(main.startLifetime), 0.001f);
		p.size = Rand(main.startSize);
		p.stuck = false;
		m_particles.push_back(p);
	}

	void ParticleSystem::Emit(const glm::vec3 &pos, const glm::vec3 &dir, int count)
	{
		for (int i = 0; i < count; ++i)
			Spawn(pos, dir);
	}

	glm::vec3 ParticleSystem::Velocity(const Particle &p) const
	{
		return velocityOverLifetime.enabled ? p.vel + velocityOverLifetime.linear : p.vel;
	}

	glm::vec4 ParticleSystem::EvaluateColor(float t) const
	{
		const auto &k = colorOverLifetime.keys;
		if (!colorOverLifetime.enabled || k.empty())
			return glm::vec4(1.0f);
		if (t <= k.front().first)
			return k.front().second;
		for (size_t i = 1; i < k.size(); ++i)
		{
			if (t <= k[i].first)
			{
				const float span = k[i].first - k[i - 1].first;
				return glm::mix(k[i - 1].second, k[i].second, span > 0.0f ? (t - k[i - 1].first) / span : 1.0f);
			}
		}
		return k.back().second;
	}

	void ParticleSystem::Prewarm(float seconds, const GroundQuery &ground)
	{
		constexpr float kStep = 1.0f / 30.0f;
		for (float t = 0.0f; t < seconds; t += kStep)
			Update(kStep, ground);
	}

	void ParticleSystem::Update(float dt, const GroundQuery &ground)
	{
		if (playing && emission.enabled)
		{
			m_emitAccumulator += emission.rateOverTime * emission.rateMultiplier * dt;
			const int n = static_cast<int>(m_emitAccumulator);
			m_emitAccumulator -= static_cast<float>(n);
			for (int i = 0; i < n; ++i)
			{
				glm::vec3 pos = position + shape.offset;
				if (shape.type == ShapeModule::Type::Box)
					pos += glm::vec3(Rand(-0.5f, 0.5f), Rand(-0.5f, 0.5f), Rand(-0.5f, 0.5f)) * shape.boxSize;
				Spawn(pos, shape.direction);
			}
		}

		for (size_t i = 0; i < m_particles.size();)
		{
			Particle &p = m_particles[i];
			p.age += dt;
			bool dead = p.age >= p.lifetime;

			if (!dead && !p.stuck)
			{
				p.vel.y -= 9.8f * main.gravityModifier * dt;
				p.pos += Velocity(p) * dt;

				if (collision.enabled && ground)
				{
					const float surface = std::max(ground(p.pos.x, p.pos.z), collision.minHeight);
					if (p.pos.y < surface + (collision.response == CollisionModule::Response::Stick ? collision.surfaceOffset : 0.0f))
					{
						if (collision.response == CollisionModule::Response::Kill)
						{
							ParticleSystem *sub = collision.subEmitter;
							if (sub && Rand(0.0f, 1.0f) < collision.subEmitterChance)
							{
								const glm::vec3 d = p.pos - focusPoint;
								if (d.x * d.x + d.z * d.z < collision.subEmitterRange * collision.subEmitterRange)
									sub->Emit({p.pos.x, surface + collision.surfaceOffset, p.pos.z}, glm::vec3(0.0f), 1);
							}
							dead = true;
						}
						else
						{
							p.pos.y = surface + collision.surfaceOffset;
							p.vel = glm::vec3(0.0f);
							p.stuck = true;
							p.size *= collision.stickSizeMultiplier;
							p.lifetime = std::max(p.lifetime - p.age, collision.stickLifetime);
							p.age = 0.0f;
						}
					}
				}
			}

			if (dead)
			{
				p = m_particles.back();
				m_particles.pop_back();
			}
			else
			{
				++i;
			}
		}
	}

	void ParticleSystem::Collect(gfx::ParticleRenderer &out)
	{
		m_gpu.clear();
		for (const Particle &p : m_particles)
		{
			const float t = std::clamp(p.age / p.lifetime, 0.0f, 1.0f);
			const float size = p.size * (sizeOverLifetime.enabled ? glm::mix(sizeOverLifetime.start, sizeOverLifetime.end, t) : 1.0f);
			const glm::vec4 color = main.startColor * EvaluateColor(t);

			glm::vec4 vel(0.0f);
			if (renderer.mode == gfx::ParticleMode::Streak)
			{
				const glm::vec3 v = Velocity(p);
				const float speed = glm::length(v);
				if (speed > 1e-4f)
					vel = glm::vec4(v / speed, speed * renderer.velocityScale);
			}
			m_gpu.push_back({{p.pos, size}, vel, color});
		}
		out.Submit(m_gpu, renderer.mode);
	}

} // namespace eng
