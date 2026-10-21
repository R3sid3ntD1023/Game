#include "eng/AnimStateMachine.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace eng
{
    namespace
    {
        struct TRS
        {
            glm::vec3 t, s;
            glm::quat r;
        };

        TRS Decompose(const glm::mat4 &m)
        {
            TRS o;
            o.t = glm::vec3(m[3]);
            glm::vec3 c0(m[0]), c1(m[1]), c2(m[2]);
            o.s = {glm::length(c0), glm::length(c1), glm::length(c2)};
            if (o.s.x > 0.0f) c0 /= o.s.x;
            if (o.s.y > 0.0f) c1 /= o.s.y;
            if (o.s.z > 0.0f) c2 /= o.s.z;
            o.r = glm::normalize(glm::quat_cast(glm::mat3(c0, c1, c2)));
            return o;
        }

        glm::mat4 Compose(const TRS &x)
        {
            glm::mat4 m = glm::mat4_cast(x.r);
            m[0] *= x.s.x;
            m[1] *= x.s.y;
            m[2] *= x.s.z;
            m[3] = glm::vec4(x.t, 1.0f);
            return m;
        }
    } // namespace

    void BlendPoses(const std::vector<glm::mat4> &a, const std::vector<glm::mat4> &b, float t, std::vector<glm::mat4> &out)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const size_t n = std::min(a.size(), b.size());
        std::vector<glm::mat4> result(n);
        for (size_t i = 0; i < n; ++i)
        {
            const TRS x = Decompose(a[i]), y = Decompose(b[i]);
            TRS r;
            r.t = glm::mix(x.t, y.t, t);
            r.s = glm::mix(x.s, y.s, t);
            r.r = glm::normalize(glm::slerp(x.r, y.r, t));
            result[i] = Compose(r);
        }
        out = std::move(result);
    }

    // ---- ClipNode ----

    void ClipNode::Evaluate(const gfx::Skeleton &skeleton, std::vector<glm::mat4> &locals) const
    {
        skeleton.LocalBindPose(locals);
        if (!m_clip || m_clip->duration <= 0.0f)
            return;
        const float t = m_loop ? std::fmod(m_time, m_clip->duration) : std::min(m_time, m_clip->duration);
        m_clip->Sample(t, locals);
    }

    float ClipNode::NormalizedTime() const
    {
        if (!m_clip || m_clip->duration <= 0.0f)
            return 0.0f;
        return m_loop ? std::fmod(m_time, m_clip->duration) / m_clip->duration : std::min(m_time / m_clip->duration, 1.0f);
    }

    // ---- BlendNode1D ----

    BlendNode1D &BlendNode1D::Add(float threshold, std::unique_ptr<AnimNode> node)
    {
        m_entries.push_back({threshold, std::move(node)});
        std::stable_sort(m_entries.begin(), m_entries.end(),
                         [](const Entry &a, const Entry &b) { return a.threshold < b.threshold; });
        return *this;
    }

    void BlendNode1D::Update(float dt, const AnimParams &params)
    {
        m_value = params.Get(m_parameter);
        for (auto &e : m_entries)
            e.node->Update(dt, params);
    }

    void BlendNode1D::Evaluate(const gfx::Skeleton &skeleton, std::vector<glm::mat4> &locals) const
    {
        if (m_entries.empty())
        {
            skeleton.LocalBindPose(locals);
            return;
        }
        if (m_value <= m_entries.front().threshold || m_entries.size() == 1)
            return m_entries.front().node->Evaluate(skeleton, locals);
        if (m_value >= m_entries.back().threshold)
            return m_entries.back().node->Evaluate(skeleton, locals);

        size_t hi = 1;
        while (m_entries[hi].threshold < m_value)
            ++hi;
        const Entry &lo = m_entries[hi - 1], &up = m_entries[hi];
        const float span = up.threshold - lo.threshold;
        const float t = span > 0.0f ? (m_value - lo.threshold) / span : 0.0f;

        std::vector<glm::mat4> a, b;
        lo.node->Evaluate(skeleton, a);
        up.node->Evaluate(skeleton, b);
        BlendPoses(a, b, t, locals);
    }

    void BlendNode1D::Reset()
    {
        for (auto &e : m_entries)
            e.node->Reset();
    }

    // ---- BlendNode2 ----

    void BlendNode2::Update(float dt, const AnimParams &params)
    {
        m_weight = params.Get(m_parameter);
        m_a->Update(dt, params);
        m_b->Update(dt, params);
    }

    void BlendNode2::Evaluate(const gfx::Skeleton &skeleton, std::vector<glm::mat4> &locals) const
    {
        std::vector<glm::mat4> a, b;
        m_a->Evaluate(skeleton, a);
        m_b->Evaluate(skeleton, b);
        BlendPoses(a, b, m_weight, locals);
    }

    // ---- AnimStateMachine ----

    AnimStateMachine &AnimStateMachine::AddStateId(StateId id, std::unique_ptr<AnimNode> node)
    {
        m_states[id] = std::move(node);
        if (!m_started)
        {
            m_started = true;
            m_current = id;
        }
        return *this;
    }

    AnimNode *AnimStateMachine::Find(StateId id) const
    {
        auto it = m_states.find(id);
        return it == m_states.end() ? nullptr : it->second.get();
    }

    void AnimStateMachine::GoTo(StateId to, float fade)
    {
        AnimNode *next = Find(to);
        if (!next || (m_started && to == m_current && !m_fading))
            return;
        next->Reset();
        if (fade > 0.0f && Find(m_current))
        {
            m_previous = m_current;
            m_fading = true;
            m_fadeTime = 0.0f;
            m_fadeDuration = fade;
        }
        else
        {
            m_fading = false;
        }
        m_current = to;
        m_started = true;
    }

    void AnimStateMachine::Update(float dt, const AnimParams &params)
    {
        if (!m_started)
            return;

        if (!m_fading)
        {
            for (const auto &tr : m_transitions)
            {
                if ((!tr.any && tr.from != m_current) || tr.to == m_current)
                    continue;
                if (tr.when && tr.when(params))
                {
                    GoTo(tr.to, tr.duration);
                    break;
                }
            }
        }

        if (AnimNode *cur = Find(m_current))
            cur->Update(dt, params);

        if (m_fading)
        {
            if (AnimNode *prev = Find(m_previous))
                prev->Update(dt, params);
            m_fadeTime += dt;
            if (m_fadeTime >= m_fadeDuration)
                m_fading = false;
        }
    }

    void AnimStateMachine::Evaluate(const gfx::Skeleton &skeleton, std::vector<glm::mat4> &locals) const
    {
        const AnimNode *cur = m_started ? Find(m_current) : nullptr;
        if (!cur)
        {
            skeleton.LocalBindPose(locals);
            return;
        }
        cur->Evaluate(skeleton, locals);
        if (m_fading)
        {
            if (const AnimNode *prev = Find(m_previous))
            {
                std::vector<glm::mat4> from;
                prev->Evaluate(skeleton, from);
                BlendPoses(from, locals, m_fadeTime / m_fadeDuration, locals);
            }
        }
    }

    void AnimStateMachine::Reset()
    {
        m_fading = false;
        for (auto &[id, node] : m_states)
            node->Reset();
    }

} // namespace eng
