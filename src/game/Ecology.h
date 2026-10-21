#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace game
{

	class DinosaurAI;
	class HerbivoreAI;
	class Plant;

	// A raptor's hunting cry: every awake raptor within `radius` may join the pack going after `preyId`.
	struct PackCall
	{
		uint32_t id = 0;
		glm::vec3 position{0.0f};
		float radius = 0.0f;
		float age = 0.0f;
		uint32_t preyId = 0;
		const DinosaurAI *caller = nullptr;
	};

	// Registry of the living things that react to each other. Members register and unregister themselves;
	// scripts hold it by shared_ptr so it outlives them however the game tears the world down.
	class Ecology
	{
	public:
		std::vector<Plant *> plants;
		std::vector<HerbivoreAI *> herbivores;
		std::vector<DinosaurAI *> raptors;

		void Update(float dt);

		uint32_t NextHerbivoreId() { return m_nextHerbivoreId++; }
		HerbivoreAI *FindHerbivore(uint32_t id) const;

		void EmitPackCall(const glm::vec3 &position, float radius, uint32_t preyId, const DinosaurAI *caller);
		const std::vector<PackCall> &Calls() const { return m_calls; }
		// How many living raptors are currently committed to this prey.
		int PackSize(uint32_t preyId) const;

		// Living raptor closest to `position` within `range`, or null.
		DinosaurAI *NearestRaptor(const glm::vec3 &position, float range, float *distance = nullptr) const;

		template <class T>
		static void Remove(std::vector<T *> &list, const T *item)
		{
			std::erase(list, item);
		}

	private:
		std::vector<PackCall> m_calls;
		uint32_t m_nextCallId = 1;
		uint32_t m_nextHerbivoreId = 1;
	};

} // namespace game
