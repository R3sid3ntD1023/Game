#pragma once
#include <cereal/cereal.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// cereal finds these through ADL on the glm types.
namespace glm
{

    template <class Archive>
    void serialize(Archive &ar, vec2 &v) { ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y)); }
    template <class Archive>
    void serialize(Archive &ar, vec3 &v) { ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z)); }
    template <class Archive>
    void serialize(Archive &ar, vec4 &v) { ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w)); }
    template <class Archive>
    void serialize(Archive &ar, quat &q) { ar(cereal::make_nvp("x", q.x), cereal::make_nvp("y", q.y), cereal::make_nvp("z", q.z), cereal::make_nvp("w", q.w)); }

} // namespace glm
