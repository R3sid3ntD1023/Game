#include "eng/World.h"
#include "eng/BuiltinComponents.h"
#include "eng/AudioSource.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace eng
{

	World::World()
		: m_renderer(m_meshes)
	{
		static float scale = 10.0f;
		static gfx::Line axes[3]
			= {{{0.0f, 0.0f, 0.0f}, {scale, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
			   {{0.0f, 0.0f, 0.0f}, {0.0f, scale, 0.0f}, {0.0f, 1.0f, 0.0f}},
			   {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, scale}, {0.0f, 0.0f, 1.0f}}};

		m_lineRenderer.DrawPersistentLine(
			{[this](gfx::LineRenderer &lr)
			 {
				 for (const auto &line : axes)
					 lr.DrawLine(line);
			 },
			 -1.0f}
		);
	}

	World::~World()
	{
		for (auto &o : m_objects)
			o->Shutdown();
		m_objects.clear();
	}

	GameObject &World::CreateGameObject(const std::string &name, const glm::vec3 &position)
	{
		const entt::entity e = m_registry.create();
		m_registry.emplace<Transform>(e, Transform{position});
		m_objects.push_back(std::make_unique<GameObject>(*this, e, name));
		return *m_objects.back();
	}

	GameObject *World::Find(const std::string &name) const
	{
		for (const auto &o : m_objects)
			if (o->IsAlive() && o->name == name)
				return o.get();
		return nullptr;
	}

	gfx::Camera &World::GetCamera()
	{
		static gfx::Camera defaultCamera;
		auto view = m_registry.view<CameraComponent>();
		for (auto e : view)
		{
			auto &cam = view.get<CameraComponent>(e);
			if (cam.active)
				return cam.camera;
		}

		return defaultCamera;
	}

	void World::UnregisterParticles(ParticleSystemComponent *e)
	{
		m_particleComponents.erase(std::remove(m_particleComponents.begin(), m_particleComponents.end(), e), m_particleComponents.end());
		if (m_bloodEmitter == e)
			m_bloodEmitter = nullptr;
	}

	void World::EmitBlood(const glm::vec3 &position, const glm::vec3 &direction, int count)
	{
		if (!m_bloodEmitter)
		{
			m_bloodEmitter = &CreateGameObject("BloodFX").AddComponent<ParticleSystemComponent>();
			ConfigureBlood(*m_bloodEmitter);
		}
		m_bloodEmitter->Primary().Emit(position, direction, count);
	}

	void World::EmitNoise(const glm::vec3 &position, float radius, const GameObject *source)
	{
		m_noises.push_back({m_nextNoiseId++, position, radius, 0.0f, source});
	}

	namespace
	{
		float RandRange(std::mt19937 &rng, float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }

		void Subdivide(std::mt19937 &rng, std::vector<glm::vec3> &pts, float offset, int levels)
		{
			for (int l = 0; l < levels; ++l)
			{
				std::vector<glm::vec3> next;
				next.reserve(pts.size() * 2);
				for (size_t i = 0; i + 1 < pts.size(); ++i)
				{
					glm::vec3 mid = (pts[i] + pts[i + 1]) * 0.5f;
					mid.x += RandRange(rng, -offset, offset);
					mid.z += RandRange(rng, -offset, offset);
					next.push_back(pts[i]);
					next.push_back(mid);
				}
				next.push_back(pts.back());
				pts = std::move(next);
				offset *= 0.55f;
			}
		}
	} // namespace

	// Jagged bolt from the cloud deck to the ground by midpoint displacement, with a couple of side branches.
	void World::BuildBolt(const glm::vec3 &cameraPos, const LightningStrike &strike)
	{
		static std::mt19937 rng(777u);
		glm::vec3 base = cameraPos + glm::vec3(std::cos(strike.azimuth), 0.0f, std::sin(strike.azimuth)) * strike.distance;
		base.y = GetTerrainHeight(base.x, base.z);
		const glm::vec3 top = base + glm::vec3(RandRange(rng, -25.0f, 25.0f), 190.0f, RandRange(rng, -25.0f, 25.0f));

		std::vector<glm::vec3> main{top, base};
		Subdivide(rng, main, 16.0f, 6);

		m_boltSegments.clear();
		for (size_t i = 0; i + 1 < main.size(); ++i)
			m_boltSegments.push_back({main[i], main[i + 1]});

		for (int b = 0; b < 3; ++b)
		{
			const size_t at = static_cast<size_t>(RandRange(rng, 0.15f, 0.7f) * static_cast<float>(main.size()));
			const glm::vec3 start = main[std::min(at, main.size() - 1)];
			const glm::vec3 end = start + glm::vec3(RandRange(rng, -40.0f, 40.0f), -RandRange(rng, 30.0f, 70.0f), RandRange(rng, -40.0f, 40.0f));
			std::vector<glm::vec3> branch{start, end};
			Subdivide(rng, branch, 8.0f, 4);
			for (size_t i = 0; i + 1 < branch.size(); ++i)
				m_boltSegments.push_back({branch[i], branch[i + 1]});
		}
		m_boltTimer = 0.6f;
	}

	void World::UpdateWeather(float dt, const gfx::Camera &camera)
	{
		m_weather.Update(dt);
		const WeatherProfile &w = m_weather.Blend();

		// Rain falls over the whole island; the weather only sets its intensity.
		m_weatherLookupTimer -= dt;
		if ((!m_rainObject || !m_windObject) && m_weatherLookupTimer <= 0.0f)
		{
			m_weatherLookupTimer = 2.0f;
			if (!m_rainObject)
				m_rainObject = Find("Rain");
			if (!m_windObject)
				m_windObject = Find("Wind");
		}

		static std::mt19937 rng(4242u);
		if (m_rainObject)
		{
			if (auto *particles = m_rainObject->GetComponent<ParticleSystemComponent>())
				particles->SetEmissionScale(w.rain);
			if (auto *sound = m_rainObject->GetComponent<AudioSource>())
				sound->SetVolume(0.55f * (0.35f + 0.65f * w.rain) * (w.rain > 0.01f ? 1.0f : 0.0f));
		}
		if (m_windObject)
			if (auto *sound = m_windObject->GetComponent<AudioSource>())
				sound->SetVolume(0.4f * std::clamp(w.wind / 1.1f, 0.25f, 1.6f));

		LightningStrike strike;
		if (m_weather.TakeStrike(strike))
			BuildBolt(camera.position, strike);
		float thunderVolume = 0.0f;
		if (m_weather.TakeThunder(thunderVolume) && audio)
			audio->Play2D("thunder", thunderVolume, 0.9f + 0.2f * RandRange(rng, 0.0f, 1.0f));

		// The bolt flickers with the flash and is gone after about half a second.
		if (m_boltTimer > 0.0f)
		{
			m_boltTimer -= dt;
			const float f = m_weather.Flash();
			if (f > 0.08f)
			{
				const glm::vec3 color = glm::vec3(0.85f, 0.9f, 1.0f) * std::min(1.0f, 0.35f + f);
				for (const auto &[a, b] : m_boltSegments)
					for (const glm::vec3 off : {glm::vec3(0.0f), glm::vec3(0.35f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.35f)})
						m_lineRenderer.QueueLine({a + off, b + off, color});
			}
		}
	}

	void World::Update(float dt, const gfx::Camera &camera)
	{
		m_deltaTime = dt;
		UpdateWeather(dt, camera);
		for (auto &n : m_noises)
			n.age += dt;
		if (debugPerception)
		{
			// Noise rings expand to their audible radius within a quarter second.
			for (const auto &n : m_noises)
				m_lineRenderer.QueueCircle(n.position, n.radius * std::min(1.0f, n.age * 4.0f), {0.2f, 0.8f, 1.0f}, 40);
		}
		m_noises.erase(std::remove_if(m_noises.begin(), m_noises.end(), [](const NoiseEvent &n) { return n.age > 1.0f; }), m_noises.end());
		m_dayNight.Advance(dt);
		m_windTime += dt;

		for (size_t i = 0; i < m_objects.size(); ++i)
			m_objects[i]->Update(dt);

		// Cap catch-up steps so a slow frame can't snowball into ever more AI/physics work.
		m_accumulator = std::min(m_accumulator + dt, 4.0f * kFixedStep);
		while (m_accumulator >= kFixedStep)
		{
			for (size_t i = 0; i < m_objects.size(); ++i)
				m_objects[i]->FixedUpdate(kFixedStep);
			m_physics.Step(kFixedStep);
			m_accumulator -= kFixedStep;
		}

		for (auto &o : m_objects)
		{
			if (o->IsAlive())
				continue;
			o->Shutdown();
			m_registry.destroy(o->Entity());
		}
		m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(), [](const auto &o) { return !o->IsAlive(); }), m_objects.end());

		if (audio)
			audio->SetListener(camera.position, camera.forward, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	void World::Render(float aspect, GLuint depthTexture, int width, int height, const gfx::Camera &camera)
	{
		const glm::mat4 view = camera.View();
		const glm::mat4 proj = camera.Projection(aspect);
		const glm::mat4 viewProj = proj * view;

		m_renderer.SetWind(m_windTime, m_windDirection * m_weather.Blend().wind);
		m_renderer.Build(m_registry);

		const WeatherProfile &weather = m_weather.Blend();
		if (environment)
			environment->SetWeather(weather.cloud, weather.dark, m_windTime, m_weather.Flash());

		// Sun and moon from the clock replace scene-authored directional lights; clouds dim them.
		const bool cycle = m_dayNight.enabled;
		const glm::vec3 sunDir = m_dayNight.SunDirection();
		if (cycle && environment)
			environment->SetSunDirection(sunDir);

		gfx::LightData sun, moon;
		bool shadowsOn = false;
		if (cycle)
		{
			const float sunElev = sunDir.y;
			const float sunI = 3.0f * glm::smoothstep(-0.02f, 0.15f, sunElev) * weather.light;
			sun.type = gfx::LightType::Directional;
			sun.direction = -sunDir;
			sun.color = glm::mix(glm::vec3(1.0f, 0.55f, 0.3f), glm::vec3(1.0f, 0.96f, 0.85f), glm::smoothstep(0.0f, 0.4f, sunElev)) * sunI;

			const float moonI = glm::smoothstep(-0.02f, 0.15f, -sunElev) * glm::mix(1.0f, weather.light, 0.7f);
			moon.type = gfx::LightType::Directional;
			moon.direction = sunDir;
			moon.color = glm::vec3(0.35f, 0.42f, 0.65f) * moonI;

			const bool sunCasts = sunI >= moonI;
			gfx::LightData &caster = sunCasts ? sun : moon;
			const float casterElev = sunCasts ? sunElev : -sunElev;
			shadowsOn = casterElev > 0.03f && glm::length(caster.color) > 0.01f;
			caster.castShadow = shadowsOn;
			if (shadowsOn)
				m_shadows.Prepare(camera, aspect, caster.direction);
		}

		if (shadowsOn)
		{
			for (int c = 0; c < gfx::ShadowMap::kCascades; ++c)
			{
				m_shadows.Begin(c);
				m_renderer.DepthPass(m_shadows.ViewProj(c));
				m_shadows.End();
			}
		}
		m_renderer.DepthPass(viewProj);

		// Directional lights first, then the nearest local lights up to the Forward+ budget.
		std::vector<gfx::LightData> lights;
		{
			struct Candidate
			{
				float d2;
				gfx::LightData data;
			};
			std::vector<Candidate> found;
			if (cycle)
			{
				if (glm::length(sun.color) > 0.001f)
					lights.push_back(sun);
				if (glm::length(moon.color) > 0.001f)
					lights.push_back(moon);
			}
			if (m_weather.Flash() > 0.01f)
			{
				// Lightning lights the whole scene from above for a split second.
				gfx::LightData bolt;
				bolt.type = gfx::LightType::Directional;
				bolt.direction = glm::normalize(glm::vec3(0.2f, -1.0f, 0.3f));
				bolt.color = glm::vec3(0.7f, 0.78f, 1.0f) * (6.0f * m_weather.Flash());
				lights.push_back(bolt);
			}
			m_registry.view<Transform, Light>().each(
				[&](const Transform &t, const Light &l)
				{
					if (!l.enabled || l.intensity <= 0.001f)
						return;
					if (cycle && l.type == gfx::LightType::Directional)
						return;
					gfx::LightData d;
					d.type = l.type;
					d.position = t.position;
					d.range = l.range;
					d.color = l.color * l.intensity;
					d.direction = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
					d.innerCos = std::cos(glm::radians(l.innerDegrees));
					d.outerCos = std::cos(glm::radians(l.outerDegrees));
					d.castShadow = l.castShadow;

					const glm::vec3 to = t.position - camera.position;
					const float d2 = l.type == gfx::LightType::Directional ? -1.0f : glm::dot(to, to);
					if (d2 < 200.0f * 200.0f)
						found.push_back({d2, d});
				}
			);
			std::sort(found.begin(), found.end(), [](const Candidate &a, const Candidate &b) { return a.d2 < b.d2; });
			for (size_t i = 0; i < found.size() && i < static_cast<size_t>(gfx::kMaxLights); ++i)
				lights.push_back(found[i].data);
		}

		// The nearest shadow-casting point/spot lights get a slot; their faces are rendered now.
		m_localShadows.Reset();
		for (gfx::LightData &l : lights)
		{
			if (l.type == gfx::LightType::Directional || !l.castShadow)
				continue;
			if (m_localShadows.Full() || glm::distance(l.position, camera.position) > l.range + 40.0f)
				continue;
			int faces = 0;
			l.shadowSlot = m_localShadows.Assign(l, faces);
			for (int f = 0; f < faces; ++f)
			{
				m_localShadows.Begin(l.shadowSlot, f);
				m_renderer.DepthPass(m_localShadows.ViewProj(l.shadowSlot, f), true);
				m_localShadows.End();
			}
		}
		m_forwardPlus.Cull(lights, depthTexture, view, proj, width, height);

		if (environment)
			environment->BindForLighting();
		m_shadows.Bind(shadowsOn);
		m_localShadows.Bind();
		m_renderer.ShadePass(viewProj, camera.position, m_forwardPlus.TilesX());
		if (environment)
			environment->DrawSky(view, proj, camera.position);


		m_particleRenderer.Begin();
		for (ParticleSystemComponent *e : m_particleComponents)
			e->Collect(m_particleRenderer);
		m_particleRenderer.Render(view, proj, camera.position);

		if (m_navDebugDirty && m_worldQuery)
			RebuildNavDebug();
		UpdateNavDebugVisibility(camera.position);

		m_renderer.OverlayPass(viewProj);
		m_quadRenderer.Render(viewProj);
		m_lineRenderer.Begin(m_deltaTime, viewProj);
		m_lineRenderer.Render(viewProj);
	}

	void World::SetWorldQuery(IWorldQuery *query)
	{
		m_worldQuery = query;
		if (m_worldQuery)
		{
			// Cell size 2m covers the 700m terrain with 350x350 cells (4x cheaper than 1m).
			const float cellSize = 2.0f;
			const int cells = static_cast<int>(2.0f * m_worldQuery->GetHalfSize() / cellSize);
			m_navigationGrid.Initialize(cells, cells, cellSize, *m_worldQuery);
			m_navDebugDirty = true;
		}
	}

	void World::RebuildNavDebug()
	{
		m_navDebugDirty = false;
		auto &grid = m_navigationGrid;
		if (grid.width <= 0 || grid.height <= 0)
			return;

		const bool first = m_navChunks.empty();
		size_t index = 0;
		for (int y0 = 0; y0 < grid.height; y0 += kNavChunkCells)
		{
			for (int x0 = 0; x0 < grid.width; x0 += kNavChunkCells, ++index)
			{
				const int x1 = std::min(x0 + kNavChunkCells, grid.width);
				const int y1 = std::min(y0 + kNavChunkCells, grid.height);
				auto buildGrid = [&](gfx::LineRenderer &lr) { lr.DrawNavigationGrid(grid, 1, x0, y0, x1, y1); };

				if (first)
				{
					NavChunk c;
					const auto &a = grid.cells[y0 * grid.width + x0].position;
					const auto &b = grid.cells[(y1 - 1) * grid.width + (x1 - 1)].position;
					c.center = {(a.x + b.x) * 0.5f, (a.z + b.z) * 0.5f};
					c.grid = m_lineRenderer.CreateStaticBatch(buildGrid);
					m_lineRenderer.SetStaticBatchVisible(c.grid, false);
					m_navChunks.push_back(c);
				}
				else
				{
					m_lineRenderer.RebuildStaticBatch(m_navChunks[index].grid, buildGrid);
				}
			}
		}

		m_renderer.SetOverlayMesh(m_meshes.Create(grid.BuildMesh()));
		m_renderer.SetOverlayStyle(m_navDebugMode == 2 ? 1 : 0, {grid.cells.front().position.x, grid.cells.front().position.z}, grid.cellSize);
		m_renderer.SetOverlayVisible(m_navMeshVisible);
	}

	void World::UpdateNavDebugVisibility(const glm::vec3 &cameraPos)
	{
		const float reach = kNavDrawRadius + kNavChunkCells * m_navigationGrid.cellSize;
		for (const NavChunk &c : m_navChunks)
		{
			const bool near = glm::distance(c.center, glm::vec2(cameraPos.x, cameraPos.z)) < reach;
			m_lineRenderer.SetStaticBatchVisible(c.grid, near && m_navGridVisible);
		}
	}

	void World::SetNavDebugMode(int mode)
	{
		m_navDebugMode = mode & 3;
		m_navGridVisible = m_navDebugMode == 3;
		m_navMeshVisible = m_navDebugMode == 1 || m_navDebugMode == 2;
		m_renderer.SetOverlayMode(m_navDebugMode == 2 ? 1 : 0);
		m_renderer.SetOverlayVisible(m_navMeshVisible);
	}

	void World::SetNavGridVisible(bool visible)
	{
		m_navGridVisible = visible;
	}

	void World::SetNavMeshVisible(bool visible)
	{
		m_navMeshVisible = visible;
		m_renderer.SetOverlayVisible(visible);
	}

	float World::GetTerrainHeight(float x, float z) const
	{
		return m_worldQuery ? m_worldQuery->Height(x, z) : 0.0f;
	}

	bool World::IsWalkable(float x, float z) const
	{
		return m_worldQuery ? m_worldQuery->IsWalkable(x, z) : true;
	}

	bool World::FindRandomReachablePoint(glm::vec3 &out, const glm::vec3 &center, float radius) const
	{
		static thread_local std::mt19937 gen{std::random_device{}()};

		if (m_worldQuery && m_navigationGrid.width > 0)
			return m_navigationGrid.RandomReachablePoint(center, radius, 8.0f, gen, out);

		std::uniform_real_distribution<float> distX(center.x - radius, center.x + radius);
		std::uniform_real_distribution<float> distZ(center.z - radius, center.z + radius);

		for (int attempt = 0; attempt < 100; ++attempt)
		{
			const float x = distX(gen);
			const float z = distZ(gen);
			if (IsWalkable(x, z))
			{
				out = glm::vec3(x, GetTerrainHeight(x, z), z);
				return true;
			}
		}
		return false;
	}

	void World::AddNavigationObstacle(const glm::vec3 &position, const glm::vec3 &halfExtents)
	{
		// Agent clearance in metres (was 5m, which swallowed everything around obstacles).
		m_navigationGrid.AddBoxObstacle(position, halfExtents.x, halfExtents.z, 0.6f);
		m_navDebugDirty = true;
	}

} // namespace eng
