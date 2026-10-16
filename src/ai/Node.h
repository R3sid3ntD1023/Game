#pragma once

#include "AIContext.h"

namespace ai
{
	enum class NodeStatus
	{
		Success,
		Failure,
		Running
	};

	// Base class for all behavior tree nodes. Each node must implement the Execute method.
	class Node
	{
	public:
		virtual ~Node() = default;

		virtual NodeStatus Execute(AIContext &ctx) = 0;
	};

} // namespace ai