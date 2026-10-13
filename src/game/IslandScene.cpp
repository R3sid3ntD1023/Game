#include "game/IslandScene.h"
#include "game/Terrain.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>

namespace game
{

    eng::SceneData MakeIslandScene(int trees, int rocks, int dinosaurs)
    {
        eng::SceneData s;

        const int cube = s.AddMesh({"cube", "cube", {0.5f}});
        const int sphere = s.AddMesh({"sphere", "sphere", {0.5f}});
        const int terrain = s.AddMesh({"terrain", kMeshTerrain, {160.0f}});
        const int sea = s.AddMesh({"sea", "plane", {300.0f, 1.0f}});
        const int raptor = s.AddMesh({"raptor", kMeshRaptor, {}});

        const int grass = s.AddMaterial({"grass", {0.22f, 0.36f, 0.12f}, 0.0f, 0.9f, {}});
        const int water = s.AddMaterial({"sea", {0.05f, 0.25f, 0.4f}, 0.0f, 0.05f, {}});
        const int bark = s.AddMaterial({"bark", {0.3f, 0.2f, 0.1f}, 0.0f, 0.8f, {}});
        const int leaves = s.AddMaterial({"leaves", {0.1f, 0.3f, 0.08f}, 0.0f, 0.7f, {}});
        const int rock = s.AddMaterial({"rock", {0.4f, 0.4f, 0.4f}, 0.0f, 0.6f, {}});
        const int wood = s.AddMaterial({"boat", {0.5f, 0.35f, 0.2f}, 0.0f, 0.5f, {}});
        const int beaconMat = s.AddMaterial({"beacon", {0.1f, 0.1f, 0.1f}, 0.0f, 0.3f, {0.2f, 1.0f, 1.2f}});
        const int wall = s.AddMaterial({"wall", {0.45f, 0.42f, 0.38f}, 0.0f, 0.9f, {}});
        const int doorMat = s.AddMaterial({"door", {0.3f, 0.2f, 0.12f}, 0.0f, 0.7f, {}});
        const int gateMat = s.AddMaterial({"gate", {0.25f, 0.27f, 0.3f}, 0.8f, 0.4f, {}});
        const int medkitMat = s.AddMaterial({"medkit", {0.9f, 0.9f, 0.9f}, 0.0f, 0.5f, {0.1f, 1.2f, 0.3f}});
        const int weaponMat = s.AddMaterial({"weapon", {0.12f, 0.12f, 0.14f}, 0.9f, 0.3f, {1.2f, 0.7f, 0.1f}});
        const int raptorMats[2] = {s.AddMaterial({"raptor_green", {0.2f, 0.4f, 0.15f}, 0.0f, 0.6f, {}}),
                                   s.AddMaterial({"raptor_rust", {0.5f, 0.28f, 0.12f}, 0.0f, 0.6f, {}})};

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

        add("Terrain", {0, 0, 0}, glm::vec3(1.0f), terrain, grass);
        add("Sea", {0, Terrain::kSeaLevel, 0}, glm::vec3(1.0f), sea, water);

        const glm::vec2 startXZ{-62.0f, 40.0f};
        const glm::vec2 beaconXZ{65.0f, -45.0f};
        constexpr float kKeepOut = 16.0f; // radius around the beacon reserved for the compound

        // Player: params = yaw, pitch (degrees); faces the beacon. Starts unarmed.
        {
            auto &p = add("Player", {startXZ.x, Terrain::Height(startXZ.x, startXZ.y) + 2.0f, startXZ.y}, glm::vec3(1.0f), -1, -1);
            p.hasBody = true;
            p.halfExtents = {0.35f, 0.9f, 0.35f};
            p.health = 100.0f;
            p.script = kScriptPlayer;
            p.scriptParams = {glm::degrees(std::atan2(beaconXZ.y - startXZ.y, beaconXZ.x - startXZ.x)), 0.0f};
        }

        // Beacon: params = trigger radius.
        {
            auto &b = add("Beacon", {beaconXZ.x, Terrain::Height(beaconXZ.x, beaconXZ.y) + 7.0f, beaconXZ.y}, {0.8f, 14.0f, 0.8f}, cube, beaconMat);
            b.script = kScriptEscape;
            b.scriptParams = {6.0f};

            const glm::vec2 dir = glm::normalize(beaconXZ);
            const glm::vec2 boatXZ = dir * 95.0f;
            auto &boat = add("Boat", {boatXZ.x, 0.0f, boatXZ.y}, {9.0f, 1.2f, 3.5f}, cube, wood);
            boat.rotation = glm::angleAxis(std::atan2(-dir.y, dir.x), glm::vec3(0, 1, 0));
        }

        // Walled compound around the beacon: a lifting gate on the south side, a sliding door on the east side.
        {
            constexpr float kHalf = 9.0f;
            constexpr float kThick = 1.0f;
            constexpr float kGateW = 5.0f, kGateH = 4.5f, kGateLift = 2.5f;
            constexpr float kDoorW = 2.4f, kDoorH = 2.8f;

            const auto height = [](float x, float z)
            { return Terrain::Height(x, z); };
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
            auto wallX = [&](float z, float x0, float x1)
            { solid("Wall", {(x0 + x1) * 0.5f, wallY, z}, {x1 - x0, wallH, kThick}, wall); };
            auto wallZ = [&](float x, float z0, float z1)
            { solid("Wall", {x, wallY, (z0 + z1) * 0.5f}, {kThick, wallH, z1 - z0}, wall); };

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
        }

        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        auto randomLandPoint = [&](float minHeight, float minFromStart)
        {
            for (;;)
            {
                const float a = unit(rng) * glm::two_pi<float>();
                const float r = std::sqrt(unit(rng)) * 85.0f;
                const glm::vec2 p{std::cos(a) * r, std::sin(a) * r};
                if (Terrain::Height(p.x, p.y) < minHeight)
                    continue;
                if (glm::distance(p, startXZ) < minFromStart || glm::distance(p, beaconXZ) < kKeepOut)
                    continue;
                return p;
            }
        };

        for (int i = 0; i < trees; ++i)
        {
            const glm::vec2 p = randomLandPoint(1.0f, 12.0f);
            const float h = Terrain::Height(p.x, p.y);
            const float trunk = 4.0f + unit(rng) * 3.0f;
            auto &t = add("Trunk", {p.x, h + trunk * 0.5f, p.y}, {0.6f, trunk, 0.6f}, cube, bark);
            t.hasBody = true;
            t.bodyStatic = true;
            t.halfExtents = {0.3f, trunk * 0.5f, 0.3f};

            const float crown = 3.0f + unit(rng) * 2.0f;
            add("Crown", {p.x, h + trunk + crown * 0.3f, p.y}, glm::vec3(crown), sphere, leaves);
        }

        for (int i = 0; i < rocks; ++i)
        {
            const glm::vec2 p = randomLandPoint(0.5f, 10.0f);
            const float sz = 1.2f + unit(rng) * 2.5f;
            auto &r = add("Rock", {p.x, Terrain::Height(p.x, p.y) + sz * 0.25f, p.y}, {sz, sz * 0.7f, sz}, sphere, rock);
            r.hasBody = true;
            r.bodyStatic = true;
            r.halfExtents = {sz * 0.35f, sz * 0.25f, sz * 0.35f};
        }

        // Weapon pickup params: damage, cooldown, magazine, reserve, automatic (0/1), reload time, range.
        // The pickup's object name becomes the weapon name.
        auto weaponPickup = [&](const char *name, glm::vec2 xz, std::vector<float> params)
        {
            auto &w = add(name, {xz.x, Terrain::Height(xz.x, xz.y) + 0.9f, xz.y}, {0.9f, 0.2f, 0.25f}, cube, weaponMat);
            w.script = kScriptWeaponPickup;
            w.scriptParams = std::move(params);
        };
        auto medkit = [&](glm::vec2 xz)
        {
            auto &m = add("Medkit", {xz.x, Terrain::Height(xz.x, xz.y) + 0.8f, xz.y}, {0.5f, 0.35f, 0.5f}, cube, medkitMat);
            m.script = kScriptHealthPickup;
            m.scriptParams = {35.0f};
        };

        weaponPickup("Pistol", startXZ + glm::vec2(4.0f, 3.0f), {30.0f, 0.3f, 12.0f, 36.0f, 0.0f, 1.2f, 100.0f});
        medkit(startXZ + glm::vec2(2.0f, 6.0f));
        weaponPickup("Rifle", randomLandPoint(1.0f, 30.0f), {22.0f, 0.1f, 30.0f, 90.0f, 1.0f, 1.8f, 140.0f});
        for (int i = 0; i < 2 + dinosaurs / 3; ++i)
            medkit(randomLandPoint(1.0f, 20.0f));

        // Raptor script params = speed, bite damage.
        for (int i = 0; i < dinosaurs; ++i)
        {
            const glm::vec2 p = randomLandPoint(1.0f, 30.0f);
            auto &d = add("Raptor", {p.x, Terrain::Height(p.x, p.y) + 1.1f, p.y}, glm::vec3(1.0f), raptor, raptorMats[i % 2]);
            d.hasBody = true;
            d.halfExtents = {1.1f, 0.75f, 0.45f};
            d.health = 80.0f;
            d.script = kScriptDinosaur;
            d.scriptParams = {8.0f, 12.0f};
        }

        return s;
    }

    eng::SceneData MakeTestScene() { return MakeIslandScene(8, 3, 2); }

} // namespace game
