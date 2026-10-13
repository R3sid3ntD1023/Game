#include "gfx/ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace gfx
{

    MeshData LoadModel(const std::string &path, float scale)
    {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path,
                                                 aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                                                     aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices | aiProcess_GenUVCoords);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mNumMeshes == 0)
            throw std::runtime_error("Failed to load model '" + path + "': " + importer.GetErrorString());

        MeshData out;
        for (unsigned m = 0; m < scene->mNumMeshes; ++m)
        {
            const aiMesh *mesh = scene->mMeshes[m];
            const uint32_t base = static_cast<uint32_t>(out.vertices.size());

            for (unsigned i = 0; i < mesh->mNumVertices; ++i)
            {
                Vertex v{};
                const aiVector3D &p = mesh->mVertices[i];
                v.position = glm::vec3(p.x, p.y, p.z) * scale;

                const aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
                v.normal = glm::vec3(n.x, n.y, n.z);

                if (mesh->HasTextureCoords(0))
                    v.uv = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};

                if (mesh->HasTangentsAndBitangents())
                {
                    const aiVector3D &t = mesh->mTangents[i];
                    const aiVector3D &b = mesh->mBitangents[i];
                    const glm::vec3 T(t.x, t.y, t.z), B(b.x, b.y, b.z);
                    v.tangent = glm::vec4(T, glm::dot(glm::cross(v.normal, T), B) < 0.0f ? -1.0f : 1.0f);
                }
                else
                {
                    const glm::vec3 up = std::abs(v.normal.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                    v.tangent = glm::vec4(glm::normalize(glm::cross(up, v.normal)), 1.0f);
                }
                out.vertices.push_back(v);
            }

            for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace &face = mesh->mFaces[f];
                if (face.mNumIndices != 3)
                    continue;
                for (unsigned k = 0; k < 3; ++k)
                    out.indices.push_back(base + face.mIndices[k]);
            }
        }

        spdlog::info("Loaded model '{}': {} vertices, {} triangles", path, out.vertices.size(), out.indices.size() / 3);
        return out;
    }

} // namespace gfx
