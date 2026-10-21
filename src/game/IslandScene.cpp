#include "game/IslandScene.h"
#include "game/Terrain.h"
#include "game/HerbivoreModel.h"
#include "game/TreeGen.h"
#include "gfx/ModelLoader.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <spdlog/spdlog.h>

namespace game
{

	eng::SceneData MakeIslandScene(const IslandParams &params, const Terrain &heightSource)
	{
		eng::SceneData s;

		// constexpr int kTreeVariants = 6;
		constexpr int kFernVariants = 4;
		// float treeHeight[kTreeVariants];
		int /*trunkMesh[kTreeVariants], canopyMesh[kTreeVariants],*/ fernMesh[kFernVariants];
		int tree = s.AddMesh({"tree", "model", {1.0f}, "models/realistic_tree.glb", true});

		int medkits[2];
		medkits[0] = s.AddMesh({"medkit", "model", {0.5f}, "models/medkit.glb"});
		medkits[1] = s.AddMesh({"firstaid", "model", {0.5f}, "models/firstaid.glb"});

		const int cube = s.AddMesh({"cube", "cube", {0.5f}});
		const int boulder = s.AddMesh({"boulder", "model", {0.5f}, "models/forest_boulder_wgosajs_low.glb", true});
		gfx::Bounds boulderBounds{glm::vec3(-0.5f), glm::vec3(0.5f)};
		try
		{
			boulderBounds = gfx::QueryModelBounds(std::string(ASSETS_DIR) + "models/forest_boulder_wgosajs_low.glb", 0.5f);
		}
		catch (const std::exception &e)
		{
			spdlog::warn("Boulder bounds unavailable, using a unit box: {}", e.what());
		}
		const int terrain = s.AddMesh({"terrain", kMeshTerrain, {320.0f}});
		const int sea = s.AddMesh({"sea", "plane", {900.0f, 1.0f}});
		const int raptor = s.AddMesh({"raptor", kMeshRaptor, {}});
		// for (int v = 0; v < kTreeVariants; ++v)
		// {
		//     const uint32_t seed = 100u + static_cast<uint32_t>(v);
		//     // treeHeight[v] = GenerateRainforestTree(seed).height;
		//     //  trunkMesh[v] = s.AddMesh({"trunk", kMeshTreeTrunk, {static_cast<float>(seed)}});
		//     canopyMesh[v] = s.AddMesh({"canopy", "model", {1.0f}, "models/realistic_tree.glb"});
		// }
		// All fern instances share one mesh (and its imported materials).
		const int fernModel = s.AddMesh({"fern", "model", {1.0f}, "models/fern.glb", true});
		for (int v = 0; v < kFernVariants; ++v)
			fernMesh[v] = fernModel;

		const int grass = s.AddMaterial({"ground", {0.07f, 0.12f, 0.05f}, 0.0f, 0.95f, {}});
		s.materials[grass].shader = "Terrain"; // procedural layers (sand, grass, mud, rock)
		const int grassMat = s.AddMaterial({"grass", {0.07f, 0.2f, 0.04f}, 0.0f, 0.8f, {}});
		s.materials[grassMat].shader = "Grass";
		s.materials[grassMat].windStrength = 0.4f;
		constexpr int kGrassVariants = 3;
		int grassMesh[kGrassVariants];
		for (int v = 0; v < kGrassVariants; ++v)
			grassMesh[v] = s.AddMesh({"grass", kMeshGrass, {static_cast<float>(500 + v)}});
		const int water = s.AddMaterial({"sea", {0.01f, 0.04f, 0.07f}, 0.0f, 0.05f, {}});
		s.materials[water].shader = "Ocean"; // procedural waves, foam and shallow-water tint
		s.materials[water].doubleSided = true; // the underside is drawn as seen from below
		// const int bark = s.AddMaterial({"bark", {0.16f, 0.11f, 0.08f}, 0.0f, 0.9f, {}});
		const int leaves = s.AddMaterial({"leaves", {0.05f, 0.16f, 0.05f}, 0.0f, 0.75f, {}});
		const int fern = s.AddMaterial({"fern", {0.08f, 0.22f, 0.06f}, 0.0f, 0.8f, {}});
		const int rock = s.AddMaterial({"rock", {0.25f, 0.25f, 0.27f}, 0.0f, 0.6f, {}});
		const int wood = s.AddMaterial({"boat", {0.4f, 0.28f, 0.16f}, 0.0f, 0.5f, {}});
		const int beaconMat = s.AddMaterial({"beacon", {0.1f, 0.1f, 0.1f}, 0.0f, 0.3f, {0.2f, 1.0f, 1.2f}});
		const int wall = s.AddMaterial({"wall", {0.3f, 0.28f, 0.25f}, 0.0f, 0.9f, {}});
		const int doorMat = s.AddMaterial({"door", {0.2f, 0.13f, 0.08f}, 0.0f, 0.7f, {}});
		const int gateMat = s.AddMaterial({"gate", {0.2f, 0.22f, 0.25f}, 0.8f, 0.4f, {}});
		const int buildingMat = s.AddMaterial({"building", {0.22f, 0.21f, 0.19f}, 0.0f, 0.85f, {}});
		const int roofMat = s.AddMaterial({"roof", {0.12f, 0.1f, 0.1f}, 0.1f, 0.7f, {}});
		const int windowMat = s.AddMaterial({"window", {0.05f, 0.04f, 0.03f}, 0.0f, 0.4f, {0.9f, 0.55f, 0.2f}});
		const int lampMat = s.AddMaterial({"lamp", {0.1f, 0.1f, 0.1f}, 0.0f, 0.4f, {3.0f, 2.1f, 1.1f}});
		const int poleMat = s.AddMaterial({"pole", {0.08f, 0.08f, 0.09f}, 0.7f, 0.5f, {}});
		const int medkitMat = s.AddMaterial({"medkit", {0.9f, 0.9f, 0.9f}, 0.0f, 0.5f, {0.1f, 1.2f, 0.3f}});
		const int weaponMat = s.AddMaterial({"weapon", {0.12f, 0.12f, 0.14f}, 0.9f, 0.3f, {1.2f, 0.7f, 0.1f}});
		const int crateMat = s.AddMaterial({"crate", {0.2f, 0.24f, 0.12f}, 0.0f, 0.7f, {0.05f, 0.25f, 0.05f}});
		const int raptorMats[2] = {s.AddMaterial({"raptor_green", {0.12f, 0.25f, 0.1f}, 0.0f, 0.6f, {}}), s.AddMaterial({"raptor_rust", {0.35f, 0.18f, 0.08f}, 0.0f, 0.6f, {}})};

		auto add = [&](const char *name, glm::vec3 pos, glm::vec3 scale, int mesh, int material) -> eng::ObjectDesc &
		{
			eng::ObjectDesc o;
			o.name = name;
			o.position = pos;
			o.scale = scale;
			o.mesh = mesh;
			o.material = material;
			return s.AddObject(std::move(o));
		};
		// Static box collider matching its (cube) visual.
		auto solid = [&](const char *name, glm::vec3 center, glm::vec3 size, int material) -> eng::ObjectDesc &
		{
			auto &o = add(name, center, size, cube, material);
			o.hasBody = true;
			o.bodyStatic = true;
			o.halfExtents = size * 0.5f;
			return o;
		};

		std::mt19937 rng(1337);
		std::uniform_real_distribution<float> unit(0.0f, 1.0f);
		auto U = [&](float a, float b) { return a + (b - a) * unit(rng); };
		auto yaw = [&] { return glm::angleAxis(U(0.0f, glm::two_pi<float>()), glm::vec3(0, 1, 0)); };

		// Moves a point outward until it finds solid ground above `minHeight`.
		auto snapToLand = [&](glm::vec2 p, float minHeight)
		{
			for (float r = 0.0f; r < 120.0f; r += 4.0f)
				for (int k = 0; k < 8; ++k)
				{
					const float a = glm::two_pi<float>() * static_cast<float>(k) / 8.0f;
					const glm::vec2 q = p + glm::vec2(std::cos(a), std::sin(a)) * r;
					if (glm::length(q) < Terrain::kWalkRadius - 20.0f && heightSource.Height(q.x, q.y) >= minHeight)
						return q;
				}
			return p;
		};

		add("Terrain", {0, 0, 0}, glm::vec3(1.0f), terrain, grass);
		add("Sea", {0, Terrain::kSeaLevel, 0}, glm::vec3(1.0f), sea, water);

		// Moonlight: a directional light shining along the object's -Z axis (matches the moon in the sky).
		{
			auto &moon = add("Moon", {0, 0, 0}, glm::vec3(1.0f), -1, -1);
			moon.rotation = glm::quatLookAt(glm::normalize(glm::vec3(0.4f, -0.8f, -0.3f)), glm::vec3(0, 1, 0));
			moon.script = kScriptLight;
			moon.scriptParams = {2.0f, 0.35f, 0.42f, 0.65f, 1.0f, 0.0f, 0.0f, 0.0f};
		}
		{
			auto &rain = add("Rain", {0, 0, 0}, glm::vec3(1.0f), -1, -1);
			rain.script = kScriptRain;
			rain.scriptParams = {1.0f, 2.5f, 1.0f};
		}
		{
			auto &wind = add("Wind", {0, 0, 0}, glm::vec3(1.0f), -1, -1);
			wind.script = kScriptWind;
			wind.scriptParams = {0.4f};
		}

		const glm::vec2 startXZ = snapToLand({-150.0f, 95.0f}, 4.0f);
		const glm::vec2 beaconXZ = snapToLand({160.0f, -110.0f}, 4.0f);
		constexpr float kKeepOut = 24.0f; // radius around the beacon reserved for the compound

		// Player: params = yaw, pitch (degrees); faces the beacon. Starts unarmed.
		{
			auto &p = add("Player", {startXZ.x, heightSource.Height(startXZ.x, startXZ.y) + 2.0f, startXZ.y}, glm::vec3(1.0f), -1, -1);
			p.hasBody = true;
			p.halfExtents = {0.35f, 0.9f, 0.35f};
			p.health = 100.0f;
			p.script = kScriptPlayer;
			p.scriptParams = {glm::degrees(std::atan2(beaconXZ.y - startXZ.y, beaconXZ.x - startXZ.x)), 0.0f};
		}

		// Lamp head with a flickering point light.
		int lampSeed = 1;
		auto lamp = [&](glm::vec3 pos, float intensity, float radius, float flicker, glm::vec3 color)
		{
			auto &l = add("Lamp", pos, {0.5f, 0.22f, 0.5f}, cube, lampMat);
			l.script = kScriptLightPost;
			l.scriptParams = {intensity, radius, flicker, static_cast<float>(lampSeed++), color.r, color.g, color.b};
		};
		const glm::vec3 sodium(1.0f, 0.72f, 0.4f);
		auto post = [&](glm::vec2 xz, float flicker)
		{
			const float h = heightSource.Height(xz.x, xz.y);
			solid("LightPost", {xz.x, h + 2.5f, xz.y}, {0.2f, 5.0f, 0.2f}, poleMat);
			lamp({xz.x, h + 5.1f, xz.y}, 140.0f, 30.0f, flicker, sodium);
		};
		int postsPlaced = 0;
		auto placePost = [&](glm::vec2 xz)
		{
			if (postsPlaced++ < params.posts)
				post(xz, U(0.15f, 1.0f));
		};

		// Beacon: params = trigger radius.
		{
			auto &b = add("Beacon", {beaconXZ.x, heightSource.Height(beaconXZ.x, beaconXZ.y) + 7.0f, beaconXZ.y}, {0.8f, 14.0f, 0.8f}, cube, beaconMat);
			b.script = kScriptEscape;
			b.scriptParams = {6.0f};

			const glm::vec2 dir = glm::normalize(beaconXZ);
			const glm::vec2 boatXZ = dir * (Terrain::kWalkRadius - 5.0f);
			auto &boat = add("Boat", {boatXZ.x, 0.0f, boatXZ.y}, {9.0f, 1.2f, 3.5f}, cube, wood);
			boat.rotation = glm::angleAxis(std::atan2(-dir.y, dir.x), glm::vec3(0, 1, 0));
		}

		// Walled compound around the beacon: a lifting gate on the south side, a sliding door on the east side.
		{
			constexpr float kHalf = 9.0f;
			constexpr float kThick = 1.0f;
			constexpr float kGateW = 5.0f, kGateH = 4.5f, kGateLift = 2.5f;
			constexpr float kDoorW = 2.4f, kDoorH = 2.8f;

			const auto height = [&](float x, float z) { return heightSource.Height(x, z); };
			float minH = 1e9f, maxH = -1e9f;
			for (float t = -kHalf; t <= kHalf; t += 1.5f)
				for (const glm::vec2 q : {glm::vec2(t, -kHalf), glm::vec2(t, kHalf), glm::vec2(-kHalf, t), glm::vec2(kHalf, t)})
				{
					const float h = height(beaconXZ.x + q.x, beaconXZ.y + q.y);
					minH = std::min(minH, h);
					maxH = std::max(maxH, h);
				}
			const float bottom = minH - 1.5f, top = maxH + 6.0f;
			const float wallH = top - bottom, wallY = (top + bottom) * 0.5f;

			const glm::vec2 C = beaconXZ;
			auto wallX = [&](float z, float x0, float x1) { solid("Wall", {(x0 + x1) * 0.5f, wallY, z}, {x1 - x0, wallH, kThick}, wall); };
			auto wallZ = [&](float x, float z0, float z1) { solid("Wall", {x, wallY, (z0 + z1) * 0.5f}, {kThick, wallH, z1 - z0}, wall); };

			wallX(C.y - kHalf, C.x - kHalf - 0.5f, C.x + kHalf + 0.5f); // north
			wallZ(C.x - kHalf, C.y - kHalf + 0.5f, C.y + kHalf - 0.5f); // west
			wallZ(C.x + kHalf, C.y - kHalf + 0.5f, C.y - kDoorW * 0.5f);
			wallZ(C.x + kHalf, C.y + kDoorW * 0.5f, C.y + kHalf - 0.5f);
			wallX(C.y + kHalf, C.x - kHalf - 0.5f, C.x - kGateW * 0.5f);
			wallX(C.y + kHalf, C.x + kGateW * 0.5f, C.x + kHalf + 0.5f);

			// Gate (south): params = open offset xyz, open time.
			{
				const float gx = C.x, gz = C.y + kHalf;
				const float ground = std::max({height(gx - kGateW * 0.5f, gz), height(gx, gz), height(gx + kGateW * 0.5f, gz)});
				const float gateBottom = ground - 0.3f;
				auto &g = solid("Gate", {gx, gateBottom + kGateH * 0.5f, gz}, {kGateW, kGateH, 0.5f}, gateMat);
				g.script = kScriptDoor;
				g.scriptParams = {0.0f, kGateH * 0.5f + kGateLift, 0.0f, 2.5f};
				const float lintelBottom = gateBottom + kGateH;
				solid("Wall", {gx, (lintelBottom + top) * 0.5f, gz}, {kGateW, top - lintelBottom, kThick}, wall);
			}

			// Door (east): slides along the wall into the neighbouring segment.
			{
				const float dx = C.x + kHalf, dz = C.y;
				const float ground = std::max({height(dx, dz - kDoorW * 0.5f), height(dx, dz), height(dx, dz + kDoorW * 0.5f)});
				const float doorBottom = ground - 0.3f;
				auto &d = solid("Door", {dx, doorBottom + kDoorH * 0.5f, dz}, {0.4f, kDoorH, kDoorW}, doorMat);
				d.script = kScriptDoor;
				d.scriptParams = {0.0f, 0.0f, kDoorW + 0.1f, 1.2f};
				const float lintelBottom = doorBottom + kDoorH;
				solid("Wall", {dx, (lintelBottom + top) * 0.5f, dz}, {kThick, top - lintelBottom, kDoorW}, wall);
			}

			// Lit corners and entrances (flickering low, so the goal stays readable).
			for (const glm::vec2 corner : {glm::vec2(-1, -1), glm::vec2(1, -1), glm::vec2(-1, 1), glm::vec2(1, 1)})
				post(C + corner * (kHalf + 2.5f), 0.25f);
		}

		// Enterable buildings: four walls with a sliding door, roof, glowing window, a failing ceiling lamp
		// and supplies inside.
		const glm::vec2 buildingSpots[] = {{-110.0f, 60.0f}, {-30.0f, -60.0f}, {70.0f, 90.0f}, {120.0f, -30.0f}, {-70.0f, -130.0f}};
		std::vector<glm::vec2> buildingCenters;
		std::vector<glm::vec2> interiorSpots; // two per building
		for (int i = 0; i < params.buildings && i < 5; ++i)
		{
			const glm::vec2 c = snapToLand(buildingSpots[i], 4.0f);
			if (glm::distance(c, beaconXZ) < kKeepOut + 10.0f)
				continue;
			buildingCenters.push_back(c);

			constexpr float kWall = 0.4f, kDoorW = 1.6f, kDoorH = 2.2f;
			const float w = U(7.0f, 10.0f), d = U(5.0f, 7.0f), hgt = U(3.2f, 4.2f);
			const float xL = c.x - w * 0.5f, xR = c.x + w * 0.5f;
			const float zBack = c.y - d * 0.5f, zFront = c.y + d * 0.5f;

			float minH = 1e9f, maxH = -1e9f;
			for (const glm::vec2 o : {glm::vec2(-1, -1), glm::vec2(1, -1), glm::vec2(-1, 1), glm::vec2(1, 1), glm::vec2(0, 0)})
			{
				const float h = heightSource.Height(c.x + o.x * w * 0.5f, c.y + o.y * d * 0.5f);
				minH = std::min(minH, h);
				maxH = std::max(maxH, h);
			}
			const float bottom = minH - 0.8f, top = maxH + hgt;
			const float wallH = top - bottom, wallY = (top + bottom) * 0.5f;

			solid("Wall", {c.x, wallY, zBack}, {w + kWall, wallH, kWall}, buildingMat);
			solid("Wall", {xL, wallY, c.y}, {kWall, wallH, d}, buildingMat);
			solid("Wall", {xR, wallY, c.y}, {kWall, wallH, d}, buildingMat);

			// Front wall: two segments around the door opening and a lintel above it.
			const float doorX = c.x - w * 0.2f;
			const float leftEnd = doorX - kDoorW * 0.5f, rightStart = doorX + kDoorW * 0.5f;
			const float frontL = xL - kWall * 0.5f, frontR = xR + kWall * 0.5f;
			solid("Wall", {(frontL + leftEnd) * 0.5f, wallY, zFront}, {leftEnd - frontL, wallH, kWall}, buildingMat);
			solid("Wall", {(rightStart + frontR) * 0.5f, wallY, zFront}, {frontR - rightStart, wallH, kWall}, buildingMat);

			const float doorGround = std::max({heightSource.Height(doorX - 0.8f, zFront), heightSource.Height(doorX, zFront), heightSource.Height(doorX + 0.8f, zFront)});
			const float doorBottom = doorGround - 0.1f;
			auto &door = solid("Door", {doorX, doorBottom + kDoorH * 0.5f, zFront}, {kDoorW, kDoorH, 0.2f}, doorMat);
			door.script = kScriptDoor;
			door.scriptParams = {kDoorW + 0.1f, 0.0f, 0.0f, 1.0f}; // slides into the wall segment beside it
			const float lintelBottom = doorBottom + kDoorH;
			solid("Wall", {doorX, (lintelBottom + top) * 0.5f, zFront}, {kDoorW, top - lintelBottom, kWall}, buildingMat);

			add("Roof", {c.x, top + 0.25f, c.y}, {w + 1.2f, 0.5f, d + 1.2f}, cube, roofMat);
			add("Window", {xR - 1.4f, doorGround + 1.8f, zFront + kWall * 0.5f + 0.02f}, {1.3f, 0.9f, 0.12f}, cube, windowMat);
			lamp({c.x, top - 0.4f, c.y}, 70.0f, 14.0f, 0.9f, sodium);
			lamp({doorX + 1.3f, doorGround + 3.0f, zFront + 0.5f}, 90.0f, 22.0f, 0.9f, sodium);

			interiorSpots.push_back({c.x + w * 0.25f, c.y - d * 0.2f});
			interiorSpots.push_back({c.x - w * 0.3f, c.y - d * 0.25f});
			placePost({xR + 3.0f, zFront + 3.0f});
		}

		// Lit area at the spawn so the first moments are not pitch black.
		placePost(startXZ + glm::vec2(8.0f, 5.0f));
		placePost(startXZ + glm::vec2(-6.0f, 9.0f));

		auto randomLandPoint = [&](float minHeight, float minFromStart)
		{
			for (;;)
			{
				const float a = U(0.0f, glm::two_pi<float>());
				const float r = std::sqrt(unit(rng)) * (Terrain::kWalkRadius - 15.0f);
				const glm::vec2 p{std::cos(a) * r, std::sin(a) * r};
				if (heightSource.Height(p.x, p.y) < minHeight)
					continue;
				if (glm::distance(p, startXZ) < minFromStart || glm::distance(p, beaconXZ) < kKeepOut)
					continue;
				bool blocked = false;
				for (const glm::vec2 &b : buildingCenters)
					blocked = blocked || glm::distance(p, b) < 12.0f;
				if (!blocked)
					return p;
			}
		};

		while (postsPlaced < params.posts)
			placePost(randomLandPoint(2.0f, 10.0f));

		for (int i = 0; i < params.trees; ++i)
		{
			const glm::vec2 p = randomLandPoint(1.5f, 15.0f);
			// const int v = i % kTreeVariants;
			const float scale = U(0.8f, 1.3f);
			// const float H = treeHeight[v] * scale;
			const glm::vec3 pos{p.x, heightSource.Height(p.x, p.y) - .4f, p.y};
			const glm::quat rot = yaw();

			// auto &t = add("Tree", pos, glm::vec3(scale), trunkMesh[v], bark);
			// t.rotation = rot;
			// t.hasBody = true;
			// t.bodyStatic = true;
			// t.halfExtents = {0.7f * scale, H * 0.5f, 0.7f * scale};
			add("Canopy", pos, glm::vec3(scale), tree, -1).rotation = rot;

			// Mesh-less static trunk collider so trees block movement and carve the navigation grid.
			auto &trunk = add("TreeTrunk", pos + glm::vec3(0.0f, 2.5f * scale, 0.0f), glm::vec3(1.0f), -1, -1);
			trunk.hasBody = true;
			trunk.bodyStatic = true;
			trunk.halfExtents = {0.5f * scale, 2.5f * scale, 0.5f * scale};
		}

		// Grass: tufts scattered in clumps on gentle, dry ground. No colliders, so they never touch the nav grid.
		for (int c = 0; c < params.grassClumps; ++c)
		{
			const glm::vec2 centre = randomLandPoint(1.0f, 4.0f);
			for (int k = 0; k < 14; ++k)
			{
				const float a = U(0.0f, glm::two_pi<float>());
				const float r = std::sqrt(unit(rng)) * 4.0f;
				const glm::vec2 p = centre + glm::vec2(std::cos(a), std::sin(a)) * r;
				const float h = heightSource.Height(p.x, p.y);
				if (h < 1.0f)
					continue;
				// Skip steep ground, which the terrain shader shows as bare rock.
				const float slope = std::max(std::abs(heightSource.Height(p.x + 0.5f, p.y) - h), std::abs(heightSource.Height(p.x, p.y + 0.5f) - h)) / 0.5f;
				if (slope > 0.6f)
					continue;
				auto &g = add("Grass", {p.x, h - 0.03f, p.y}, glm::vec3(U(0.8f, 1.5f)), grassMesh[(c + k) % kGrassVariants], grassMat);
				g.rotation = yaw();
			}
		}

		for (int i = 0; i < params.ferns; ++i)
		{
			const glm::vec2 p = randomLandPoint(0.8f, 6.0f);
			auto &f = add("Fern", {p.x, heightSource.Height(p.x, p.y) - 0.05f, p.y}, glm::vec3(U(0.8f, 1.8f)), fernMesh[i % kFernVariants], -1);
			f.rotation = yaw();
			f.script = kScriptPlant;
			f.scriptParams = {3.0f, 60.0f};
		}

		for (int i = 0; i < params.rocks; ++i)
		{
			const glm::vec2 p = randomLandPoint(0.5f, 10.0f);
			const float sz = U(1.2f, 3.7f);
			const glm::vec3 scale{sz, sz * 0.7f, sz};

			// Sit the model's real bottom on the lowest ground under its footprint, sunk slightly so no gap shows.
			const glm::vec3 half = boulderBounds.HalfExtents() * scale;
			const float lowest = std::min(
				{heightSource.Height(p.x, p.y),
				 heightSource.Height(p.x - half.x, p.y - half.z),
				 heightSource.Height(p.x + half.x, p.y - half.z),
				 heightSource.Height(p.x - half.x, p.y + half.z),
				 heightSource.Height(p.x + half.x, p.y + half.z)}
			);
			const float y = lowest - boulderBounds.min.y * scale.y - half.y * 0.15f;

			auto &r = add("Rock", {p.x, y, p.y}, scale, boulder, -1);
			r.hasBody = true;
			r.bodyStatic = true;
			r.halfExtents = half * 0.85f;
		}

		// Weapon pickup params: damage, cooldown, magazine, reserve, automatic (0/1), reload time, range.
		// The pickup's object name becomes the weapon name.
		auto weaponPickup = [&](const char *name, glm::vec2 xz, std::vector<float> wp)
		{
			auto &w = add(name, {xz.x, heightSource.Height(xz.x, xz.y) + 0.9f, xz.y}, {0.9f, 0.2f, 0.25f}, cube, weaponMat);
			w.script = kScriptWeaponPickup;
			w.scriptParams = std::move(wp);
		};
		auto medkit = [&](glm::vec2 xz, int variant)
		{
			auto &m = add("Medkit", {xz.x, heightSource.Height(xz.x, xz.y) + 0.8f, xz.y}, {10.f, 10.f, 10.f}, medkits[variant], medkitMat);
			m.script = kScriptHealthPickup;
			m.scriptParams = {35.0f};
		};

		weaponPickup("Pistol", startXZ + glm::vec2(4.0f, 3.0f), {30.0f, 0.3f, 12.0f, 36.0f, 0.0f, 1.2f, 100.0f});
		medkit(startXZ + glm::vec2(2.0f, 6.0f), 0);
		weaponPickup("Rifle", interiorSpots.empty() ? randomLandPoint(1.0f, 60.0f) : interiorSpots[0], {22.0f, 0.1f, 30.0f, 90.0f, 1.0f, 1.8f, 140.0f});
		for (size_t i = 1; i < interiorSpots.size(); ++i)
		{
			if (i % 2 == 1 || i == 1)
			{
				const glm::vec2 xz = interiorSpots[i];
				auto &c = add("Ammo Crate", {xz.x, heightSource.Height(xz.x, xz.y) + 0.4f, xz.y}, {0.9f, 0.7f, 0.6f}, cube, crateMat);
				c.script = kScriptAmmoCrate;
				c.scriptParams = {24.0f};
			}
			else
			{
				medkit(interiorSpots[i], 0);
			}
		}
		for (int i = 0; i < 3 + params.raptors / 3; ++i)
		{
			medkit(randomLandPoint(1.0f, 20.0f), i % 2);
		}

		// Raptor script params = speed, bite damage.
		for (int i = 0; i < params.raptors; ++i)
		{
			const glm::vec2 p = randomLandPoint(1.0f, 60.0f);
			auto &d = add("Raptor", {p.x, heightSource.Height(p.x, p.y) + 1.1f, p.y}, glm::vec3(1.0f), raptor, raptorMats[i % 2]);
			d.hasBody = true;
			d.halfExtents = {1.1f, 0.75f, 0.45f};
			d.health = 80.0f;
			d.script = kScriptDinosaur;
			d.scriptParams = {8.0f, 12.0f};
		}

		// Herbivore script params = species, speed.
		const int triMesh = s.AddMesh({"triceratops", kMeshTriceratops, {}});
		const int protoMesh = s.AddMesh({"protoceratops", kMeshProtoceratops, {}});
		const int triMat = s.AddMaterial({"triceratops_hide", {0.28f, 0.33f, 0.24f}, 0.0f, 0.7f, {}});
		const int protoMat = s.AddMaterial({"protoceratops_hide", {0.45f, 0.36f, 0.22f}, 0.0f, 0.7f, {}});
		for (int i = 0; i < params.triceratops + params.protoceratops; ++i)
		{
			const bool large = i < params.triceratops;
			const HerbivoreSpecies sp = large ? HerbivoreSpecies::Triceratops : HerbivoreSpecies::Protoceratops;
			const glm::vec3 half = HerbivoreHalfExtents(sp);
			const glm::vec2 p = randomLandPoint(1.0f, 50.0f);
			auto &d = add(large ? "Triceratops" : "Protoceratops", {p.x, heightSource.Height(p.x, p.y) + half.y + 0.3f, p.y}, glm::vec3(1.0f), large ? triMesh : protoMesh, large ? triMat : protoMat);
			d.hasBody = true;
			d.halfExtents = half;
			d.health = large ? 300.0f : 60.0f;
			d.script = kScriptHerbivore;
			d.scriptParams = {large ? 0.0f : 1.0f, large ? 5.5f : 7.5f};
		}

		return s;
	}

	eng::SceneData MakeTestScene()
	{
		IslandParams p;
		p.trees = 40;
		p.ferns = 80;
		p.grassClumps = 60;
		p.rocks = 4;
		p.raptors = 2;
		p.triceratops = 1;
		p.protoceratops = 2;
		p.buildings = 2;
		p.posts = 8;
		return MakeIslandScene(p);
	}

} // namespace game
