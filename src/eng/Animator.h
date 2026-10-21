#pragma once
#include "eng/AnimStateMachine.h"
#include "eng/Component.h"
#include "gfx/Skeleton.h"
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace eng {

// Poses a Skeleton each frame (clip and/or procedural hook) and publishes the skin matrices for rendering.
class Animator : public Component {
public:
    // Runs after the clip is sampled; edit `locals` (parent-relative bone transforms) in place.
    using PoseHook = std::function<void(float time, float blend, std::vector<glm::mat4>& locals)>;

    explicit Animator(std::shared_ptr<const gfx::Skeleton> skeleton,
                      std::shared_ptr<const std::vector<gfx::AnimationClip>> clips = nullptr);

    void OnAttach() override;
    void OnUpdate(float dt) override;

    bool Play(const std::string& clip, bool loop = true);

    // Optional animation graph; when set it replaces the single clip as the base pose.
    std::shared_ptr<AnimNode> graph;
    AnimParams graphParams; // parameters fed to the graph (a state machine also reads its own via Update(dt))
    const gfx::AnimationClip* FindClip(const std::string& name) const;

    PoseHook hook;
    float speed = 1.0f; // playback / time scale
    float blend = 0.0f; // free parameter for the hook (e.g. idle 0 .. run 1)

private:
    std::shared_ptr<const gfx::Skeleton> m_skeleton;
    std::shared_ptr<const std::vector<gfx::AnimationClip>> m_clips;
    const gfx::AnimationClip* m_clip = nullptr;
    bool m_loop = true;
    float m_time = 0.0f;
    std::vector<glm::mat4> m_locals;
};

} // namespace eng
