#pragma once

#include "Node.h"
#include <eng/PathFinder.h>

namespace ai
{

	class Decorator : public Node
	{
	public:
		virtual ~Decorator() = default;

		void SetChild(Node *node) { m_child = node; }

		Node *GetChild() const { return m_child; }

	private:
		Node *m_child = nullptr;
	};

	class CompositeNode : public Node
	{
	public:
		virtual ~CompositeNode() = default;

		void AddChild(Node *child) { children.push_back(child); }

		std::vector<Node *> &GetChildren() { return children; }

	private:
		std::vector<Node *> children;
	};

	class Root : public Decorator
	{
	public:
		Node::State OnExecute(AIContext &ctx) override;
	};

	class Repeat : public Decorator
	{
	public:
		virtual ~Repeat() = default;

		Node::State OnExecute(AIContext &ctx) override;
	};

	class Sequence : public CompositeNode
	{
	public:
		virtual ~Sequence() = default;

		Node::State OnExecute(AIContext &ctx) override;
	};

	class Selector : public CompositeNode
	{
	public:
		virtual ~Selector() = default;

		Node::State OnExecute(AIContext &ctx) override;
	};

	class Inverter : public Node
	{
	public:
		virtual ~Inverter() = default;

		Node::State OnExecute(AIContext &ctx) override;

		void SetChild(Node *node) { child = node; }

	private:
		Node *child = nullptr;
	};

	using ConditionFunc = std::function<bool(AIContext &)>;
	using ActionFunc = std::function<Node::State(AIContext &)>;

	class Conditional : public Node
	{
	public:
		virtual ~Conditional() = default;

		Node::State OnExecute(AIContext &ctx) override;

		void SetCondition(ConditionFunc func) { condition = std::move(func); }

	private:
		ConditionFunc condition;
	};

	class Action : public Node
	{
	public:
		virtual ~Action() = default;

		Node::State OnExecute(AIContext &ctx) override;

		void SetAction(ActionFunc func) { action = std::move(func); }

	private:
		ActionFunc action;
	};

	struct MoveTo : public ai::Node
	{
		std::string m_target;
		float m_speed = 0.0f;
		float m_stopDistance = 1.0f;

		void OnStart(ai::AIContext &ctx) override;

		ai::Node::State OnExecute(ai::AIContext &ctx) override;

	private:
<<<<<<< HEAD
		eng::PathFinder::Path m_path;
		int m_currentNodeIndex = -1;
=======
		std::deque<eng::PathFinder::Node> m_path;
		eng::PathFinder::Node *m_currentNode = nullptr;
>>>>>>> f68a28ca91c449c2a958af4570e2161e14e30d3a
	};

	struct FindRandomReachablePoint : public ai::Node
	{
		std::string m_target;

		ai::Node::State OnExecute(ai::AIContext &ctx) override;
	};
} // namespace ai