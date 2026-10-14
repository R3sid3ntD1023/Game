#pragma once
#include "eng/Component.h"
#include <memory>
#include <vector>

namespace eng
{

    class GameObject;

    // Gameplay logic unit. Attach any number of scripts to a ScriptComponent.
    class Script
    {
    public:
        virtual ~Script() = default;

        virtual void OnStart() {}
        virtual void OnUpdate(float /*dt*/) {}
        virtual void OnFixedUpdate(float /*dt*/) {}
        virtual void OnKey(int /*key*/, int /*action*/) {}
        virtual void OnMouseMove(float /*dx*/, float /*dy*/) {}
        virtual void OnMouseButton(int /*button*/, int /*action*/) {}
        virtual void OnMouseScroll(float /*offset*/) {}

        GameObject *gameObject = nullptr;
        bool started = false;
    };

    class ScriptComponent : public Component
    {
    public:
        template <class T, class... Args>
        T &Add(Args &&...args)
        {
            auto script = std::make_unique<T>(std::forward<Args>(args)...);
            T &ref = *script;
            script->gameObject = gameObject;
            m_scripts.push_back(std::move(script));
            return ref;
        }

        void OnUpdate(float dt) override;
        void OnFixedUpdate(float dt) override;
        void OnKey(int key, int action) override;
        void OnMouseMove(float dx, float dy) override;
        void OnMouseButton(int button, int action) override;
        void OnMouseScroll(float offset) override;

    private:
        std::vector<std::unique_ptr<Script>> m_scripts;
    };

} // namespace eng
