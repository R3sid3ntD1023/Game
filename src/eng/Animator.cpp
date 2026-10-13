#include "eng/Animator.h"
#include "eng/GameObject.h"
#include "eng/World.h"
#include <cmath>

namespace eng
{

    Animator::Animator(std::shared_ptr<const gfx::Skeleton> skeleton, std::shared_ptr<const std::vector<gfx::AnimationClip>> clips)
        : m_skeleton(std::move(skeleton)), m_clips(std::move(clips)) {}

    void Animator::OnAttach()
    {
        gameObject->GetWorld().Registry().emplace_or_replace<SkinnedPose>(gameObject->Entity());
    }

    bool Animator::Play(const std::string &clip, bool loop)
    {
        if (!m_clips)
            return false;
        for (const auto &c : *m_clips)
        {
            if (c.name == clip)
            {
                m_clip = &c;
                m_loop = loop;
                m_time = 0.0f;
                return true;
            }
        }
        return false;
    }

    void Animator::OnUpdate(float dt)
    {
        m_time += dt * speed;
        m_skeleton->LocalBindPose(m_locals);

        if (m_clip && m_clip->duration > 0.0f)
        {
            const float t = m_loop ? std::fmod(m_time, m_clip->duration) : std::min(m_time, m_clip->duration);
            m_clip->Sample(t, m_locals);
        }
        if (hook)
            hook(m_time, blend, m_locals);

        auto &pose = gameObject->GetWorld().Registry().get<SkinnedPose>(gameObject->Entity());
        m_skeleton->ComputeSkinMatrices(m_locals, pose.matrices);
    }

} // namespace eng
