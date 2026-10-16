#pragma once

#include "BehaviorTree.h"

namespace ai
{
    void BehaviorTree::Tick()
    {
        if (root && status == NodeStatus::Running)
        {
            status = root->Execute();
        }
    }

    void BehaviorTree::SetRoot(Node *node)
    {
        root = node;
    }
}