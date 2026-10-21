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
		float deltaTime;
		eng::GameObject *self;
		Blackboard blackboard;
	};
} // namespace ai