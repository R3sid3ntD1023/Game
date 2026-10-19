#pragma once

#include <glm/vec3.hpp>

namespace eng
{
	class IWorldQuery
	{
	public:
		virtual ~IWorldQuery() = default;

		virtual float Height(float x, float z) const = 0;

		virtual glm::vec3 Normal(float x, float z) const = 0;

		virtual bool IsWalkable(float x, float z) const = 0;

		virtual int GetHalfSize() const = 0;
	};
} // namespace eng