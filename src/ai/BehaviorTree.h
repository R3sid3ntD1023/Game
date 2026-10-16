#pragma once

#include "Blackboard.h"
#include "Node.h"

namespace ai
{
    class BehaviorTree
    {
    public:
        void Tick();

        void SetRoot(Node *node);

        template <typename T>
        T *AddNode()
        {
            std::unique_ptr<T> node = std::make_unique<T>();
            nodes.push_back(std::move(node));
            return static_cast<T *>(nodes.back().get());
        }

    private:
        Node *root = nullptr;

        std::vector<std::unique_ptr<Node>> nodes;

        NodeStatus status = NodeStatus::Running;
    };
}