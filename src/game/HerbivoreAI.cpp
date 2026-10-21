#include "game/HerbivoreAI.h"
#include "ai/Nodes.h"
#include "eng/World.h"
#include "game/CarnivoreAI.h"
#include "game/DinosaurAI.h"
#include "game/Survival.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

namespace game
{

	namespace
	{
		float Rand01() { return static_cast<float>(std::rand() % 1001) / 1000.0f; }

		glm::vec3 Flat(glm::vec3 v)
		{
			v.y = 0.0f;
			return v;
		}

		constexpr float kForageHunger = 0.45f;
		constexpr float kFoodSearchRadius = 90.0f;
	} // namespace

	HerbivoreAI::HerbivoreAI(HerbivoreSpecies species, eng::GameObject &player, GameState &state, float speed)
		: m_species(species),
		  m_player(player),
		  m_state(state),
		  m_ecology(state.ecology),
		  m_speed(speed),
		  m_hunger(0.2f + 0.6f * Rand01()),
		  m_callTimer(5.0f + 25.0f * Rand01())
	{
	}

	HerbivoreAI::~HerbivoreAI()
	{
		if (m_registered)
		{
			ReleaseClaim();
			Ecology::Remove(m_ecology->herbivores, this);
		}
	}

	void HerbivoreAI::OnStart()
	{
		m_body = gameObject->GetComponent<eng::RigidBody>();
		m_health = gameObject->GetComponent<eng::Health>();
		m_animator = gameObject->GetComponent<eng::Animator>();

		m_id = m_ecology->NextHerbivoreId();
		m_ecology->herbivores.push_back(this);
		m_registered = true;

		m_health->SetOnDamageCallback(
			[this](float)
			{
				if (m_health->Dead())
				{
					SetState(HerbivoreState::Dead);
					m_body->velocity = glm::vec3(0.0f);
					gameObject->GetWorld().EmitBlood(gameObject->GetTransform().position, glm::vec3(0.0f, 1.0f, 0.0f), Large() ? 80 : 40);
					gameObject->Destroy();
					return;
				}
				m_hurtTimer = kRecentlyHurtSeconds;
				if (!m_animalDamage)
					m_panicTimer = 6.0f;
			}
		);

		InitializeBehaviorTree();
	}

	void HerbivoreAI::Hurt(float amount)
	{
		if (Dead())
			return;
		if (auto *audio = gameObject->GetWorld().audio)
			audio->PlayOneShot("flesh_hit", gameObject->GetTransform().position, 1.0f, 0.9f + 0.2f * Rand01());
		m_animalDamage = true;
		m_health->Damage(amount);
		m_animalDamage = false;
	}

	void HerbivoreAI::ReleaseClaim()
	{
		if (m_claimed)
			m_claimed->Release(this);
		m_claimed = nullptr;
	}

	void HerbivoreAI::OnUpdate(float dt)
	{
		if (!m_body || Dead())
			return;

		m_goreTimer = std::max(0.0f, m_goreTimer - dt);
		m_panicTimer = std::max(0.0f, m_panicTimer - dt);
		m_hurtTimer = std::max(0.0f, m_hurtTimer - dt);
		if (m_stateEnum == HerbivoreState::Sleeping)
			m_health->Heal(m_health->max * kSleepHealPerSecond * dt);
		m_hunger = std::min(1.0f, m_hunger + dt / 70.0f);
		if (m_claimed && m_stateEnum != HerbivoreState::Seeking && m_stateEnum != HerbivoreState::Eating)
			ReleaseClaim();

		UpdateAudio(dt);
		UpdateAnimation(dt);

		m_aiContext.deltaTime = dt;
		m_behaviorTree.Tick(m_aiContext);

		if (gameObject->GetWorld().debugPerception)
			DrawDebug();
	}

