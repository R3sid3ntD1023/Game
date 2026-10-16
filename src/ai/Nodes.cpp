#include "Nodes.h"

namespace ai
{
	NodeStatus Sequence::Execute(AIContext &ctx)
	{
		for (Node *child : GetChildren())
		{
			NodeStatus status = child->Execute(ctx);
			if (status != NodeStatus::Success)
				return status;
		}
		return NodeStatus::Success;
	}

	NodeStatus Selector::Execute(AIContext &ctx)
	{
		for (Node *child : GetChildren())
		{
			NodeStatus status = child->Execute(ctx);
			if (status != NodeStatus::Failure)
				return status;
		}
		return NodeStatus::Failure;
	}

	NodeStatus Inverter::Execute(AIContext &ctx)
	{
		if (child)
		{
			NodeStatus status = child->Execute(ctx);
			if (status == NodeStatus::Success)
				return NodeStatus::Failure;
			if (status == NodeStatus::Failure)
				return NodeStatus::Success;
			return status;
		}
		return NodeStatus::Failure;
	}

	NodeStatus Conditional::Execute(AIContext &ctx)
	{
		if (condition)
			return condition(ctx) ? NodeStatus::Success : NodeStatus::Failure;
		return NodeStatus::Failure;
	}

	NodeStatus Action::Execute(AIContext &ctx)
	{
		if (action)
			return action(ctx);
		return NodeStatus::Failure;
	}

} // namespace ai