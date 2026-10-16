#pragma once

#include "Blackboard.h"
#include "Node.h"

namespace ai
{
	// A behavior tree that manages AI nodes and executes them based on the AI context.
	class BehaviorTree
	{
	public:
		void Tick(AIContext &ctx);

		void SetRoot(Node *node);

		template <typename T>
		T *AddNode()
		{
			std::unique_ptr<T> node = std::make_unique<T>();
			nodes.push_back(std::move(node));
			return static_cast<T *>(nodes.back().get());
		}

	private:
		std::vector<std::unique_ptr<Node>> nodes;

		NodeStatus status = NodeStatus::Running;

		Node *root = nullptr;
	};
} // namespace ai