	// Mirrors AIPerception::DrawDebug: green = calm, yellow = a raptor within the outer range, red = fleeing/fighting.
	void HerbivoreAI::DrawDebug() const
	{
		gfx::LineRenderer &lines = gameObject->GetWorld().Lines();
		const glm::vec3 pos = Position();
		const glm::vec3 ground = pos - glm::vec3(0.0f, HerbivoreHalfExtents(m_species).y - 0.1f, 0.0f);
		const glm::vec3 green{0.2f, 1.0f, 0.3f}, yellow{1.0f, 0.9f, 0.1f}, red{1.0f, 0.15f, 0.1f};

		float distance = 0.0f;
		const DinosaurAI *threat = FindThreat(distance);
		const bool alarmed = m_threatened || m_panicTimer > 0.0f;
		const glm::vec3 color = alarmed ? red : (threat ? yellow : green);

		// Threat range (with hysteresis while alarmed) and the bite radius raptors must reach.
		const float range = (Large() ? 13.0f : 18.0f) * (m_threatened ? 1.3f : 1.0f);
		lines.QueueCircle(ground, range, color, 32);
		lines.QueueCircle(ground, Radius(), {0.9f, 0.2f, 0.9f}, 16);

		// Awareness bar: full while alarmed, otherwise how close the nearest raptor is to the threat range.
		const glm::vec3 right{1.0f, 0.0f, 0.0f};
		const glm::vec3 barBase = pos + glm::vec3(0.0f, HerbivoreHalfExtents(m_species).y + 1.0f, 0.0f);
		const float fill = alarmed ? 1.0f : (threat ? std::clamp(1.0f - distance / range, 0.0f, 1.0f) : 0.0f);
		lines.QueueLine({barBase - right * 0.6f, barBase + right * 0.6f, glm::vec3(0.3f)});
		lines.QueueLine({barBase - right * 0.6f + glm::vec3(0, 0.05f, 0), barBase - right * 0.6f + right * 1.2f * fill + glm::vec3(0, 0.05f, 0), alarmed ? red : yellow});

		// Hunger bar (cyan) just below the awareness bar.
		const glm::vec3 hungerBase = barBase - glm::vec3(0.0f, 0.25f, 0.0f);
		lines.QueueLine({hungerBase - right * 0.6f, hungerBase + right * 0.6f, glm::vec3(0.3f)});
		lines.QueueLine({hungerBase - right * 0.6f, hungerBase - right * 0.6f + right * 1.2f * m_hunger, {0.2f, 0.9f, 1.0f}});

		if (threat)
			lines.QueueLine({pos, threat->gameObject->GetTransform().position, color});

		// The plant it is heading to / eating.
		if (m_claimed)
		{
			const glm::vec3 plant = m_claimed->Position();
			lines.QueueLine({pos, plant, {0.3f, 1.0f, 0.5f}});
			lines.QueueLine({plant, plant + glm::vec3(0, 2.0f, 0), {0.3f, 1.0f, 0.5f}});
			lines.QueueCircle(plant - glm::vec3(0, 0.2f, 0), 1.0f, {0.3f, 1.0f, 0.5f}, 16);
		}
	}

	DinosaurAI *HerbivoreAI::FindThreat(float &distance) const
	{
		const float range = (Large() ? 13.0f : 18.0f) * (m_threatened ? 1.3f : 1.0f);
		return m_ecology->NearestRaptor(Position(), range, &distance);
	}

	CarnivoreAI *HerbivoreAI::FindRex(float &distance) const
	{
		return m_ecology->NearestCarnivore(Position(), 30.0f, &distance);
	}

	bool HerbivoreAI::IsThreatened(ai::AIContext &)
	{
		float distance = 0.0f;
		m_threatened = FindThreat(distance) != nullptr || FindRex(distance) != nullptr || m_panicTimer > 0.0f;
		return m_threatened;
	}

