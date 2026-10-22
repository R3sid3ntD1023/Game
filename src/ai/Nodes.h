#pragma once

#include "EnvQuery.h"
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

		void Abort() override
		{
			Node::Abort();
			if (m_child)
				m_child->Abort();
		}

	private:
		Node *m_child = nullptr;
	};

	class CompositeNode : public Node
	{
	public:
		virtual ~CompositeNode() = default;

		void AddChild(Node *child) { m_children.push_back(child); }

		std::vector<Node *> &GetChildren() { return m_children; }

		void Abort() override
		{
			Node::Abort();
			for (Node *child : m_children)
				child->Abort();
		}

	private:
		std::vector<Node *> m_children;
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

	// Runs children in order and remembers the one that is still Running, so later children are not re-evaluated
	// every tick (unlike Sequence, which restarts from the first child each tick). Aborting resets it.
	class MemSequence : public CompositeNode
	{
	public:
		virtual ~MemSequence() = default;

		void OnStart(AIContext &) override { m_index = 0; }
		Node::State OnExecute(AIContext &ctx) override;
		void Abort() override
		{
			CompositeNode::Abort();
			m_index = 0;
		}

	private:
		size_t m_index = 0;
	};

	// Picks the first child that does not fail. A child that was Running and loses to a higher-priority one is aborted.
	class Selector : public CompositeNode
	{
	public:
		virtual ~Selector() = default;

		Node::State OnExecute(AIContext &ctx) override;
		void Abort() override
		{
			CompositeNode::Abort();
			m_active = kNone;
		}

	private:
		static constexpr size_t kNone = static_cast<size_t>(-1);
		size_t m_active = kNone;
	};

	// Waits for duration +/- deviation seconds (standing still), then succeeds.
	class Wait : public Node
	{
	public:
		float m_duration = 1.0f;
		float m_deviation = 0.0f;

		void OnStart(AIContext &ctx) override;
		Node::State OnExecute(AIContext &ctx) override;

	private:
		float m_remaining = 0.0f;
	};

	// Runs an EnvQuery and writes the winning location (and optional data pointer) to the blackboard.
	// The keys must already exist on the blackboard. Fails when the query finds nothing.
	class RunEnvQuery : public Node
	{
	public:
		EnvQuery query;
		std::string m_locationKey;	 // glm::vec3
		std::string m_dataKey;		 // void*, optional
		bool m_skipIfSet = false;	 // keep a previous result instead of querying again
		float m_failCooldown = 0.0f; // after a failed query, fail fast for this many seconds instead of re-running it

		Node::State OnExecute(AIContext &ctx) override;

	private:
		float m_cooldown = 0.0f;
	};

	class Inverter : public Node
	{
	public:
		virtual ~Inverter() = default;

		Node::State OnExecute(AIContext &ctx) override;

		void SetChild(Node *node) { child = node; }

		void Abort() override
		{
			Node::Abort();
			if (child)
				child->Abort();
		}

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
		float m_repathInterval = 0.5f;
		float m_repathTargetDistance = 2.0f;
		float m_failCooldown = 1.0f;

		void OnStart(ai::AIContext &ctx) override;

		ai::Node::State OnExecute(ai::AIContext &ctx) override;

	private:
		eng::PathFinder::Path m_path;
		int m_currentNodeIndex = -1;

		bool Repath(ai::AIContext &ctx);
		glm::vec3 m_targetPosition{0.0f};
		float m_sinceLastRepath = 0.0f;
		float m_cooldown = 0.0f;
		glm::vec3 m_progressPos{0.0f};
		float m_stuckTime = 0.0f;
	};

	struct FindRandomReachablePoint : public ai::Node
	{
		std::string m_target;
		float m_radius = 100.0f;

		ai::Node::State OnExecute(ai::AIContext &ctx) override;
	};
} // namespace ai