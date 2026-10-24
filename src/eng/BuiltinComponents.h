#pragma once
#include "eng/Component.h"
#include "gfx/Camera.h"
#include "gfx/Material.h"
#include "gfx/Mesh.h"
#include <algorithm>
#include <memory>

namespace eng
{

	// Registers the object with the RenderSystem (via the ECS MeshRenderer component).
	class MeshRendererComponent : public Component
	{
	public:
		MeshRendererComponent(gfx::MeshHandle mesh, std::shared_ptr<gfx::Material> material)
			: mesh(mesh),
			  material(std::move(material))
		{
		}

		void OnAttach() override;

		gfx::MeshHandle mesh;
		std::shared_ptr<gfx::Material> material;
	};

	using OnDamageCallback = std::function<void(float)>;
	using OnHealCallback = std::function<void(float)>;

	class Health : public Component
	{
	public:
		explicit Health(float maxHealth = 100.0f)
			: max(maxHealth),
			  current(maxHealth)
		{
		}

		void Damage(float amount)
		{
			current = std::max(0.0f, current - amount);
			if (onDamage)
				onDamage(amount);
		}
		void Heal(float amount)
		{
			current = std::min(max, current + amount);
			if (onHeal)
				onHeal(amount);
		}

		bool Dead() const { return current <= 0.0f; }

		void SetOnDamageCallback(OnDamageCallback callback) { onDamage = std::move(callback); }

		void SetOnHealCallback(OnHealCallback callback) { onHeal = std::move(callback); }

		float max;
		float current;
		OnDamageCallback onDamage;
		OnHealCallback onHeal;
	};

	class CameraComponent : public Component
	{
	public:
		CameraComponent() = default;
		CameraComponent(bool primary, const gfx::Camera &camera)
			: primary(primary),
			  camera(camera)
		{
		}

		void OnAttach() override;

		bool primary = true;
		gfx::Camera camera;
	};

} // namespace eng