	// Moves along `dir` unless the ground ahead is not walkable.
	bool HerbivoreAI::StepToward(const glm::vec3 &dir, float speed)
	{
		auto &transform = gameObject->GetTransform();
		auto &grid = gameObject->GetWorld().NavigationGrid();
		const glm::ivec2 cell = grid.WorldToGrid(transform.position + dir * 2.5f);
		if (!grid.IsWalkable(cell.x, cell.y))
			return false;
		m_body->velocity.x = dir.x * speed;
		m_body->velocity.z = dir.z * speed;
		transform.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
		return true;
	}

	ai::Node::State HerbivoreAI::React(ai::AIContext &)
	{
		auto &transform = gameObject->GetTransform();
		float distance = 0.0f, rexDistance = 0.0f;
		DinosaurAI *raptor = FindThreat(distance);
		CarnivoreAI *rex = FindRex(rexDistance);

		// A hurt animal that is still under attack runs instead of fighting. A T-Rex is too great a threat to
		// face, unless it has a healthy triceratops cornered.
		const bool hurtAndAttacked = m_hurtTimer > 0.0f && HealthFraction() < kFleeHealthFraction;
		const bool fightRex = Large() && rex && rexDistance < 6.0f && !hurtAndAttacked;
		const bool fightRaptor = Large() && raptor && !rex && !hurtAndAttacked;

		if (fightRex || fightRaptor)
		{
			// Stand and fight: charge the attacker and gore it.
			SetState(HerbivoreState::Defending);
			const glm::vec3 foe = fightRex ? rex->Position() : raptor->gameObject->GetTransform().position;
			glm::vec3 dir = Flat(foe - transform.position);
			const float len = glm::length(dir);
			if (len > 1e-3f)
				dir /= len;
			if (len > (fightRex ? 5.5f : 3.8f))
			{
				if (!StepToward(dir, m_speed))
					m_body->velocity.x = m_body->velocity.z = 0.0f;
			}
			else
			{
				m_body->velocity.x = m_body->velocity.z = 0.0f;
				transform.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
				if (m_goreTimer <= 0.0f)
				{
					m_goreTimer = 1.3f;
					if (fightRex)
						rex->Hurt(18.0f, transform.position);
					else
						raptor->Hurt(18.0f, &transform.position);
					gameObject->GetWorld().EmitBlood(foe + glm::vec3(0.0f, 0.5f, 0.0f), dir, 16);
					if (auto *audio = gameObject->GetWorld().audio)
						audio->PlayOneShot("triceratops_bellow", transform.position, 0.8f, 1.15f);
				}
			}
			return ai::Node::State::Running;
		}

		// Run: directly away from the predator, or from the player after being shot. Swerve around blocked ground.
		SetState(HerbivoreState::Fleeing);
		const glm::vec3 from = rex ? rex->Position() : raptor ? raptor->gameObject->GetTransform().position : m_player.GetTransform().position;
		glm::vec3 away = Flat(transform.position - from);
		if (glm::length(away) < 1e-3f)
			away = glm::vec3(1.0f, 0.0f, 0.0f);
		away = glm::normalize(away);

		bool moving = false;
		for (float degrees : {0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f})
		{
			if (StepToward(glm::angleAxis(glm::radians(degrees), glm::vec3(0, 1, 0)) * away, m_speed))
			{
				moving = true;
				break;
			}
		}
		if (!moving)
			m_body->velocity.x = m_body->velocity.z = 0.0f;
		return ai::Node::State::Running;
	}

	// Lie down and heal; sleeping restores health until the wounds have mended.
	ai::Node::State HerbivoreAI::Rest(ai::AIContext &)
	{
		m_body->velocity.x = m_body->velocity.z = 0.0f;
		SetState(HerbivoreState::Sleeping);
		return ai::Node::State::Running;
	}

