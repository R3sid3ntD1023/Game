#include "Nodes.h"

namespace ai
{
    NodeStatus Sequence::Execute()
    {
        for (Node *child : GetChildren())
        {
            NodeStatus status = child->Execute();
            if (status != NodeStatus::Success)
                return status;
        }
        return NodeStatus::Success;
    }

    NodeStatus Selector::Execute()
    {
        for (Node *child : GetChildren())
        {
            NodeStatus status = child->Execute();
            if (status != NodeStatus::Failure)
                return status;
        }
        return NodeStatus::Failure;
    }

    NodeStatus Inverter::Execute()
    {
        if (child)
        {
            NodeStatus status = child->Execute();
            if (status == NodeStatus::Success)
                return NodeStatus::Failure;
            if (status == NodeStatus::Failure)
                return NodeStatus::Success;
            return status;
        }
        return NodeStatus::Failure;
    }

    NodeStatus Conditional::Execute()
    {
        if (condition)
            return condition() ? NodeStatus::Success : NodeStatus::Failure;
        return NodeStatus::Failure;
    }

    NodeStatus Action::Execute()
    {
        if (action)
            return action();
        return NodeStatus::Failure;
    }
}