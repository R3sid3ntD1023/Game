#include "eng/BuiltinComponents.h"
#include "eng/GameObject.h"
#include "eng/World.h"

namespace eng
{

	void MeshRendererComponent::OnAttach()
	{
		gameObject->GetWorld().Registry().emplace_or_replace<MeshRenderer>(gameObject->Entity(), MeshRenderer{mesh, material});
	}

	void CameraComponent::OnAttach()
	{
		gameObject->GetWorld().Registry().emplace_or_replace<CameraComponent>(gameObject->Entity(), CameraComponent{primary, camera});
	}

} // namespace eng
