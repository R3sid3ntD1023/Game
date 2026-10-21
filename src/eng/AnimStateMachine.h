#pragma once
#include "gfx/Skeleton.h"
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace eng {

// Named float parameters driving blend nodes and transition conditions (bools/ints are stored as floats).
class AnimParams {
public:
    void Set(const std::string& name, float v) { m_values[name] = v; }
    void SetBool(const std::string& name, bool v) { m_values[name] = v ? 1.0f : 0.0f; }
    void SetInt(const std::string& name, int v) { m_values[name] = static_cast<float>(v); }
    float Get(const std::string& name, float fallback = 0.0f) const {
        auto it = m_values.find(name);
        return it == m_values.end() ? fallback : it->second;
    }
    bool GetBool(const std::string& name) const { return Get(name) != 0.0f; }
    int GetInt(const std::string& name) const { return static_cast<int>(Get(name)); }

private:
    std::unordered_map<std::string, float> m_values;
};

// Blends two parent-relative poses per bone (lerp T/S, slerp R). t = 0 -> a, t = 1 -> b.
void BlendPoses(const std::vector<glm::mat4>& a, const std::vector<glm::mat4>& b, float t, std::vector<glm::mat4>& out);

// A node in the animation graph; evaluates to a full local-space pose.
class AnimNode {
public:
    virtual ~AnimNode() = default;
    virtual void Update(float dt, const AnimParams& params) = 0;
    virtual void Evaluate(const gfx::Skeleton& skeleton, std::vector<glm::mat4>& locals) const = 0;
    virtual void Reset() {}
};

// Plays a single clip.
class ClipNode : public AnimNode {
public:
    ClipNode(const gfx::AnimationClip* clip, bool loop = true, float speed = 1.0f)
        : m_clip(clip), m_loop(loop), m_speed(speed) {}

    void Update(float dt, const AnimParams&) override { m_time += dt * m_speed; }
    void Evaluate(const gfx::Skeleton& skeleton, std::vector<glm::mat4>& locals) const override;
    void Reset() override { m_time = 0.0f; }

    bool Finished() const { return !m_loop && m_clip && m_time >= m_clip->duration; }
    float NormalizedTime() const;

private:
    const gfx::AnimationClip* m_clip;
    bool m_loop;
    float m_speed;
    float m_time = 0.0f;
};

// Procedural pose: starts from the bind pose and runs a hook with a fixed blend value.
class ProceduralNode : public AnimNode {
public:
    using Hook = std::function<void(float time, float blend, std::vector<glm::mat4>& locals)>;

    ProceduralNode(Hook hook, float blend = 0.0f) : m_hook(std::move(hook)), m_blend(blend) {}

    void Update(float dt, const AnimParams&) override { m_time += dt; }
    void Evaluate(const gfx::Skeleton& skeleton, std::vector<glm::mat4>& locals) const override {
        skeleton.LocalBindPose(locals);
        if (m_hook)
            m_hook(m_time, m_blend, locals);
    }
    void Reset() override { m_time = 0.0f; }

private:
    Hook m_hook;
    float m_blend;
    float m_time = 0.0f;
};

// 1D blend space: children are placed on a float parameter axis and the two neighbours of the value are blended.
class BlendNode1D : public AnimNode {
public:
    explicit BlendNode1D(std::string parameter) : m_parameter(std::move(parameter)) {}

    BlendNode1D& Add(float threshold, std::unique_ptr<AnimNode> node);

    void Update(float dt, const AnimParams& params) override;
    void Evaluate(const gfx::Skeleton& skeleton, std::vector<glm::mat4>& locals) const override;
    void Reset() override;

private:
    struct Entry {
        float threshold;
        std::unique_ptr<AnimNode> node;
    };
    std::string m_parameter;
    std::vector<Entry> m_entries; // sorted by threshold
    float m_value = 0.0f;
};

// Blends two nodes by a parameter in [0, 1].
class BlendNode2 : public AnimNode {
public:
    BlendNode2(std::string parameter, std::unique_ptr<AnimNode> a, std::unique_ptr<AnimNode> b)
        : m_parameter(std::move(parameter)), m_a(std::move(a)), m_b(std::move(b)) {}

    void Update(float dt, const AnimParams& params) override;
    void Evaluate(const gfx::Skeleton& skeleton, std::vector<glm::mat4>& locals) const override;
    void Reset() override { m_a->Reset(); m_b->Reset(); }

private:
    std::string m_parameter;
    std::unique_ptr<AnimNode> m_a, m_b;
    float m_weight = 0.0f;
};

// State machine whose states are any enum (stored as uint8_t) and whose state bodies are AnimNodes.
// Transitions are crossfaded; new transitions are ignored while a crossfade is running.
class AnimStateMachine : public AnimNode {
public:
    using StateId = std::uint8_t;
    using Condition = std::function<bool(const AnimParams&)>;

    template <class E>
    AnimStateMachine& AddState(E state, std::unique_ptr<AnimNode> node) {
        return AddStateId(ToId(state), std::move(node));
    }

    // Transition from `from` to `to` when `when` returns true, crossfading over `duration` seconds.
    template <class E>
    AnimStateMachine& AddTransition(E from, E to, Condition when, float duration = 0.2f) {
        m_transitions.push_back({ ToId(from), ToId(to), false, std::move(when), duration });
        return *this;
    }

    // Transition that may fire from any state.
    template <class E>
    AnimStateMachine& AddAnyTransition(E to, Condition when, float duration = 0.2f) {
        m_transitions.push_back({ 0, ToId(to), true, std::move(when), duration });
        return *this;
    }

    // Jumps to a state immediately (no fade) when fade <= 0, otherwise crossfades.
    template <class E>
    void SetState(E state, float fade = 0.0f) { GoTo(ToId(state), fade); }

    template <class E>
    E State() const { return static_cast<E>(m_current); }
    template <class E>
    bool IsIn(E state) const { return HasState() && m_current == ToId(state); }

    bool HasState() const { return m_started; }
    bool IsTransitioning() const { return m_fading; }
    AnimParams& Params() { return m_params; }
    const AnimParams& Params() const { return m_params; }

    void Update(float dt, const AnimParams& params) override;
    // Convenience: updates using the machine's own parameters.
    void Update(float dt) { Update(dt, m_params); }
    void Evaluate(const gfx::Skeleton& skeleton, std::vector<glm::mat4>& locals) const override;
    void Reset() override;

private:
    struct Transition {
        StateId from;
        StateId to;
        bool any;
        Condition when;
        float duration;
    };

    template <class E>
    static StateId ToId(E e) {
        static_assert(std::is_enum_v<E>, "state must be an enum");
        return static_cast<StateId>(static_cast<std::underlying_type_t<E>>(e));
    }

    AnimStateMachine& AddStateId(StateId id, std::unique_ptr<AnimNode> node);
    AnimNode* Find(StateId id) const;
    void GoTo(StateId to, float fade);

    std::unordered_map<StateId, std::unique_ptr<AnimNode>> m_states;
    std::vector<Transition> m_transitions;
    AnimParams m_params;

    StateId m_current = 0;
    StateId m_previous = 0;
    bool m_started = false; // first added state becomes the entry state
    bool m_fading = false;
    float m_fadeTime = 0.0f;
    float m_fadeDuration = 0.0f;
};

} // namespace eng
