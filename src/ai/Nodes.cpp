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
		auto &transform = ctx.self->GetTransform();
		auto target = ctx.blackboard.Get<glm::vec3>(m_target);
		auto &world = ctx.self->GetWorld();
		auto query = world.Query();
		auto &m_grid = world.NavigationGrid();

		if (!m_path.empty())
		{
			m_currentNodeIndex = -1;
			m_path.clear();
		}

		if (query && m_path.empty())
		{
			eng::PathFinder pathFinder(world.NavigationGrid());
			auto start = m_grid.WorldToGrid(transform.position);
			auto end = m_grid.WorldToGrid(target);

			m_path = pathFinder.FindPath(start, end);
			if (!m_path.empty())
			{
				m_currentNodeIndex = 0;
			}

			for (uint32_t i = 0; i < m_path.size(); ++i)
			{
				if (i + 1 >= m_path.size())
					break;

				auto gridPos = m_path[i];
				auto nextGridPos = m_path[i + 1];
				auto worldPos = m_grid.GridToWorld(m_path[i].x, m_path[i].y);
				auto nextWorldPos = m_grid.GridToWorld(nextGridPos.x, nextGridPos.y);
				gfx::Line line(worldPos, nextWorldPos, glm::vec3(1, 0, 0));
				world.Lines().DrawPersistentLine({[line](gfx::LineRenderer &lr) { lr.DrawLine(line); }, 100.f});
			}
		}
	}

	ai::Node::State MoveTo::OnExecute(ai::AIContext &ctx)
	{
		auto target = ctx.blackboard.Get<glm::vec3>(m_target);
		auto body = ctx.self->GetComponent<eng::RigidBody>();
		auto &transform = ctx.self->GetTransform();
		auto &world = ctx.self->GetWorld();

		auto &grid = world.NavigationGrid();

		glm::vec3 moveTarget = target;
		float waypointRadius = 0.5f; // Example radius for the waypoint

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

		dir /= distance;

		glm::vec3 nextPosition = ctx.self->GetTransform().position + dir * m_speed;

		if (!world.IsWalkable(nextPosition.x, nextPosition.z))
		{
			body->velocity.x = 0.0f;
			body->velocity.z = 0.0f;
			spdlog::info("Blocked");
			ctx.blackboard.Clear(m_target);
			return ai::Node::State::Failure;
		}

		body->velocity.x = dir.x * m_speed;
		body->velocity.z = dir.z * m_speed;

		transform.rotation = glm::angleAxis(std::atan2(-dir.z, dir.x), glm::vec3(0, 1, 0));

		return ai::Node::State::Running;
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