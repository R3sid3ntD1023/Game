#include "eng/RigidBody.h"
#include "eng/GameObject.h"
#include "eng/World.h"

namespace eng
{

    void RigidBody::OnAttach() { gameObject->GetWorld().Physics().Add(this); }
    void RigidBody::OnDestroy() { gameObject->GetWorld().Physics().Remove(this); }

} // namespace eng
