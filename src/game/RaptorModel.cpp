#include "game/RaptorModel.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace game
{

    namespace
    {

        enum Bone
        {
            Hips,
            Spine,
            Neck,
            Head,
            Tail1,
            Tail2,
            Tail3,
            ThighL,
            ShinL,
            FootL,
            ThighR,
            ShinR,
            FootR,
            ArmL,
            ArmR,
            BoneCount
        };

        struct BoneDef
        {
            const char *name;
            int parent;
            glm::vec3 pos; // bind-pose joint position
        };

        // Re-centers the model so the origin is the middle of the hit box.
        const glm::vec3 kShift{0.25f, -0.75f, 0.0f};

        const BoneDef kBones[BoneCount] = {
            {"hips", -1, {0.0f, 0.9f, 0.0f}},
            {"spine", Hips, {0.35f, 1.0f, 0.0f}},
            {"neck", Spine, {0.7f, 1.1f, 0.0f}},
            {"head", Neck, {1.0f, 1.35f, 0.0f}},
            {"tail1", Hips, {-0.4f, 0.95f, 0.0f}},
            {"tail2", Tail1, {-1.0f, 0.9f, 0.0f}},
            {"tail3", Tail2, {-1.6f, 0.85f, 0.0f}},
            {"thigh_l", Hips, {0.0f, 0.8f, 0.18f}},
            {"shin_l", ThighL, {0.1f, 0.5f, 0.18f}},
            {"foot_l", ShinL, {0.0f, 0.12f, 0.18f}},
            {"thigh_r", Hips, {0.0f, 0.8f, -0.18f}},
            {"shin_r", ThighR, {0.1f, 0.5f, -0.18f}},
            {"foot_r", ShinR, {0.0f, 0.12f, -0.18f}},
            {"arm_l", Spine, {0.6f, 0.9f, 0.15f}},
            {"arm_r", Spine, {0.6f, 0.9f, -0.15f}},
        };

        struct Face
        {
            glm::vec3 n, u, v;
        };
        // u x v == n, same winding as primitives::Cube.
        const Face kFaces[6] = {
            {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
            {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
            {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
            {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
        };

        void AddBox(gfx::MeshData &m, const glm::vec3 &center, const glm::vec3 &half, int bone)
        {
            const glm::vec3 c = center + kShift;
            for (const Face &f : kFaces)
            {
                const uint32_t base = static_cast<uint32_t>(m.vertices.size());
                const glm::vec4 tangent(f.u, 1.0f);
                const glm::vec3 fc = c + f.n * half;
                const glm::vec3 du = f.u * half, dv = f.v * half;
                const glm::ivec4 ids(bone, 0, 0, 0);
                const glm::vec4 weights(1.0f, 0.0f, 0.0f, 0.0f);
                m.vertices.push_back({fc - du - dv, f.n, {0, 0}, tangent, ids, weights});
                m.vertices.push_back({fc + du - dv, f.n, {1, 0}, tangent, ids, weights});
                m.vertices.push_back({fc + du + dv, f.n, {1, 1}, tangent, ids, weights});
                m.vertices.push_back({fc - du + dv, f.n, {0, 1}, tangent, ids, weights});
                for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u})
                    m.indices.push_back(base + i);
            }
        }

    } // namespace

    gfx::MeshData RaptorMesh()
    {
        gfx::MeshData m;
        AddBox(m, {0.1f, 1.0f, 0.0f}, {0.5f, 0.28f, 0.24f}, Hips);   // torso
        AddBox(m, {0.6f, 1.05f, 0.0f}, {0.22f, 0.26f, 0.2f}, Spine); // chest
        AddBox(m, {0.85f, 1.22f, 0.0f}, {0.2f, 0.09f, 0.09f}, Neck);
        AddBox(m, {1.2f, 1.4f, 0.0f}, {0.25f, 0.12f, 0.12f}, Head);
        AddBox(m, {1.5f, 1.36f, 0.0f}, {0.15f, 0.06f, 0.07f}, Head); // snout
        AddBox(m, {-0.7f, 0.93f, 0.0f}, {0.35f, 0.14f, 0.11f}, Tail1);
        AddBox(m, {-1.3f, 0.88f, 0.0f}, {0.3f, 0.1f, 0.08f}, Tail2);
        AddBox(m, {-1.85f, 0.84f, 0.0f}, {0.25f, 0.06f, 0.05f}, Tail3);
        for (float side : {1.0f, -1.0f})
        {
            const bool left = side > 0.0f;
            AddBox(m, {0.05f, 0.65f, 0.18f * side}, {0.1f, 0.28f, 0.08f}, left ? ThighL : ThighR);
            AddBox(m, {0.0f, 0.32f, 0.18f * side}, {0.06f, 0.2f, 0.05f}, left ? ShinL : ShinR);
            AddBox(m, {0.12f, 0.06f, 0.18f * side}, {0.18f, 0.05f, 0.06f}, left ? FootL : FootR);
            AddBox(m, {0.72f, 0.82f, 0.15f * side}, {0.1f, 0.03f, 0.03f}, left ? ArmL : ArmR);
        }
        return m;
    }

    std::shared_ptr<const gfx::Skeleton> RaptorSkeleton()
    {
        static const std::shared_ptr<const gfx::Skeleton> skeleton = []
        {
            auto s = std::make_shared<gfx::Skeleton>();
            for (int i = 0; i < BoneCount; ++i)
            {
                const glm::vec3 pos = kBones[i].pos + kShift;
                const glm::vec3 parentPos = kBones[i].parent < 0 ? glm::vec3(0.0f) : kBones[kBones[i].parent].pos + kShift;
                gfx::Bone b;
                b.name = kBones[i].name;
                b.parent = kBones[i].parent;
                b.localBind = glm::translate(glm::mat4(1.0f), pos - parentPos);
                b.inverseBind = glm::translate(glm::mat4(1.0f), -pos);
                s->bones.push_back(std::move(b));
            }
            return std::shared_ptr<const gfx::Skeleton>(s);
        }();
        return skeleton;
    }

    void RaptorPose(float time, float blend, std::vector<glm::mat4> &L)
    {
        if (L.size() < BoneCount)
            return;
        const glm::vec3 axisZ(0, 0, 1), axisY(0, 1, 0);
        auto rotZ = [&](int b, float a)
        { L[b] = L[b] * glm::rotate(glm::mat4(1.0f), a, axisZ); };
        auto rotY = [&](int b, float a)
        { L[b] = L[b] * glm::rotate(glm::mat4(1.0f), a, axisY); };

        const float ph = time * 6.0f;
        const float s = std::sin(ph);
        const float amp = 0.2f + 0.7f * blend;
        const float kneeBend = 0.3f + 0.5f * blend;

        // Legs alternate; the knee flexes while the leg swings forward.
        rotZ(ThighL, amp * s);
        rotZ(ThighR, -amp * s);
        rotZ(ShinL, -kneeBend * (0.5f + 0.5f * std::sin(ph + 1.2f)));
        rotZ(ShinR, -kneeBend * (0.5f + 0.5f * std::sin(ph + 1.2f + 3.14159f)));
        rotZ(FootL, 0.3f * amp * s);
        rotZ(FootR, -0.3f * amp * s);

        // Body: lean forward and bob while running, breathe while idle.
        L[Hips] = glm::translate(glm::mat4(1.0f), {0.0f, 0.05f * blend * std::abs(s), 0.0f}) * L[Hips];
        rotZ(Hips, -0.12f * blend);
        rotZ(Spine, 0.04f * std::sin(time * 2.0f) + 0.05f * blend * std::sin(2.0f * ph));
        rotZ(Neck, 0.1f * blend + 0.03f * std::sin(time * 2.0f + 0.5f));
        rotZ(Head, -0.1f * blend * std::sin(2.0f * ph + 0.6f));

        const float sway = 0.3f + blend;
        rotY(Tail1, 0.15f * sway * std::sin(ph * 0.5f));
        rotY(Tail2, 0.2f * sway * std::sin(ph * 0.5f - 0.8f));
        rotY(Tail3, 0.25f * sway * std::sin(ph * 0.5f - 1.6f));

        rotZ(ArmL, 0.3f * s);
        rotZ(ArmR, -0.3f * s);
    }

} // namespace game
