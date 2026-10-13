#include "gfx/Skeleton.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace gfx
{

    int Skeleton::FindBone(const std::string &name) const
    {
        for (size_t i = 0; i < bones.size(); ++i)
            if (bones[i].name == name)
                return static_cast<int>(i);
        return -1;
    }

    void Skeleton::LocalBindPose(std::vector<glm::mat4> &out) const
    {
        out.resize(bones.size());
        for (size_t i = 0; i < bones.size(); ++i)
            out[i] = bones[i].localBind;
    }

    void Skeleton::ComputeSkinMatrices(const std::vector<glm::mat4> &locals, std::vector<glm::mat4> &out) const
    {
        out.resize(bones.size());
        std::vector<glm::mat4> global(bones.size());
        for (size_t i = 0; i < bones.size(); ++i)
        {
            const int p = bones[i].parent;
            global[i] = p < 0 ? locals[i] : global[p] * locals[i];
            out[i] = global[i] * bones[i].inverseBind;
        }
    }

    namespace
    {

        template <class T, class Interp>
        T SampleKeys(const std::vector<Key<T>> &keys, float t, const T &fallback, Interp interp)
        {
            if (keys.empty())
                return fallback;
            if (keys.size() == 1 || t <= keys.front().time)
                return keys.front().value;
            if (t >= keys.back().time)
                return keys.back().value;
            auto next = std::upper_bound(keys.begin(), keys.end(), t, [](float v, const Key<T> &k)
                                         { return v < k.time; });
            const Key<T> &a = *(next - 1);
            const Key<T> &b = *next;
            return interp(a.value, b.value, (t - a.time) / (b.time - a.time));
        }

    } // namespace

    void AnimationClip::Sample(float time, std::vector<glm::mat4> &locals) const
    {
        for (const BoneChannel &ch : channels)
        {
            if (ch.bone < 0 || ch.bone >= static_cast<int>(locals.size()))
                continue;
            const glm::vec3 p = SampleKeys(ch.positions, time, glm::vec3(0.0f), [](auto a, auto b, float f)
                                           { return glm::mix(a, b, f); });
            const glm::quat r = SampleKeys(ch.rotations, time, glm::quat(1, 0, 0, 0), [](auto a, auto b, float f)
                                           { return glm::slerp(a, b, f); });
            const glm::vec3 s = SampleKeys(ch.scales, time, glm::vec3(1.0f), [](auto a, auto b, float f)
                                           { return glm::mix(a, b, f); });
            locals[ch.bone] = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
        }
    }

} // namespace gfx
