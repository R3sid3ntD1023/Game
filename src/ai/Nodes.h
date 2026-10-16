#pragma once

#include "Node.h"

namespace ai
{
    class CompositeNode : public Node
    {
    public:
        virtual ~CompositeNode() = default;

        void AddChild(Node *child)
        {
            children.push_back(child);
        }

    protected:
        std::vector<Node *> &GetChildren()
        {
            return children;
        }

    private:
        std::vector<Node *> children;
    };

    class Sequence : public CompositeNode
    {
    public:
        virtual ~Sequence() = default;

        NodeStatus Execute() override;
    };

    class Selector : public CompositeNode
    {
    public:
        virtual ~Selector() = default;

        NodeStatus Execute() override;
    };

    class Inverter : public Node
    {
    public:
        virtual ~Inverter() = default;

        NodeStatus Execute() override;

        void SetChild(Node *node)
        {
            child = node;
        }

    private:
        Node *child = nullptr;
    };

    class Conditional : public Node
    {
    public:
        virtual ~Conditional() = default;

        NodeStatus Execute() override;

        void SetCondition(std::function<bool()> func)
        {
            condition = func;
        }

    private:
        std::function<bool()> condition;
    };

    class Action : public Node
    {
    public:
        virtual ~Action() = default;

        NodeStatus Execute() override;

        void SetAction(std::function<NodeStatus()> func)
        {
            action = func;
        }

    private:
        std::function<NodeStatus()> action;
    };
}