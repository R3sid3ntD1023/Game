#pragma once

#include "AIContext.h"
#include "core/Core.h"
#include <spdlog/spdlog.h>

namespace ai
{

	// Base class for all behavior tree nodes. Each node must implement the Execute method.
	class Node
	{
	public:
		enum class State
		{
			Idle,
			Success,
			Failure,
			Running
		};

		static std::string ToString(Node::State status)
		{
			switch (status)
			{
			case Node::State::Success:
				return "Success";
			case Node::State::Failure:
				return "Failure";
			case Node::State::Running:
				return "Running";
			case Node::State::Idle:
				return "Idle";
			default:
				return "Unknown";
			}
		}

	public:
		virtual ~Node() = default;

		void SetName(const std::string &name) { m_name = name; }

		State Execute(AIContext &ctx)
		{
			if (!m_started)
			{
				OnStart(ctx);
				m_started = true;
			}

			m_state = OnExecute(ctx);

			if (m_state != State::Running)
				m_started = false;

			return m_state;
		}

		void Restart() { m_started = false; } // forget a half-finished run so OnStart fires again (used when a branch is interrupted)

		// Called by a Selector when a higher-priority branch preempts this one; composites also abort their children.
		virtual void Abort() { m_started = false; }

		virtual void OnStart(AIContext &) {}

		void Reset() { m_state = State::Running; }

		State GetState() { return m_state; }

		virtual State OnExecute(AIContext &ctx) = 0;

		const std::string &GetName() const { return m_name; }

	private:
		std::string m_name = "Node";
		State m_state = State::Running;
		bool m_started = false;
	};

} // namespace ai