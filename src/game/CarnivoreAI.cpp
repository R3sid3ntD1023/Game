#include "game/CarnivoreAI.h"
#include "ai/Nodes.h"
#include "eng/World.h"
#include "game/DinosaurAI.h"
#include "game/HerbivoreAI.h"
#include "game/Survival.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace game
{

	namespace
	{
		float Rand01() { return static_cast<float>(std::rand() % 1001) / 1000.0f; }

		void FaceToward(Transform &self, glm::vec3 dir)
		{
			dir.y = 0.0f;
			if (glm::dot(dir, dir) > 1e-6f)
				self.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));
		}
	} // namespace

	CarnivoreAI::CarnivoreAI(eng::GameObject &target, GameState &state, float speed, float damage)
		: m_target(target),
		  m_state(state),
		  m_ecology(state.ecology),
		  m_speed(speed),
		  m_damage(damage)
	{
	}

	CarnivoreAI::~CarnivoreAI()
	{
		if (m_registered)
			Ecology::Remove(m_ecology->carnivores, this);
	}

	void CarnivoreAI::Hurt(float amount, const glm::vec3 &from)
	{
		if (Dead())
			return;
		m_animalDamage = true;
		m_pendingAttacker = from;
		m_health->Damage(amount);
		m_animalDamage = false;
	}

	void CarnivoreAI::OnStart()
	{
		m_body = gameObject->GetComponent<eng::RigidBody>();
		m_health = gameObject->GetComponent<eng::Health>();
		m_targetHealth = m_target.GetComponent<eng::Health>();
		m_animator = gameObject->GetComponent<eng::Animator>();

		m_ecology->carnivores.push_back(this);
		m_registered = true;

		m_perception.config.sightRange = 55.0f;
		m_perception.config.closeRange = 9.0f;
		m_perception.config.touchRadius = 3.0f;
		m_perception.config.memory = 12.0f;

		m_health->SetOnDamageCallback(
			[this](float amount)
			{
				if (m_health->Dead())
				{
					SetState(CarnivoreState::Dead);
					m_body->velocity = glm::vec3(0.0f);
					if (!m_animalDamage)
						++m_state.kills;
					gameObject->GetWorld().EmitBlood(gameObject->GetTransform().position, glm::vec3(0.0f, 1.0f, 0.0f), 120);
					gameObject->Destroy();
					spdlog::info("T-Rex down ({} kills)", m_state.kills);
					return;
				}
				m_hurtTimer = kRecentlyHurtSeconds;
				if (m_animalDamage)
				{
					m_attackerPos = m_pendingAttacker;
				}
				else
				{
					m_attackerPos = m_target.GetTransform().position;
					m_perception.OnDamaged(amount, m_attackerPos);
				}
			}
		);

		InitializeBehaviorTree();

		m_awakeSenses = m_perception.config;
		m_awakeTimer = static_cast<float>(std::rand() % 4000) / 100.0f;
		m_awakeLimit = 70.0f + static_cast<float>(std::rand() % 6000) / 100.0f;
	}

	void CarnivoreAI::OnUpdate(float dt)
	{
		if (!m_body || Dead())
			return;

		m_attackTimer = std::max(0.0f, m_attackTimer - dt);
		m_biteTimer = std::max(0.0f, m_biteTimer - dt);
		m_hurtTimer = std::max(0.0f, m_hurtTimer - dt);

		m_perception.Update(dt, *gameObject, m_target);
		if (gameObject->GetWorld().debugPerception)
			m_perception.DrawDebug(*gameObject, m_target.GetTransform().position);

		UpdateFlee(dt);
		UpdateRest(dt);
		UpdatePrey(dt);
		UpdateAudio(dt);
		UpdateAnimation(dt);

		m_aiContext.deltaTime = dt;
		m_behaviorTree.Tick(m_aiContext);
	}

	void CarnivoreAI::StartSleep()
	{
		m_sleeping = true;
		m_sleepTimer = 25.0f + static_cast<float>(std::rand() % 2000) / 100.0f;
		m_awakeSenses = m_perception.config;
		m_perception.config.sightRange = 9.0f;
		m_perception.config.hearingSensitivity = 0.35f;
		m_perception.config.sightRate *= 0.5f;
	}

	void CarnivoreAI::WakeUp()
	{
		m_sleeping = false;
		m_awakeTimer = 0.0f;
		m_awakeLimit = 70.0f + static_cast<float>(std::rand() % 6000) / 100.0f;
		m_perception.config = m_awakeSenses;
	}

	// A threat is too great when the T-Rex is being attacked and is either badly hurt or mobbed by several triceratops.
	void CarnivoreAI::UpdateFlee(float dt)
	{
		const float fraction = HealthFraction();
		const int herd = m_ecology->CountLargeHerbivores(Position(), 16.0f) + m_ecology->CountRaptorsNear(Position(), 16.0f, 0.0f);
		const bool attacked = m_hurtTimer > 0.0f;
		const bool tooGreat = attacked && (fraction < kFleeHealthFraction || (fraction < 0.7f && herd >= 2));

		if (tooGreat)
			m_fleeTimer = 5.0f;
		else
			m_fleeTimer = std::max(0.0f, m_fleeTimer - dt);

		const bool fleeing = m_fleeTimer > 0.0f;
		if (fleeing && !m_fleeing)
		{
			if (m_sleeping)
				WakeUp();
			m_preyId = 0;
			m_preyCooldown = 30.0f;
		}
		m_fleeing = fleeing;
	}

	void CarnivoreAI::UpdateRest(float dt)
	{
		const bool alerted = m_perception.Alerted();
		const float fraction = HealthFraction();

		if (m_sleeping)
		{
			// Sleep heals.
			if (m_health->current < m_health->max)
				m_health->Heal(m_health->max * kSleepHealPerSecond * dt);
			m_sleepTimer -= dt;
			if (alerted || (m_sleepTimer <= 0.0f && fraction >= kHealedFraction))
				WakeUp();
			return;
		}

		if (alerted || m_fleeing || m_preyId || m_hurtTimer > 0.0f)
			return;
		if (m_stateEnum != CarnivoreState::Wandering && m_stateEnum != CarnivoreState::Idle)
			return;

		m_awakeTimer += dt;
		if (m_awakeTimer > m_awakeLimit || fraction < kRestHealthFraction)
			StartSleep();
	}

	HerbivoreAI *CarnivoreAI::CurrentPrey() const
	{
		if (!m_preyId)
			return nullptr;
		HerbivoreAI *prey = m_ecology->FindHerbivore(m_preyId);
		return prey && !prey->Dead() ? prey : nullptr;
	}

	float CarnivoreAI::PreyDistance(const HerbivoreAI &prey) const
	{
		const glm::vec3 d = prey.Position() - Position();
		return glm::length(glm::vec2(d.x, d.z));
	}

	void CarnivoreAI::UpdatePrey(float dt)
	{
		m_preyCooldown = std::max(0.0f, m_preyCooldown - dt);
		m_preyScanTimer -= dt;

		if (m_preyId)
		{
			m_preyTime += dt;
			if (!CurrentPrey() || m_preyTime > 45.0f || m_perception.Alerted() || m_fleeing)
			{
				m_preyId = 0;
				m_preyTime = 0.0f;
				m_preyCooldown = 15.0f;
				m_aiContext.blackboard.Clear("preyTarget");
			}
			return;
		}

		if (m_sleeping || m_fleeing || m_perception.Alerted() || m_preyCooldown > 0.0f || m_preyScanTimer > 0.0f || HealthFraction() < kRestHealthFraction)
			return;
		m_preyScanTimer = 0.8f + 0.4f * Rand01();

		HerbivoreAI *best = nullptr;
		float bestDist = 60.0f;
		for (HerbivoreAI *h : m_ecology->herbivores)
		{
			if (h->Dead())
				continue;
			const float d = PreyDistance(*h);
			if (d < bestDist)
			{
				bestDist = d;
				best = h;
			}
		}
		if (best)
		{
			m_preyId = best->Id();
			m_preyTime = 0.0f;
		}
	}

	void CarnivoreAI::InitializeBehaviorTree()
	{
		m_aiContext.self = gameObject;
		m_aiContext.blackboard.Add<glm::vec3>("target");
		m_aiContext.blackboard.Add<glm::vec3>("chaseTarget");
		m_aiContext.blackboard.Add<glm::vec3>("preyTarget");
		m_aiContext.blackboard.Add<glm::vec3>("fleeTarget");

		auto &tree = m_behaviorTree;
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
		auto sequence = [&](const char *name, std::initializer_list<ai::Node *> children)
		{
			auto node = tree.AddNode<ai::Sequence>();
			node->SetName(name);
			for (ai::Node *child : children)
				node->AddChild(child);
			return node;
		};
		auto moveTo = [&](const char *name, const char *key, float speed)
		{
			auto node = tree.AddNode<ai::MoveTo>();
			node->SetName(name);
			node->m_target = key;
			node->m_speed = speed;
			return node;
		};

		auto root = tree.AddNode<ai::Root>();
		root->SetName("Root");
		auto priority = tree.AddNode<ai::Selector>();
		priority->SetName("PrioritySelector");

		// Flee: run from whatever is hurting us, far enough to lose it.
		auto fleeTo = moveTo("FleeTo", "fleeTarget", m_speed * 1.1f);
		auto flee = sequence(
			"FleeSequence",
			{condition("IsFleeing", [this](ai::AIContext &) { return m_fleeing; }),
			 action(
				 "Flee",
				 [this, fleeTo](ai::AIContext &ctx)
				 {
					 m_fleeRepick -= ctx.deltaTime;
					 if (m_branch != Branch::Flee || !ctx.blackboard.IsSet("fleeTarget") || m_fleeRepick <= 0.0f)
					 {
						 m_branch = Branch::Flee;
						 m_fleeRepick = 1.5f;
						 ctx.blackboard.Set<glm::vec3>("fleeTarget", PickFleePoint(*gameObject, m_attackerPos, 40.0f));
						 fleeTo->Restart();
					 }
					 SetState(CarnivoreState::Fleeing);
					 return ai::Node::State::Success;
				 }
			 ),
			 fleeTo}
		);

		// Fight back: when raptors are biting it, snap at the nearest one.
		auto retaliate = sequence(
			"RetaliateSequence",
			{condition(
				 "RaptorInReach",
				 [this](ai::AIContext &)
				 {
					 float d = 0.0f;
					 return m_hurtTimer > 0.0f && m_ecology->NearestRaptor(Position(), 5.5f, &d) != nullptr;
				 }
			 ),
			 action(
				 "Retaliate",
				 [this](ai::AIContext &ctx)
				 {
					 DinosaurAI *raptor = m_ecology->NearestRaptor(Position(), 5.5f);
					 if (!raptor)
						 return ai::Node::State::Failure;
					 SetState(CarnivoreState::Attacking);
					 m_branch = Branch::Attack;
					 m_body->velocity.x = m_body->velocity.z = 0.0f;
					 auto &self = ctx.self->GetTransform();
					 glm::vec3 dir = raptor->Position() - self.position;
					 dir.y = 0.0f;
					 if (glm::length(dir) > 1e-3f)
						 dir = glm::normalize(dir);
					 FaceToward(self, dir);
					 if (m_biteTimer <= 0.0f)
					 {
						 m_biteTimer = 1.0f;
						 gameObject->GetWorld().EmitBlood(raptor->Position() + glm::vec3(0.0f, 0.5f, 0.0f), dir, 16);
						 raptor->Hurt(m_damage, &self.position);
					 }
					 return ai::Node::State::Running;
				 }
			 )}
		);

		// Bite the player when in reach.
		auto attack = sequence(
			"AttackSequence",
			{condition(
				 "AttackRange",
				 [this](ai::AIContext &ctx)
				 {
					 glm::vec3 d = m_target.GetTransform().position - ctx.self->GetTransform().position;
					 d.y = 0.0f;
					 return m_perception.Alerted() && glm::length(d) < m_attackRange;
				 }
			 ),
			 action(
				 "Attack",
				 [this](ai::AIContext &ctx)
				 {
					 SetState(CarnivoreState::Attacking);
					 m_branch = Branch::Attack;
					 auto &self = ctx.self->GetTransform();
					 const glm::vec3 target = m_target.GetTransform().position;
					 glm::vec3 dir = target - self.position;
					 dir.y = 0.0f;
					 if (glm::length(dir) > 1e-3f)
						 dir = glm::normalize(dir);
					 m_body->velocity.x = m_body->velocity.z = 0.0f;
					 FaceToward(self, dir);
					 if (m_attackTimer <= 0.0f && m_targetHealth)
					 {
						 m_targetHealth->Damage(m_damage);
						 gameObject->GetWorld().EmitBlood(target + glm::vec3(0.0f, 0.5f, 0.0f), dir, 24);
						 m_attackTimer = 1.6f;
					 }
					 return ai::Node::State::Running;
				 }
			 )}
		);

		// Chase the player's last known position.
		auto chaseTo = moveTo("ChaseTo", "chaseTarget", m_speed);
		auto chase = sequence(
			"ChaseSequence",
			{condition("Alerted", [this](ai::AIContext &) { return m_perception.Alerted(); }),
			 action(
				 "Chase",
				 [this, chaseTo](ai::AIContext &ctx)
				 {
					 if (m_branch != Branch::Chase)
					 {
						 m_branch = Branch::Chase;
						 chaseTo->Restart();
					 }
					 const glm::vec3 goal = m_perception.LastKnownPosition();
					 glm::vec3 toGoal = goal - ctx.self->GetTransform().position;
					 toGoal.y = 0.0f;
					 if (!m_perception.SeesTarget() && glm::length(toGoal) < 3.0f)
					 {
						 m_body->velocity.x = m_body->velocity.z = 0.0f;
						 ctx.blackboard.Clear("chaseTarget");
						 m_perception.NotifyArrived();
						 SetState(CarnivoreState::Idle);
						 return ai::Node::State::Running;
					 }
					 ctx.blackboard.Set<glm::vec3>("chaseTarget", goal);
					 SetState(CarnivoreState::Chasing);
					 return ai::Node::State::Success;
				 }
			 ),
			 chaseTo}
		);

		// Sleep: lie still and heal.
		auto sleep = sequence(
			"SleepSequence",
			{condition("IsSleeping", [this](ai::AIContext &) { return m_sleeping; }),
			 action(
				 "Sleep",
				 [this](ai::AIContext &)
				 {
					 m_branch = Branch::Sleep;
					 m_body->velocity.x = m_body->velocity.z = 0.0f;
					 SetState(CarnivoreState::Sleeping);
					 return ai::Node::State::Running;
				 }
			 )}
		);

		// Prey: bite it when close, otherwise stalk it.
		auto preyBite = sequence(
			"PreyBiteSequence",
			{condition(
				 "PreyInReach",
				 [this](ai::AIContext &)
				 {
					 HerbivoreAI *prey = CurrentPrey();
					 return prey && PreyDistance(*prey) <= prey->Radius() + 3.2f;
				 }
			 ),
			 action(
				 "PreyBite",
				 [this](ai::AIContext &ctx)
				 {
					 HerbivoreAI *prey = CurrentPrey();
					 if (!prey)
						 return ai::Node::State::Failure;
					 SetState(CarnivoreState::Attacking);
					 m_branch = Branch::Prey;
					 m_body->velocity.x = m_body->velocity.z = 0.0f;
					 auto &self = ctx.self->GetTransform();
					 glm::vec3 dir = prey->Position() - self.position;
					 dir.y = 0.0f;
					 if (glm::length(dir) > 1e-3f)
						 dir = glm::normalize(dir);
					 FaceToward(self, dir);
					 if (m_biteTimer <= 0.0f)
					 {
						 m_biteTimer = 1.2f;
						 gameObject->GetWorld().EmitBlood(prey->Position() - dir * prey->Radius() + glm::vec3(0.0f, 0.8f, 0.0f), -dir, 16);
						 prey->Hurt(m_damage);
					 }
					 return ai::Node::State::Running;
				 }
			 )}
		);

		auto preyTo = moveTo("PreyTo", "preyTarget", m_speed * 0.9f);
		auto preyApproach = sequence(
			"PreyApproachSequence",
			{condition("HasPrey", [this](ai::AIContext &) { return CurrentPrey() != nullptr; }),
			 action(
				 "TrackPrey",
				 [this](ai::AIContext &ctx)
				 {
					 HerbivoreAI *prey = CurrentPrey();
					 if (!prey)
						 return ai::Node::State::Failure;
					 m_branch = Branch::Prey;
					 ctx.blackboard.Set<glm::vec3>("preyTarget", prey->Position());
					 SetState(CarnivoreState::Stalking);
					 return ai::Node::State::Success;
				 }
			 ),
			 preyTo}
		);

		// Wander.
		auto wanderTo = moveTo("WanderTo", "target", m_speed * 0.3f);
		auto findPoint = tree.AddNode<ai::FindRandomReachablePoint>();
		findPoint->SetName("FindPoint");
		findPoint->m_target = "target";
		auto findPointSelector = tree.AddNode<ai::Selector>();
		findPointSelector->SetName("FindPointSelector");
		findPointSelector->AddChild(
			sequence("FindPointSequence", {condition("NotHasTarget", [](ai::AIContext &ctx) { return !ctx.blackboard.IsSet("target"); }), findPoint})
		);
		findPointSelector->AddChild(
			sequence(
				"WanderSequence",
				{action(
					 "WanderAction",
					 [this, wanderTo](ai::AIContext &)
					 {
						 if (m_branch != Branch::Wander)
						 {
							 m_branch = Branch::Wander;
							 wanderTo->Restart();
						 }
						 SetState(CarnivoreState::Wandering);
						 return ai::Node::State::Success;
					 }
				 ),
				 wanderTo}
			)
		);

		priority->AddChild(flee);
		priority->AddChild(retaliate);
		priority->AddChild(attack);
		priority->AddChild(chase);
		priority->AddChild(sleep);
		priority->AddChild(preyBite);
		priority->AddChild(preyApproach);
		priority->AddChild(findPointSelector);
		root->SetChild(priority);
		m_behaviorTree.SetRoot(root);
	}

	void CarnivoreAI::UpdateAudio(float dt)
	{
		audio::AudioEngine *audio = gameObject->GetWorld().audio;
		if (!audio || !m_body->grounded)
			return;
		if (m_stateEnum != CarnivoreState::Wandering && m_stateEnum != CarnivoreState::Chasing && m_stateEnum != CarnivoreState::Stalking && m_stateEnum != CarnivoreState::Fleeing)
			return;

		const float speed = glm::length(glm::vec2(m_body->velocity.x, m_body->velocity.z));
		m_stepDistance += speed * dt;
		if (m_stepDistance >= 3.4f)
		{
			m_stepDistance = 0.0f;
			audio->PlayOneShot("footstep_" + std::to_string(std::rand() % 4), gameObject->GetTransform().position, 1.8f, 0.3f);
		}
	}

	void CarnivoreAI::UpdateAnimation(float dt)
	{
		if (!m_animator)
			return;

		// 0 idle .. 1 run, 0 .. -1 lying down.
		float target = 0.0f, speed = 0.7f;
		switch (m_stateEnum)
		{
		case CarnivoreState::Wandering:
			target = 0.3f;
			speed = 0.6f;
			break;
		case CarnivoreState::Stalking:
			target = 0.7f;
			speed = 1.0f;
			break;
		case CarnivoreState::Chasing:
		case CarnivoreState::Fleeing:
			target = 1.0f;
			speed = 1.5f;
			break;
		case CarnivoreState::Sleeping:
			target = -1.0f;
			speed = 0.5f;
			break;
		case CarnivoreState::Attacking:
			speed = 1.0f;
			break;
		case CarnivoreState::Dead:
			speed = 0.0f;
			break;
		default:
			break;
		}
		const float rate = (target < m_poseBlend && target < 0.0f) ? 1.0f : 3.0f;
		const float step = rate * dt;
		m_poseBlend += glm::clamp(target - m_poseBlend, -step, step);
		m_animator->blend = m_poseBlend;
		m_animator->speed = speed;
	}

	void CarnivoreAI::SetState(CarnivoreState state)
	{
		if (m_stateEnum == state)
			return;
		m_stateEnum = state;
		OnStateChanged(state);
	}

	void CarnivoreAI::OnStateChanged(CarnivoreState state)
	{
		auto audio = gameObject->GetWorld().audio;
		if (!audio)
			return;
		const glm::vec3 pos = gameObject->GetTransform().position;
		switch (state)
		{
		case CarnivoreState::Chasing:
			audio->PlayOneShot("raptor_roar", pos, 1.6f, 0.45f + 0.1f * Rand01());
			break;
		case CarnivoreState::Attacking:
			audio->PlayOneShot("raptor_attack", pos, 1.4f, 0.5f);
			break;
		case CarnivoreState::Fleeing:
			audio->PlayOneShot("raptor_roar", pos, 1.2f, 0.6f);
			break;
		default:
			break;
		}
	}

} // namespace game
