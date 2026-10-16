#pragma once

#include "Blackboard.h"

namespace eng
{
	class GameObject;
}

namespace ai
{
	struct AIContext
	{
		eng::GameObject *self;
		Blackboard blackboard;
	};
} // namespace ai