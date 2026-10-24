#include "game/GameContent.h"
#include "eng/Animator.h"
#include "eng/AudioSource.h"
#include "eng/BuiltinComponents.h"
#include "eng/Lighting.h"
#include "eng/ParticleSystemComponent.h"
#include "eng/RigidBody.h"
#include "eng/Script.h"
#include "game/CarnivoreAI.h"
#include "game/DinosaurAI.h"
#include "game/EscapeZone.h"
#include "game/FirstPersonController.h"
#include "game/GrassGen.h"
#include "game/HerbivoreAI.h"
#include "game/HerbivoreModel.h"
#include "game/Interaction.h"
#include "game/IslandScene.h"
#include "game/Plant.h"
#include "game/RaptorModel.h"
#include "game/TreeGen.h"
#include "game/Weapon.h"
#include "gfx/ModelLoader.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace game
{

	namespace
	{

		float Param(const std::vector<float> &p, size_t i, float fallback)
		{
			return i < p.size() ? p[i] : fallback;
		}

	} // namespace

	void RegisterGameContent(eng::SceneLoader &loader, GameContent &content)
	{
		GameContent *c = &content;
		const auto behaviour = [&](const char *name, eng::SceneLoader::ScriptFactory factory)
		{
			if (c->gameplay)
				loader.RegisterScript(name, std::move(factory));
			else
				loader.RegisterScript(name, [](eng::GameObject &, const std::vector<float> &) {});
		};

		loader.RegisterMesh(kMeshTerrain, [c](const eng::MeshDesc &d) { return c->terrain->BuildMesh(static_cast<int>(Param(d.params, 0, 160.0f))); }, false);
		// assets/models/raptor.* (rigged, faces +X, origin at the hit-box center) replaces the procedural raptor.
		std::shared_ptr<gfx::SkinnedModel> raptorModel;
		std::shared_ptr<const gfx::Skeleton> raptorSkeleton;
		std::shared_ptr<const std::vector<gfx::AnimationClip>> raptorClips;
		if (const std::string external = gfx::FindModelOverride("raptor"); !external.empty())
		{
			try
			{
				raptorModel = std::make_shared<gfx::SkinnedModel>(gfx::LoadSkinnedModel(external));
				raptorSkeleton = std::shared_ptr<const gfx::Skeleton>(raptorModel, &raptorModel->skeleton);
				raptorClips = std::make_shared<const std::vector<gfx::AnimationClip>>(raptorModel->clips);
				spdlog::info("Raptor model loaded from {}", external);
			}
			catch (const std::exception &e)
			{
				spdlog::warn("Raptor model override ignored: {}", e.what());
				raptorModel.reset();
			}
		}
		loader.RegisterMesh(kMeshRaptor, [raptorModel](const eng::MeshDesc &) { return raptorModel ? raptorModel->mesh : RaptorMesh(); }, false);
		loader.RegisterMesh(kMeshTriceratops, [](const eng::MeshDesc &) { return HerbivoreMesh(HerbivoreSpecies::Triceratops); }, false);
		loader.RegisterMesh(kMeshProtoceratops, [](const eng::MeshDesc &) { return HerbivoreMesh(HerbivoreSpecies::Protoceratops); }, false);
		loader.RegisterMesh(kMeshTreeTrunk, [](const eng::MeshDesc &d) { return GenerateRainforestTree(static_cast<uint32_t>(Param(d.params, 0, 100.0f))).trunk; });
		loader.RegisterMesh(kMeshTreeCanopy, [](const eng::MeshDesc &d) { return GenerateRainforestTree(static_cast<uint32_t>(Param(d.params, 0, 100.0f))).canopy; });
		loader.RegisterMesh(kMeshFern, [](const eng::MeshDesc &d) { return GenerateFern(static_cast<uint32_t>(Param(d.params, 0, 200.0f))); });
		loader.RegisterMesh(kMeshGrass, [](const eng::MeshDesc &d) { return GenerateGrassTuft(static_cast<uint32_t>(Param(d.params, 0, 500.0f))); });

		behaviour(
			kScriptPlayer,
			[c](eng::GameObject &go, const std::vector<float> &p)
			{
				go.AddComponent<WeaponComponent>();

				auto &scripts = go.AddComponent<eng::ScriptComponent>();
				auto &fps = scripts.Add<FirstPersonController>();
				fps.SetLook(Param(p, 0, 0.0f), Param(p, 1, 0.0f));
				scripts.Add<InteractionScript>(*c->state);
				c->receiver = &scripts;
				c->player = &go;
				c->firstPerson = &fps;

				// The flashlight is a spot light entity that follows the camera; F toggles it.
				auto &flashlight = c->world->CreateGameObject("Flashlight");
				flashlight.AddComponent<eng::SpotLightComponent>(glm::vec3(1.0f, 0.95f, 0.85f), 220.0f, 50.0f, 11.0f, 21.0f).SetCastShadow(true);
				flashlight.AddComponent<eng::CameraFollower>(glm::vec3(0.15f, -0.12f, 0.3f));
			}
		);

		behaviour(
			kScriptDinosaur,
			[c, raptorModel, raptorSkeleton, raptorClips](eng::GameObject &go, const std::vector<float> &p)
			{
				eng::GameObject *player = c->world->Find("Player");
				if (!player)
					throw std::runtime_error("Raptor needs a Player object");
				if (raptorModel)
				{
					auto &animator = go.AddComponent<eng::Animator>(raptorSkeleton, raptorClips);
					if (!raptorModel->clips.empty())
						animator.Play(raptorModel->clips.front().name);
				}
				else
					go.AddComponent<eng::Animator>(RaptorSkeleton()).hook = RaptorPose;
				go.AddComponent<eng::ScriptComponent>().Add<DinosaurAI>(*player, *c->state, Param(p, 0, 8.0f), Param(p, 1, 12.0f), Param(p, 2, 0.0f) > 0.5f);
			}
		);

		behaviour(
			kScriptCarnivore,
			[c, raptorModel, raptorSkeleton, raptorClips](eng::GameObject &go, const std::vector<float> &p)
			{
				eng::GameObject *player = c->world->Find("Player");
				if (!player)
					throw std::runtime_error("T-Rex needs a Player object");
				if (raptorModel)
				{
					auto &animator = go.AddComponent<eng::Animator>(raptorSkeleton, raptorClips);
					if (!raptorModel->clips.empty())
						animator.Play(raptorModel->clips.front().name);
				}
				else
					go.AddComponent<eng::Animator>(RaptorSkeleton()).hook = RaptorPose;
				go.AddComponent<eng::ScriptComponent>().Add<CarnivoreAI>(*player, *c->state, Param(p, 0, 7.5f), Param(p, 1, 35.0f));
			}
		);

		behaviour(
			kScriptHerbivore,
			[c](eng::GameObject &go, const std::vector<float> &p)
			{
				eng::GameObject *player = c->world->Find("Player");
				if (!player)
					throw std::runtime_error("Herbivore needs a Player object");
				const auto species = Param(p, 0, 0.0f) < 0.5f ? HerbivoreSpecies::Triceratops : HerbivoreSpecies::Protoceratops;
				go.AddComponent<eng::Animator>(HerbivoreSkeleton(species)).hook = HerbivorePose;
				go.AddComponent<eng::ScriptComponent>().Add<HerbivoreAI>(species, *player, *c->state, Param(p, 1, 6.0f));
			}
		);

		behaviour(
			kScriptPlant,
			[c](eng::GameObject &go, const std::vector<float> &p) { go.AddComponent<Plant>(c->state->ecology, static_cast<int>(Param(p, 0, 3.0f)), Param(p, 1, 60.0f)); }
		);

		behaviour(
			kScriptEscape,
			[c](eng::GameObject &go, const std::vector<float> &p)
			{
				eng::GameObject *player = c->world->Find("Player");
				if (!player)
					throw std::runtime_error("Escape zone needs a Player object");
				go.AddComponent<eng::ScriptComponent>().Add<EscapeZone>(*player, *c->state, Param(p, 0, 6.0f));
			}
		);

		behaviour(kScriptHealthPickup, [](eng::GameObject &go, const std::vector<float> &p) { go.AddComponent<HealthPickup>(Param(p, 0, 35.0f)); });

		behaviour(
			kScriptWeaponPickup,
			[](eng::GameObject &go, const std::vector<float> &p)
			{
				WeaponStats s;
				s.name = go.name;
				s.damage = Param(p, 0, s.damage);
				s.cooldown = Param(p, 1, s.cooldown);
				s.magSize = static_cast<int>(Param(p, 2, static_cast<float>(s.magSize)));
				s.reserve = static_cast<int>(Param(p, 3, static_cast<float>(s.reserve)));
				s.automatic = Param(p, 4, 0.0f) > 0.5f;
				s.reloadTime = Param(p, 5, s.reloadTime);
				s.range = Param(p, 6, s.range);
				go.AddComponent<WeaponPickup>(s);
			}
		);

		behaviour(
			kScriptDoor,
			[](eng::GameObject &go, const std::vector<float> &p)
			{ go.AddComponent<Door>(go.name, glm::vec3(Param(p, 0, 0.0f), Param(p, 1, 0.0f), Param(p, 2, 0.0f)), Param(p, 3, 1.5f)); }
		);

		loader.RegisterScript(
			kScriptLightPost,
			[](eng::GameObject &go, const std::vector<float> &p)
			{
				go.AddComponent<eng::PointLightComponent>(glm::vec3(Param(p, 4, 1.0f), Param(p, 5, 0.7f), Param(p, 6, 0.4f)), Param(p, 0, 120.0f), Param(p, 1, 28.0f))
					.SetCastShadow(false);
				go.AddComponent<eng::LightFlicker>(Param(p, 2, 0.5f), static_cast<uint32_t>(Param(p, 3, 1.0f)));
			}
		);

		// params: type (0 point, 1 spot, 2 directional), r, g, b, intensity, range, inner degrees, outer degrees
		loader.RegisterScript(
			kScriptLight,
			[](eng::GameObject &go, const std::vector<float> &p)
			{
				go.AddComponent<eng::LightComponent>(
					static_cast<gfx::LightType>(static_cast<int>(Param(p, 0, 0.0f))),
					glm::vec3(Param(p, 1, 1.0f), Param(p, 2, 1.0f), Param(p, 3, 1.0f)),
					Param(p, 4, 10.0f),
					Param(p, 5, 20.0f),
					Param(p, 6, 12.0f),
					Param(p, 7, 22.0f)
				);
			}
		);

		// params: amount (0..1), wind x, wind z
		loader.RegisterScript(
			kScriptRain,
			[](eng::GameObject &go, const std::vector<float> &p)
			{
				auto &rain = go.AddComponent<eng::ParticleSystemComponent>();
				eng::ConfigureRain(rain, Param(p, 0, 1.0f), glm::vec3(Param(p, 1, 2.5f), 0.0f, Param(p, 2, 1.0f)));
				go.AddComponent<eng::AudioSource>("rain_loop", true, 0.55f, false);
			}
		);

		// params: volume
		loader.RegisterScript(
			kScriptWind, [](eng::GameObject &go, const std::vector<float> &p) { go.AddComponent<eng::AudioSource>("wind_loop", true, Param(p, 0, 0.4f), false); }
		);

		// params: fov, near, far (an editor-placed camera; no runtime behaviour yet)

		// params: rounds
		behaviour(kScriptAmmoCrate, [](eng::GameObject &go, const std::vector<float> &p) { go.AddComponent<AmmoCrate>(static_cast<int>(Param(p, 0, 24.0f))); });
	}

	void ConfigureWorld(eng::World &world, const eng::SceneData &scene, std::shared_ptr<gfx::EnvironmentMap> environment, Terrain &terrain)
	{
		const eng::EnvironmentDesc &env = scene.environment;
		world.environment = std::move(environment);
		world.DayNight().enabled = env.dayNightCycle;
		world.DayNight().dayLengthSeconds = std::max(env.dayLength, 1.0f);
		world.DayNight().SetTime(env.timeOfDay);
		world.SetWind({env.windX, env.windZ});
		world.Weather().automatic = env.weatherAutomatic;
		world.Weather().transitionSeconds = env.weatherTransition;
		if (env.weather >= 0 && env.weather < static_cast<int>(eng::WeatherType::Count) && static_cast<eng::WeatherType>(env.weather) != world.Weather().Current())
			world.Weather().Set(static_cast<eng::WeatherType>(env.weather));
		world.Physics().SetHeightField([&terrain](float x, float z) { return terrain.Height(x, z); });
		world.Physics().SetBoundsRadius(Terrain::kWalkRadius);
	}

} // namespace game
