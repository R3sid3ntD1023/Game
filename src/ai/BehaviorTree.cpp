#pragma once

#include "BehaviorTree.h"
#include "Nodes.h"

namespace ai
{
	ai::Node::State BehaviorTree::Tick(AIContext &ctx)
	{
		if (!root)
		{
			status = Node::State::Failure;
			return status;
		}

		status = root->Execute(ctx);

<<<<<<< HEAD
		if (status == Node::State::Failure || status == Node::State::Success)
		{
			root->Reset();
=======
		if (status == Node::State::Running)
		{
			return Node::State::Running;
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
		}

		return status;
	}

	void BehaviorTree::SetRoot(Node *node)
	{
		root = node;
	}

	void BehaviorTree::Print(Node *node, int indent)
	{
		if (!node)
			return;

		LOG_INFO("{:>{}}Node: {}", "", indent, node->GetName());

		if (auto composite = dynamic_cast<CompositeNode *>(node))
		{
			for (auto &child : composite->GetChildren())
			{
				Print(child, indent + 2);
			}
		}

		if (auto decorator = dynamic_cast<Decorator *>(node))
		{
			Print(decorator->GetChild(), indent + 2);
		}
	}
} // namespace ai