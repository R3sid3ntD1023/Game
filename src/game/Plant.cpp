#include "game/Plant.h"
#include <algorithm>

namespace game
{

	Plant::~Plant()
	{
		if (m_ecology)
			Ecology::Remove(m_ecology->plants, this);
	}

	void Plant::OnAttach()
	{
		m_baseScale = gameObject->GetTransform().scale;
		m_ecology->plants.push_back(this);
	}

	glm::vec3 Plant::Position() const
	{
		return gameObject->GetTransform().position;
	}

	void Plant::Bite()
	{
		if (m_bites > 0)
			--m_bites;
		m_regrowTimer = 0.0f;
		ApplyScale();
	}

	void Plant::OnUpdate(float dt)
	{
		if (m_bites >= m_maxBites)
			return;
		m_regrowTimer += dt;
		if (m_regrowTimer >= m_regrow / static_cast<float>(m_maxBites))
		{
			m_regrowTimer = 0.0f;
			++m_bites;
			ApplyScale();
		}
	}

	// Eaten plants shrink toward a stub rather than vanishing.
	void Plant::ApplyScale()
	{
		gameObject->GetTransform().scale = m_baseScale * (0.2f + 0.8f * Fullness());
	}

} // namespace game
