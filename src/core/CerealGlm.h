#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// cereal finds these through ADL on the glm types.
namespace glm
{

    template <class Archive>
    void serialize(Archive &ar, vec2 &v) { ar(v.x, v.y); }
    template <class Archive>
    void serialize(Archive &ar, vec3 &v) { ar(v.x, v.y, v.z); }
    template <class Archive>
    void serialize(Archive &ar, vec4 &v) { ar(v.x, v.y, v.z, v.w); }
    template <class Archive>
    void serialize(Archive &ar, quat &q) { ar(q.x, q.y, q.z, q.w); }

} // namespace glm
