#pragma once

#include "BehaviorTree.h"

namespace ai
{
	void BehaviorTree::Tick(AIContext &ctx)
	{
		if (root && status == NodeStatus::Running)
		{
			status = root->Execute(ctx);
		}
	}

	void BehaviorTree::SetRoot(Node *node)
	{
		root = node;
	}
} // namespace ai