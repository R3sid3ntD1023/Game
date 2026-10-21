#include "gfx/ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <filesystem>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace gfx
{

    std::string FindModelOverride(const std::string &name)
    {
        for (const char *ext : {".glb", ".gltf", ".obj", ".fbx", ".dae"})
        {
            const std::string path = std::string(ASSETS_DIR) + "models/" + name + ext;
            if (std::filesystem::exists(path))
                return path;
        }
        return {};
    }

    namespace
    {

        const unsigned kImportFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                                      aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices | aiProcess_GenUVCoords;

        const aiScene *Import(Assimp::Importer &importer, const std::string &path)
        {
            const aiScene *scene = importer.ReadFile(path, kImportFlags);
            if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mNumMeshes == 0)
                throw std::runtime_error("Failed to load model '" + path + "': " + importer.GetErrorString());
            return scene;
        }

        // Appends one assimp mesh to `out`.
        void AppendMesh(const aiMesh *mesh, float scale, MeshData &out)
        {
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

        bool FindTexture(const aiScene *scene, const aiMaterial *mat, std::initializer_list<aiTextureType> types,
                         const std::string &modelPath, TextureSource &out)
        {
            for (aiTextureType type : types)
            {
                aiString str;
                if (mat->GetTexture(type, 0, &str) != AI_SUCCESS || str.length == 0)
                    continue;

                if (const aiTexture *embedded = scene->GetEmbeddedTexture(str.C_Str()))
                {
                    if (embedded->mHeight != 0)
                    {
                        spdlog::warn("'{}': uncompressed embedded texture '{}' is not supported", modelPath, str.C_Str());
                        continue;
                    }
                    const auto *bytes = reinterpret_cast<const unsigned char *>(embedded->pcData);
                    out.encoded.assign(bytes, bytes + embedded->mWidth);
                    out.key = modelPath + "#" + str.C_Str();
                    return true;
                }

                std::filesystem::path p = std::filesystem::path(modelPath).parent_path() / std::string(str.C_Str());
                if (!std::filesystem::exists(p))
                {
                    spdlog::warn("'{}': texture '{}' not found", modelPath, p.string());
                    continue;
                }
                out.path = p.lexically_normal().string();
                out.key = out.path;
                return true;
            }
            return false;
        }

        ModelMaterial ImportMaterial(const aiScene *scene, const aiMaterial *mat, const std::string &modelPath)
        {
            ModelMaterial m;
            aiString name;
            if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
                m.name = name.C_Str();

            aiColor4D c;
            if (mat->Get(AI_MATKEY_BASE_COLOR, c) == AI_SUCCESS || mat->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS)
                m.baseColor = {c.r, c.g, c.b, c.a};

            aiColor3D e;
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, e) == AI_SUCCESS)
                m.emissive = {e.r, e.g, e.b};

            FindTexture(scene, mat, {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE}, modelPath, m.albedo);
            FindTexture(scene, mat, {aiTextureType_NORMALS, aiTextureType_HEIGHT}, modelPath, m.normal);
            const bool hasMR = FindTexture(scene, mat, {aiTextureType_UNKNOWN, aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS}, modelPath, m.metalRough);

            // glTF factors scale the packed texture, so a missing factor next to a texture means 1.
            float f = 0.0f;
            m.metallic = mat->Get(AI_MATKEY_METALLIC_FACTOR, f) == AI_SUCCESS ? f : (hasMR ? 1.0f : 0.0f);
            m.roughness = mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, f) == AI_SUCCESS ? f : (hasMR ? 1.0f : 0.6f);

            // Cutout transparency: glTF MASK uses its cutoff; BLEND can't be sorted by this renderer, so it is cut at 0.5.
            aiString alphaMode;
            if (mat->Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS)
            {
                const std::string mode = alphaMode.C_Str();
                if (mode == "MASK")
                    m.alphaCutoff = mat->Get("$mat.gltf.alphaCutoff", 0, 0, f) == AI_SUCCESS ? f : 0.5f;
                else if (mode == "BLEND")
                    m.alphaCutoff = 0.5f;
            }
            int twoSided = 0;
            if (mat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
                m.doubleSided = twoSided != 0;
            return m;
        }

    } // namespace

    MeshData LoadModel(const std::string &path, float scale)
    {
        Assimp::Importer importer;
        const aiScene *scene = Import(importer, path);

        MeshData out;
        for (unsigned m = 0; m < scene->mNumMeshes; ++m)
            AppendMesh(scene->mMeshes[m], scale, out);

        spdlog::info("Loaded model '{}': {} vertices, {} triangles", path, out.vertices.size(), out.indices.size() / 3);
        return out;
    }

    ModelAsset LoadModelAsset(const std::string &path, float scale)
    {
        Assimp::Importer importer;
        const aiScene *scene = Import(importer, path);

        ModelAsset asset;
        for (unsigned i = 0; i < scene->mNumMaterials; ++i)
            asset.materials.push_back(ImportMaterial(scene, scene->mMaterials[i], path));

        size_t triangles = 0;
        for (unsigned m = 0; m < scene->mNumMeshes; ++m)
        {
            ModelPart part;
            part.material = static_cast<int>(scene->mMeshes[m]->mMaterialIndex);
            AppendMesh(scene->mMeshes[m], scale, part.mesh);
            if (part.mesh.indices.empty())
                continue;
            triangles += part.mesh.indices.size() / 3;
            asset.bounds.Expand(part.mesh.ComputeBounds());
            asset.parts.push_back(std::move(part));
        }

        spdlog::info("Loaded model '{}': {} parts, {} materials, {} triangles", path, asset.parts.size(), asset.materials.size(), triangles);
        return asset;
    }

    Bounds QueryModelBounds(const std::string &path, float scale)
    {
        Assimp::Importer importer;
        const aiScene *scene = Import(importer, path);

        Bounds b;
        for (unsigned m = 0; m < scene->mNumMeshes; ++m)
        {
            const aiMesh *mesh = scene->mMeshes[m];
            for (unsigned i = 0; i < mesh->mNumVertices; ++i)
                b.Expand(glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z) * scale);
        }
        return b;
    }

} // namespace gfx