	void HerbivoreAI::InitializeBehaviorTree()
	{
		m_aiContext.self = gameObject;
		m_aiContext.blackboard.Add<glm::vec3>("foodTarget");
		m_aiContext.blackboard.Add<void *>("foodPlant");
		m_aiContext.blackboard.Add<glm::vec3>("wanderTarget");

		auto &tree = m_behaviorTree;
		auto root = tree.AddNode<ai::Root>();
		root->SetName("Root");
		auto priority = tree.AddNode<ai::Selector>();
		priority->SetName("PrioritySelector");

		auto action = [&](const char *name, ai::ActionFunc fn)
		{
			auto node = tree.AddNode<ai::Action>();
			node->SetName(name);
			node->SetAction(std::move(fn));
			return node;
		};
		auto condition = [&](const char *name, ai::ConditionFunc fn)
		{
			auto node = tree.AddNode<ai::Conditional>();
			node->SetName(name);
			node->SetCondition(std::move(fn));
			return node;
		};

		// Threat: raptors, a T-Rex nearby (or being shot) override everything else.
		auto reactSequence = tree.AddNode<ai::Sequence>();
		reactSequence->SetName("ReactSequence");
		reactSequence->AddChild(condition("IsThreatened", [this](ai::AIContext &ctx) { return IsThreatened(ctx); }));
		reactSequence->AddChild(action("React", [this](ai::AIContext &ctx) { return React(ctx); }));

		auto restSequence = tree.AddNode<ai::Sequence>();
		restSequence->SetName("RestSequence");
		restSequence->AddChild(condition(
			"NeedsRest",
			[this](ai::AIContext &)
			{
				const bool wasResting = m_resting;
				m_resting = wasResting ? HealthFraction() < kHealedFraction : (HealthFraction() < kRestHealthFraction && m_hurtTimer <= 0.0f);
				if (wasResting && !m_resting)
					SetState(HerbivoreState::Idle);
				return m_resting;
			}
		));
		restSequence->AddChild(action("Rest", [this](ai::AIContext &ctx) { return Rest(ctx); }));

		// Forage: query for the best plant, walk to it, eat for a few seconds, then repeat until full.
		auto query = tree.AddNode<ai::RunEnvQuery>();
		query->SetName("FindFood");
		query->m_locationKey = "foodTarget";
		query->m_dataKey = "foodPlant";
		query->m_failCooldown = 4.0f;
		query->query.SetGenerator(
			[this](ai::AIContext &, std::vector<ai::EnvQueryItem> &items)
			{
				for (Plant *plant : m_ecology->plants)
					if (plant->Available())
						items.push_back({plant->Position(), plant});
			}
		);
		query->query.AddFilter([this](ai::AIContext &, const ai::EnvQueryItem &item)
							   { return glm::length(Flat(item.location - Position())) <= kFoodSearchRadius; });
		query->query.AddFilter([this](ai::AIContext &, const ai::EnvQueryItem &item) { return !static_cast<Plant *>(item.data)->ClaimedByOther(this); });
		query->query.AddFilter(
			[this](ai::AIContext &, const ai::EnvQueryItem &item)
			{
				auto &grid = gameObject->GetWorld().NavigationGrid();
				const glm::ivec2 from = grid.WorldToGrid(Position()), to = grid.WorldToGrid(item.location);
				return grid.IsWalkable(to.x, to.y) && grid.Connected(from.x, from.y, to.x, to.y);
			}
		);
		query->query.AddFilter([this](ai::AIContext &, const ai::EnvQueryItem &item)
							   { return m_ecology->NearestRaptor(item.location, 14.0f) == nullptr && m_ecology->NearestCarnivore(item.location, 24.0f) == nullptr; });
		query->query.AddScorer([this](ai::AIContext &, const ai::EnvQueryItem &item) { return 1.0f - glm::length(Flat(item.location - Position())) / kFoodSearchRadius; }, 1.0f);
		query->query.AddScorer([](ai::AIContext &, const ai::EnvQueryItem &item) { return static_cast<Plant *>(item.data)->Fullness(); }, 0.5f);
		query->query.AddScorer(
			[this](ai::AIContext &, const ai::EnvQueryItem &item)
			{
				float d = 60.0f;
				m_ecology->NearestRaptor(item.location, 60.0f, &d);
				return d / 60.0f;
			},
			0.5f
		);
		query->query.AddScorer([](ai::AIContext &, const ai::EnvQueryItem &) { return Rand01(); }, 0.3f);

		auto seekTo = tree.AddNode<ai::MoveTo>();
		seekTo->SetName("SeekTo");
		seekTo->m_target = "foodTarget";
		seekTo->m_speed = m_speed * 0.3f;

		auto eatWait = tree.AddNode<ai::Wait>();
		eatWait->SetName("EatWait");
		eatWait->m_duration = 4.5f;
		eatWait->m_deviation = 1.0f;

		auto forage = tree.AddNode<ai::MemSequence>();
		forage->SetName("ForageSequence");
		forage->AddChild(condition("IsHungry", [this](ai::AIContext &) { return m_hunger > kForageHunger; }));
		forage->AddChild(query);
		forage->AddChild(
			action(
				"ClaimPlant",
				[this](ai::AIContext &ctx)
				{
					auto *plant = static_cast<Plant *>(ctx.blackboard.Get<void *>("foodPlant"));
					if (!plant || !plant->Available() || plant->ClaimedByOther(this))
						return ai::Node::State::Failure;
					ReleaseClaim();
					plant->Claim(this);
					m_claimed = plant;
					SetState(HerbivoreState::Seeking);
					return ai::Node::State::Success;
				}
			)
		);
		forage->AddChild(seekTo);
		forage->AddChild(
			action(
				"BeginEat",
				[this](ai::AIContext &)
				{
					if (!m_claimed || !m_claimed->Available())
					{
						ReleaseClaim();
						return ai::Node::State::Failure;
					}
					const glm::vec3 dir = Flat(m_claimed->Position() - Position());
					if (glm::length(dir) > 1e-3f)
						gameObject->GetTransform().rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
					m_body->velocity.x = m_body->velocity.z = 0.0f;
					m_chewTimer = 0.0f;
					SetState(HerbivoreState::Eating);
					return ai::Node::State::Success;
				}
			)
		);
		forage->AddChild(eatWait);
		forage->AddChild(
			action(
				"FinishEat",
				[this](ai::AIContext &)
				{
					if (m_claimed)
						m_claimed->Bite();
					ReleaseClaim();
					m_hunger = std::max(0.0f, m_hunger - 0.4f);
					SetState(HerbivoreState::Idle);
					return ai::Node::State::Success;
				}
			)
		);

		// Wander: amble to a nearby point and rest a while.
		auto findPoint = tree.AddNode<ai::FindRandomReachablePoint>();
		findPoint->SetName("FindPoint");
		findPoint->m_target = "wanderTarget";
		findPoint->m_radius = 35.0f;

		auto wanderTo = tree.AddNode<ai::MoveTo>();
		wanderTo->SetName("WanderTo");
		wanderTo->m_target = "wanderTarget";
		wanderTo->m_speed = m_speed * 0.25f;

		auto restWait = tree.AddNode<ai::Wait>();
		restWait->SetName("RestWait");
		restWait->m_duration = 4.0f;
		restWait->m_deviation = 2.5f;

		auto wander = tree.AddNode<ai::MemSequence>();
		wander->SetName("WanderSequence");
		wander->AddChild(findPoint);
		wander->AddChild(action("StartWander", [this](ai::AIContext &) { SetState(HerbivoreState::Wandering); return ai::Node::State::Success; }));
		wander->AddChild(wanderTo);
		wander->AddChild(action("StartRest", [this](ai::AIContext &) { SetState(HerbivoreState::Idle); return ai::Node::State::Success; }));
		wander->AddChild(restWait);

		priority->AddChild(reactSequence);
		priority->AddChild(restSequence);
		priority->AddChild(forage);
		priority->AddChild(wander);
		root->SetChild(priority);
		m_behaviorTree.SetRoot(root);
	}

