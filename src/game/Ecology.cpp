#include "game/Ecology.h"
#include "game/DinosaurAI.h"
#include "game/HerbivoreAI.h"
#include "game/Plant.h"
#include <algorithm>

namespace game
{

	void Ecology::Update(float dt)
	{
		for (auto &call : m_calls)
			call.age += dt;
		// Calls live long enough for every raptor to see them on at least one update.
		std::erase_if(m_calls, [](const PackCall &c) { return c.age > 0.5f; });
	}

	HerbivoreAI *Ecology::FindHerbivore(uint32_t id) const
	{
		for (HerbivoreAI *h : herbivores)
			if (h->Id() == id)
				return h;
		return nullptr;
	}

	void Ecology::EmitPackCall(const glm::vec3 &position, float radius, uint32_t preyId, const DinosaurAI *caller)
	{
		m_calls.push_back({m_nextCallId++, position, radius, 0.0f, preyId, caller});
	}

	int Ecology::PackSize(uint32_t preyId) const
	{
		int count = 0;
		for (const DinosaurAI *r : raptors)
			if (!r->Dead() && r->PreyId() == preyId)
				++count;
		return count;
	}

	DinosaurAI *Ecology::NearestRaptor(const glm::vec3 &position, float range, float *distance) const
	{
		DinosaurAI *best = nullptr;
		float bestDist = range;
		for (DinosaurAI *r : raptors)
		{
			if (r->Dead())
				continue;
			const glm::vec3 d = r->gameObject->GetTransform().position - position;
			const float dist = glm::length(glm::vec2(d.x, d.z));
			if (dist <= bestDist)
			{
				bestDist = dist;
				best = r;
			}
		}
		if (best && distance)
			*distance = bestDist;
		return best;
	}

} // namespace game
