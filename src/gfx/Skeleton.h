#pragma once
#include "gfx/Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace gfx {

struct Bone {
    std::string name;
    int parent = -1; // parents always precede children
    glm::mat4 localBind{ 1.0f };
    glm::mat4 inverseBind{ 1.0f };
};

struct Skeleton {
    std::vector<Bone> bones;

    int FindBone(const std::string& name) const;
    void LocalBindPose(std::vector<glm::mat4>& out) const;
    // out[i] = globalTransform(i) * inverseBind(i), ready for the vertex shader.
    void ComputeSkinMatrices(const std::vector<glm::mat4>& locals, std::vector<glm::mat4>& out) const;
};

template <class T>
struct Key {
    float time; // seconds
    T value;
};

struct BoneChannel {
    int bone = -1;
    std::vector<Key<glm::vec3>> positions;
    std::vector<Key<glm::quat>> rotations;
    std::vector<Key<glm::vec3>> scales;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f; // seconds
    std::vector<BoneChannel> channels;

    // Overwrites locals[] for every animated bone; other entries are left untouched.
    void Sample(float time, std::vector<glm::mat4>& locals) const;
};

struct SkinnedModel {
    MeshData mesh;
    Skeleton skeleton;
    std::vector<AnimationClip> clips;
};

} // namespace gfx