	void HerbivoreAI::UpdateAudio(float dt)
	{
		audio::AudioEngine *audio = gameObject->GetWorld().audio;
		if (!audio)
			return;
		const glm::vec3 pos = gameObject->GetTransform().position;

		if (m_stateEnum == HerbivoreState::Eating)
		{
			m_chewTimer -= dt;
			if (m_chewTimer <= 0.0f)
			{
				m_chewTimer = 0.9f;
				audio->PlayOneShot("chew", pos, Large() ? 0.8f : 0.5f, (Large() ? 0.75f : 1.2f) + 0.1f * Rand01());
			}
		}

		// Occasional contact call while calm.
		if (m_stateEnum == HerbivoreState::Idle || m_stateEnum == HerbivoreState::Wandering)
		{
			m_callTimer -= dt;
			if (m_callTimer <= 0.0f)
			{
				m_callTimer = 20.0f + 25.0f * Rand01();
				audio->PlayOneShot(Large() ? "triceratops_bellow" : "protoceratops_chirp", pos, Large() ? 0.55f : 0.5f, 0.9f + 0.2f * Rand01());
			}
		}

		const float speed = glm::length(Flat(m_body->velocity));
		if (speed < 0.3f || !m_body->grounded)
			return;
		m_stepDistance += speed * dt;
		const bool running = speed > m_speed * 0.6f;
		const float perStep = (Large() ? 2.6f : 1.0f) * (running ? 0.7f : 1.0f);
		if (m_stepDistance >= perStep)
		{
			m_stepDistance = 0.0f;
			audio->PlayOneShot("footstep_" + std::to_string(std::rand() % 4), pos, Large() ? 1.3f : 0.35f, Large() ? 0.5f : 1.1f);
		}
	}

