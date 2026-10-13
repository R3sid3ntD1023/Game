#include "gfx/ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <functional>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <unordered_map>

namespace gfx {

namespace {

glm::mat4 ToGlm(const aiMatrix4x4& m) { return glm::transpose(glm::make_mat4(&m.a1)); } // assimp is row-major

} // namespace

SkinnedModel LoadSkinnedModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_GenUVCoords);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode || scene->mNumMeshes == 0)
        throw std::runtime_error("Failed to load skinned model '" + path + "': " + importer.GetErrorString());

    SkinnedModel out;
    std::unordered_map<std::string, int> boneIndex;

    // Every node becomes a bone so animation channels can target any of them.
    std::function<void(const aiNode*, int)> walk = [&](const aiNode* node, int parent) {
        const int id = static_cast<int>(out.skeleton.bones.size());
        Bone b;
        b.name = node->mName.C_Str();
        b.parent = parent;
        b.localBind = ToGlm(node->mTransformation);
        out.skeleton.bones.push_back(b);
        boneIndex[b.name] = id;
        for (unsigned i = 0; i < node->mNumChildren; ++i) walk(node->mChildren[i], id);
    };
    walk(scene->mRootNode, -1);

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        const uint32_t base = static_cast<uint32_t>(out.mesh.vertices.size());

        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v{};
            v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            if (mesh->HasNormals()) v.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            else v.normal = { 0, 1, 0 };
            if (mesh->HasTextureCoords(0)) v.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            if (mesh->HasTangentsAndBitangents()) {
                const glm::vec3 T(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                const glm::vec3 B(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
                v.tangent = glm::vec4(T, glm::dot(glm::cross(v.normal, T), B) < 0.0f ? -1.0f : 1.0f);
            } else {
                const glm::vec3 up = std::abs(v.normal.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                v.tangent = glm::vec4(glm::normalize(glm::cross(up, v.normal)), 1.0f);
            }
            out.mesh.vertices.push_back(v);
        }

        for (unsigned b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            auto it = boneIndex.find(bone->mName.C_Str());
            if (it == boneIndex.end()) continue;
            out.skeleton.bones[it->second].inverseBind = ToGlm(bone->mOffsetMatrix);

            for (unsigned w = 0; w < bone->mNumWeights; ++w) {
                Vertex& v = out.mesh.vertices[base + bone->mWeights[w].mVertexId];
                for (int slot = 0; slot < 4; ++slot) {
                    if (v.boneWeights[slot] == 0.0f) {
                        v.boneIds[slot] = it->second;
                        v.boneWeights[slot] = bone->mWeights[w].mWeight;
                        break;
                    }
                }
            }
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            for (unsigned k = 0; k < 3; ++k) out.mesh.indices.push_back(base + face.mIndices[k]);
        }
    }

    for (Vertex& v : out.mesh.vertices) {
        const float sum = v.boneWeights.x + v.boneWeights.y + v.boneWeights.z + v.boneWeights.w;
        if (sum > 0.0f) v.boneWeights /= sum;
    }

    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* anim = scene->mAnimations[a];
        const float tps = anim->mTicksPerSecond > 0.0 ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;

        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = static_cast<float>(anim->mDuration) / tps;
        for (unsigned c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* ch = anim->mChannels[c];
            auto it = boneIndex.find(ch->mNodeName.C_Str());
            if (it == boneIndex.end()) continue;

            BoneChannel bc;
            bc.bone = it->second;
            for (unsigned k = 0; k < ch->mNumPositionKeys; ++k) {
                const auto& key = ch->mPositionKeys[k];
                bc.positions.push_back({ static_cast<float>(key.mTime) / tps, { key.mValue.x, key.mValue.y, key.mValue.z } });
            }
            for (unsigned k = 0; k < ch->mNumRotationKeys; ++k) {
                const auto& key = ch->mRotationKeys[k];
                bc.rotations.push_back({ static_cast<float>(key.mTime) / tps, glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z) });
            }
            for (unsigned k = 0; k < ch->mNumScalingKeys; ++k) {
                const auto& key = ch->mScalingKeys[k];
                bc.scales.push_back({ static_cast<float>(key.mTime) / tps, { key.mValue.x, key.mValue.y, key.mValue.z } });
            }
            clip.channels.push_back(std::move(bc));
        }
        out.clips.push_back(std::move(clip));
    }

    spdlog::info("Loaded skinned model '{}': {} vertices, {} bones, {} clips", path, out.mesh.vertices.size(),
                 out.skeleton.bones.size(), out.clips.size());
    return out;
}

} // namespace gfx
