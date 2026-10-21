#include "game/HerbivoreModel.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace game
{

	namespace
	{

		enum Bone
		{
			Hips,
			Chest,
			Neck,
			Head,
			Tail1,
			Tail2,
			LegFL,
			LegFR,
			LegBL,
			LegBR,
			BoneCount
		};

		struct BoneDef
		{
			const char *name;
			int parent;
			glm::vec3 pos; // bind-pose joint position for a unit-scale animal standing on y = 0
		};

		const BoneDef kBones[BoneCount] = {
			{"hips", -1, {-0.65f, 1.25f, 0.0f}},
			{"chest", Hips, {0.8f, 1.3f, 0.0f}},
			{"neck", Chest, {1.4f, 1.5f, 0.0f}},
			{"head", Neck, {1.8f, 1.5f, 0.0f}},
			{"tail1", Hips, {-1.4f, 1.2f, 0.0f}},
			{"tail2", Tail1, {-2.15f, 1.15f, 0.0f}},
			{"leg_fl", Chest, {0.95f, 0.7f, 0.5f}},
			{"leg_fr", Chest, {0.95f, 0.7f, -0.5f}},
			{"leg_bl", Hips, {-0.95f, 0.7f, 0.5f}},
			{"leg_br", Hips, {-0.95f, 0.7f, -0.5f}},
		};

		struct Layout
		{
			float scale;
			float frill; // frill size multiplier
			bool horns;
		};

		Layout LayoutOf(HerbivoreSpecies s)
		{
			return s == HerbivoreSpecies::Triceratops ? Layout{1.0f, 0.85f, true} : Layout{0.4f, 1.25f, false};
		}

		// The model stands on y = 0; this lowers it so the origin is the middle of the hit box.
		glm::vec3 ShiftOf(const Layout &l) { return {0.0f, -1.1f * l.scale, 0.0f}; }

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

		void AddBox(gfx::MeshData &m, const Layout &l, glm::vec3 center, glm::vec3 half, int bone)
		{
			const glm::vec3 c = center * l.scale + ShiftOf(l);
			half *= l.scale;
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

	glm::vec3 HerbivoreHalfExtents(HerbivoreSpecies species)
	{
		return glm::vec3(2.3f, 1.1f, 0.8f) * LayoutOf(species).scale;
	}

	gfx::MeshData HerbivoreMesh(HerbivoreSpecies species)
	{
		const Layout l = LayoutOf(species);
		gfx::MeshData m;
		AddBox(m, l, {-0.65f, 1.25f, 0.0f}, {0.85f, 0.6f, 0.65f}, Hips);	// rear torso
		AddBox(m, l, {0.8f, 1.3f, 0.0f}, {0.6f, 0.62f, 0.62f}, Chest);		// front torso
		AddBox(m, l, {1.5f, 1.45f, 0.0f}, {0.2f, 0.3f, 0.35f}, Neck);
		AddBox(m, l, {2.05f, 1.35f, 0.0f}, {0.4f, 0.32f, 0.3f}, Head);
		AddBox(m, l, {2.5f, 1.25f, 0.0f}, {0.18f, 0.2f, 0.14f}, Head);	   // beak
		AddBox(m, l, {1.78f, 1.7f + 0.2f * l.frill, 0.0f}, {0.07f, 0.45f * l.frill, 0.75f * l.frill}, Head); // frill
		if (l.horns)
		{
			AddBox(m, l, {2.3f, 1.95f, 0.22f}, {0.32f, 0.05f, 0.05f}, Head);
			AddBox(m, l, {2.3f, 1.95f, -0.22f}, {0.32f, 0.05f, 0.05f}, Head);
			AddBox(m, l, {2.55f, 1.55f, 0.0f}, {0.12f, 0.1f, 0.06f}, Head);
		}
		AddBox(m, l, {-1.7f, 1.2f, 0.0f}, {0.55f, 0.3f, 0.25f}, Tail1);
		AddBox(m, l, {-2.6f, 1.1f, 0.0f}, {0.4f, 0.2f, 0.15f}, Tail2);
		for (float side : {1.0f, -1.0f})
		{
			const bool left = side > 0.0f;
			AddBox(m, l, {0.95f, 0.36f, 0.5f * side}, {0.2f, 0.36f, 0.2f}, left ? LegFL : LegFR);
			AddBox(m, l, {-0.95f, 0.36f, 0.5f * side}, {0.24f, 0.36f, 0.22f}, left ? LegBL : LegBR);
		}
		return m;
	}

	std::shared_ptr<const gfx::Skeleton> HerbivoreSkeleton(HerbivoreSpecies species)
	{
		const Layout l = LayoutOf(species);
		auto s = std::make_shared<gfx::Skeleton>();
		for (int i = 0; i < BoneCount; ++i)
		{
			const glm::vec3 pos = kBones[i].pos * l.scale + ShiftOf(l);
			const glm::vec3 parentPos = kBones[i].parent < 0 ? glm::vec3(0.0f) : kBones[kBones[i].parent].pos * l.scale + ShiftOf(l);
			gfx::Bone b;
			b.name = kBones[i].name;
			b.parent = kBones[i].parent;
			b.localBind = glm::translate(glm::mat4(1.0f), pos - parentPos);
			b.inverseBind = glm::translate(glm::mat4(1.0f), -pos);
			s->bones.push_back(std::move(b));
		}
		return s;
	}

	void HerbivorePose(float time, float blend, std::vector<glm::mat4> &L)
	{
		if (L.size() < BoneCount)
			return;
		const glm::vec3 axisZ(0, 0, 1), axisY(0, 1, 0);
		auto rotZ = [&](int b, float a) { L[b] = L[b] * glm::rotate(glm::mat4(1.0f), a, axisZ); };
		auto rotY = [&](int b, float a) { L[b] = L[b] * glm::rotate(glm::mat4(1.0f), a, axisY); };

		const float graze = glm::clamp(-blend, 0.0f, 1.0f);
		blend = glm::max(blend, 0.0f);

		const float ph = time * 5.0f;
		const float s = std::sin(ph);
		const float amp = 0.55f * glm::min(1.0f, blend * 3.0f) + 0.2f * blend;

		// Diagonal pairs swing together.
		rotZ(LegFL, amp * s);
		rotZ(LegBR, amp * s);
		rotZ(LegFR, -amp * s);
		rotZ(LegBL, -amp * s);

		L[Hips] = glm::translate(glm::mat4(1.0f), {0.0f, 0.04f * blend * std::abs(s) - 0.05f * graze, 0.0f}) * L[Hips];
		rotZ(Chest, 0.025f * std::sin(time * 1.6f) - 0.06f * graze);
		rotZ(Neck, 0.04f * std::sin(time * 1.6f + 0.4f) - 0.12f * blend - 0.75f * graze);
		rotZ(Head, -0.05f * blend * std::sin(2.0f * ph) - 0.4f * graze + 0.09f * graze * std::sin(time * 9.0f));

		const float sway = 0.3f + blend;
		rotY(Tail1, 0.14f * sway * std::sin(ph * 0.5f));
		rotY(Tail2, 0.2f * sway * std::sin(ph * 0.5f - 0.9f));
	}

} // namespace game