	void HerbivoreAI::UpdateAnimation(float dt)
	{
		if (!m_animator)
			return;

		// The pose hook reads blend as 0 idle .. 1 run, and 0 .. -1 for grazing.
		float target = 0.0f, speed = 1.0f;
		switch (m_stateEnum)
		{
		case HerbivoreState::Wandering:
			target = 0.3f;
			speed = 0.8f;
			break;
		case HerbivoreState::Seeking:
			target = 0.35f;
			speed = 0.9f;
			break;
		case HerbivoreState::Eating:
		case HerbivoreState::Sleeping:
			target = -1.0f;
			break;
		case HerbivoreState::Fleeing:
			target = 1.0f;
			speed = 1.8f;
			break;
		case HerbivoreState::Defending:
			target = 0.8f;
			speed = 1.5f;
			break;
		case HerbivoreState::Dead:
			speed = 0.0f;
			break;
		default:
			break;
		}
		const float step = 3.0f * dt;
		m_poseBlend += glm::clamp(target - m_poseBlend, -step, step);
		m_animator->blend = m_poseBlend;
		m_animator->speed = speed;
	}

	void HerbivoreAI::SetState(HerbivoreState state)
	{
		if (m_stateEnum == state)
			return;
		m_stateEnum = state;
		OnStateChanged(state);
	}

	void HerbivoreAI::OnStateChanged(HerbivoreState state)
	{
		auto audio = gameObject->GetWorld().audio;
		if (!audio)
			return;
		const glm::vec3 pos = gameObject->GetTransform().position;

		switch (state)
		{
		case HerbivoreState::Fleeing:
			audio->PlayOneShot(Large() ? "triceratops_bellow" : "protoceratops_chirp", pos, 1.0f, 1.0f + 0.15f * Rand01());
			break;
		case HerbivoreState::Defending:
			audio->PlayOneShot("triceratops_bellow", pos, 1.2f, 0.9f);
			break;
		default:
			break;
		}
	}

} // namespace game
