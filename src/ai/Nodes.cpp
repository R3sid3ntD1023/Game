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

		if (query)
		{
			eng::PathFinder m_pathFinder(world.NavigationGrid(), *query);
			auto start = m_grid.WorldToGrid(transform.position);
			auto end = m_grid.WorldToGrid(target);

			m_path = m_pathFinder.FindPath(start, end);
			if (!m_path.empty())
				m_currentNode = &m_path.front();
		}
	}

	ai::Node::State MoveTo::OnExecute(ai::AIContext &ctx)
	{
		auto target = ctx.blackboard.Get<glm::vec3>(m_target);
		auto body = ctx.self->GetComponent<eng::RigidBody>();
		auto &transform = ctx.self->GetTransform();
		auto &world = ctx.self->GetWorld();

		if (m_currentNode && m_currentNode->parent)
		{
			m_currentNode = m_currentNode->parent;

			auto worldPos = world.NavigationGrid().GridToWorld(m_currentNode->gridPos.x, m_currentNode->gridPos.y);
		}

		auto dir = target - transform.position;
		dir.y = 0.0f;

		float distance = glm::length(dir);
		// spdlog::info("Distance to target: {}", distance);

		// Implement movement logic here
		if (distance < m_stopDistance + body->halfExtents.x)
		{
			body->velocity.x = 0.0f;
			body->velocity.z = 0.0f;
			ctx.blackboard.Clear(m_target);
			m_currentNode = nullptr;
			m_path.clear();
			return ai::Node::State::Success;
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