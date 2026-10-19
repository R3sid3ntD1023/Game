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

			auto status = OnExecute(ctx);

			if (status != State::Running)
				m_started = false;

			return status;
		}

		virtual void OnStart(AIContext &ctx) {}

		virtual State OnExecute(AIContext &ctx) = 0;

		const std::string &GetName() const { return m_name; }

	private:
		std::string m_name = "Node";
		bool m_started = false;
	};

} // namespace ai