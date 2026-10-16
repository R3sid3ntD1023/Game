#pragma once

#include "Node.h"

namespace ai
{
	class CompositeNode : public Node
	{
	public:
		virtual ~CompositeNode() = default;

		void AddChild(Node *child) { children.push_back(child); }

	protected:
		std::vector<Node *> &GetChildren() { return children; }

	private:
		std::vector<Node *> children;
	};

	class Sequence : public CompositeNode
	{
	public:
		virtual ~Sequence() = default;

		NodeStatus Execute(AIContext &ctx) override;
	};

	class Selector : public CompositeNode
	{
	public:
		virtual ~Selector() = default;

		NodeStatus Execute(AIContext &ctx) override;
	};

	class Inverter : public Node
	{
	public:
		virtual ~Inverter() = default;

		NodeStatus Execute(AIContext &ctx) override;

		void SetChild(Node *node) { child = node; }

	private:
		Node *child = nullptr;
	};

	using ConditionFunc = std::function<bool(AIContext &)>;
	using ActionFunc = std::function<NodeStatus(AIContext &)>;

	class Conditional : public Node
	{
	public:
		virtual ~Conditional() = default;

		NodeStatus Execute(AIContext &ctx) override;

		void SetCondition(ConditionFunc func) { condition = std::move(func); }

	private:
		ConditionFunc condition;
	};

	class Action : public Node
	{
	public:
		virtual ~Action() = default;

		NodeStatus Execute(AIContext &ctx) override;

		void SetAction(ActionFunc func) { action = std::move(func); }

	private:
		ActionFunc action;
	};
} // namespace ai