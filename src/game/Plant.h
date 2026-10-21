#pragma once
#include "eng/Component.h"
#include "eng/GameObject.h"
#include "game/Ecology.h"
#include <glm/glm.hpp>
#include <memory>

namespace game
{

	// An edible plant (a fern). Herbivores graze it a few bites at a time; it shrinks as it is eaten and regrows.
	class Plant : public eng::Component
	{
	public:
		Plant(std::shared_ptr<Ecology> ecology, int bites, float regrowSeconds)
			: m_ecology(std::move(ecology)),
			  m_maxBites(bites),
			  m_bites(bites),
			  m_regrow(regrowSeconds)
		{
		}
		~Plant() override;

		void OnAttach() override;
		void OnUpdate(float dt) override;

		bool Available() const { return m_bites > 0; }
		float Fullness() const { return static_cast<float>(m_bites) / static_cast<float>(m_maxBites); }
		glm::vec3 Position() const;

		// One herbivore at a time per plant.
		bool ClaimedByOther(const void *who) const { return m_claim && m_claim != who; }
		void Claim(const void *who) { m_claim = who; }
		void Release(const void *who)
		{
			if (m_claim == who)
				m_claim = nullptr;
		}
		void ForceRelease() { m_claim = nullptr; }

		void Bite();

	private:
		void ApplyScale();

		std::shared_ptr<Ecology> m_ecology;
		int m_maxBites;
		int m_bites;
		float m_regrow;
		float m_regrowTimer = 0.0f;
		glm::vec3 m_baseScale{1.0f};
		const void *m_claim = nullptr;
	};

} // namespace game
