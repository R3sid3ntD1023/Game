#pragma once
#include "core/Input.h"

namespace eng
{

    class GameObject;

    // Base class for everything attached to a GameObject. Input events are forwarded to the
    // component registered as the Input receiver.
    class Component : public core::IInputReceiver
    {
    public:
        ~Component() override = default;

        virtual void OnAttach() {}
        virtual void OnStart() {}
        virtual void OnUpdate(float /*dt*/) {}
        virtual void OnFixedUpdate(float /*dt*/) {}
        virtual void OnDestroy() {}

        GameObject *gameObject = nullptr;
        bool started = false;
    };

} // namespace eng
