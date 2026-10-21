#include "Nodes.h"
#include <eng/GameObject.h>
#include <eng/RigidBody.h>
#include <eng/World.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace ai
{
	Node::State Sequence::OnExecute(AIContext &ctx)
	{
		int currentChild = 0;
		for (Node *child : GetChildren())
		{
			Node::State status = child->Execute(ctx);
			switch (status)
			{
			case Node::State::Success:
				currentChild++;
				continue;
			case Node::State::Failure:
				return Node::State::Failure;
			case Node::State::Running:
				return Node::State::Running;
			}
		}
		return currentChild == GetChildren().size() ? Node::State::Success : Node::State::Running;
	}

	Node::State Selector::OnExecute(AIContext &ctx)
	{
		for (Node *child : GetChildren())
		{
			Node::State status = child->Execute(ctx);
			switch (status)
			{
			case Node::State::Success:
				return Node::State::Success;
			case Node::State::Failure:
				continue;
			case Node::State::Running:
				return Node::State::Running;
			}
		}
		return Node::State::Failure;
	}

	Node::State Inverter::OnExecute(AIContext &ctx)
	{
		if (child)
		{
			Node::State status = child->Execute(ctx);
			if (status == Node::State::Success)
				return Node::State::Failure;
			if (status == Node::State::Failure)
				return Node::State::Success;
			return status;
		}
		return Node::State::Failure;
	}

	Node::State Conditional::OnExecute(AIContext &ctx)
	{
		if (condition)
			return condition(ctx) ? Node::State::Success : Node::State::Failure;
		return Node::State::Failure;
	}

	Node::State Action::OnExecute(AIContext &ctx)
	{
		if (action)
			return action(ctx);
		return Node::State::Failure;
	}

	void MoveTo::OnStart(ai::AIContext &ctx)
	{
		m_path.clear();
		m_currentNodeIndex = -1;
		m_sinceLastRepath = 0.0f;
		m_cooldown = 0.0f;
		m_stuckTime = 0.0f;
		m_progressPos = ctx.self->GetTransform().position;
		Repath(ctx);
	}

	ai::Node::State MoveTo::OnExecute(ai::AIContext &ctx)
	{
		m_sinceLastRepath += ctx.deltaTime;
		m_cooldown = std::max(0.0f, m_cooldown - ctx.deltaTime);

		if (!ctx.blackboard.IsSet(m_target))
		{
			m_path.clear();
			m_currentNodeIndex = -1;
			if (auto *body = ctx.self->GetComponent<eng::RigidBody>())
				body->velocity.x = body->velocity.z = 0.0f;
			return ai::Node::State::Failure;
		}

		auto target = ctx.blackboard.Get<glm::vec3>(m_target);
		auto body = ctx.self->GetComponent<eng::RigidBody>();
		auto &transform = ctx.self->GetTransform();
		auto &world = ctx.self->GetWorld();
		auto &grid = world.NavigationGrid();

		// Give up when wedged against geometry (e.g. a tree corner) so the tree can pick a new goal.
		m_stuckTime += ctx.deltaTime;
		if (glm::distance(transform.position, m_progressPos) > 0.5f)
		{
			m_progressPos = transform.position;
			m_stuckTime = 0.0f;
		}
		else if (m_stuckTime > 2.0f)
		{
			body->velocity.x = body->velocity.z = 0.0f;
			ctx.blackboard.Clear(m_target);
			m_path.clear();
			m_currentNodeIndex = -1;
			return ai::Node::State::Failure;
		}

		bool targetMoved = glm::distance(target, m_targetPosition) > m_repathTargetDistance;
		bool stale = m_sinceLastRepath > m_repathInterval;

		// Only repath for a moved target; periodic repaths of a static target just burn CPU.
		if (m_cooldown <= 0.0f && (m_path.empty() || (stale && targetMoved)))
		{
			if (!Repath(ctx) && m_path.empty())
				return ai::Node::State::Failure;
		}

		glm::vec3 moveTarget = target;
		float waypointRadius = 0.5f; // Example radius for the waypoint

		for (size_t i = std::max(m_currentNodeIndex, 0); i + 1 < m_path.size(); ++i)
		{
			const auto a = grid.GetCell(m_path[i].x, m_path[i].y).position;
			const auto b = grid.GetCell(m_path[i + 1].x, m_path[i + 1].y).position;
			world.Lines().QueueLine({a, b, glm::vec3(1.0f, 0.5f, 0.0f)});
		}

		if (m_currentNodeIndex >= 0)
		{
			const auto &gridPos = m_path[m_currentNodeIndex];
			moveTarget = grid.GridToWorld(gridPos.x, gridPos.y);
			waypointRadius = grid.cellSize * 0.5f;
		}

		auto dir = moveTarget - transform.position;
		dir.y = 0.0f;

		float distance = glm::length(dir);

		if (distance < waypointRadius + m_stopDistance)
		{
			++m_currentNodeIndex;

			if (m_currentNodeIndex >= int(m_path.size()))
			{
				body->velocity.x = 0.0f;
				body->velocity.z = 0.0f;

				ctx.blackboard.Clear(m_target);
				m_currentNodeIndex = -1;
				m_path.clear();
				return ai::Node::State::Success;
			}

			const auto &nextGridPos = m_path[m_currentNodeIndex];
			moveTarget = grid.GridToWorld(nextGridPos.x, nextGridPos.y);
			waypointRadius = grid.cellSize * 0.5f;

			dir = moveTarget - transform.position;
			dir.y = 0.0f;
			distance = glm::length(dir);
		}

		if (distance < 1e-4f)
			return ai::Node::State::Running;

		dir /= distance;

		glm::vec3 nextPosition = ctx.self->GetTransform().position + dir * m_speed;
		auto gridPos = grid.WorldToGrid(nextPosition);

		if (!grid.IsWalkable(gridPos.x, gridPos.y))
		{
			body->velocity.x = body->velocity.z = 0.0f;

			if (m_cooldown <= 0.0f && Repath(ctx))
			{
				return ai::Node::State::Running;
			}

			ctx.blackboard.Clear(m_target);
			m_path.clear();
			m_currentNodeIndex = -1;
			return ai::Node::State::Failure;
		}

		body->velocity.x = dir.x * m_speed;
		body->velocity.z = dir.z * m_speed;

		transform.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));

		return ai::Node::State::Running;
	}

	bool MoveTo::Repath(ai::AIContext &ctx)
	{
		auto &world = ctx.self->GetWorld();
		auto &grid = world.NavigationGrid();
		if (!ctx.blackboard.IsSet(m_target))
			return false;

		auto target = ctx.blackboard.Get<glm::vec3>(m_target);

		eng::PathFinder pf(grid);
		auto path = pf.FindPath(grid.WorldToGrid(ctx.self->GetTransform().position), grid.WorldToGrid(target));

		m_sinceLastRepath = 0.0f;
		m_targetPosition = target;
		if (path.empty())
		{
			m_cooldown = m_failCooldown;
			return false;
		}

		m_path = std::move(path);
		m_currentNodeIndex = 0;
		m_cooldown = m_repathInterval; // successful repaths are rate-limited too, otherwise a blocked step repaths every frame
		return true;
	}

	ai::Node::State FindRandomReachablePoint::OnExecute(ai::AIContext &ctx)
	{
		auto &world = ctx.self->GetWorld();
		glm::vec3 point;
		if (world.FindRandomReachablePoint(point, ctx.self->GetTransform().position, 100.0f))
		{
			// spdlog::info("Found random reachable point: ({}, {}, {})", point.x, point.y, point.z);
			ctx.blackboard.Set<glm::vec3>(m_target, point);
		}
		else
			return ai::Node::State::Failure;

		return ai::Node::State::Success;
	}

	Node::State Repeat::OnExecute(AIContext &ctx)
	{
		auto child = GetChild();
		if (child)
		{
			child->Execute(ctx);
			return Node::State::Running;
		}
		return Node::State::Failure;
	}

	Node::State Root::OnExecute(AIContext &ctx)
	{
		auto child = GetChild();
		if (child)
			return child->Execute(ctx);
		return Node::State::Failure;
	}

} // namespace ai