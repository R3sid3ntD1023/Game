#include "game/IslandScene.h"
#include "game/Terrain.h"
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

        const int grass = s.AddMaterial({"grass", {0.22f, 0.36f, 0.12f}, 0.0f, 0.9f, {}});
        const int water = s.AddMaterial({"sea", {0.05f, 0.25f, 0.4f}, 0.0f, 0.05f, {}});
        const int bark = s.AddMaterial({"bark", {0.3f, 0.2f, 0.1f}, 0.0f, 0.8f, {}});
        const int leaves = s.AddMaterial({"leaves", {0.1f, 0.3f, 0.08f}, 0.0f, 0.7f, {}});
        const int rock = s.AddMaterial({"rock", {0.4f, 0.4f, 0.4f}, 0.0f, 0.6f, {}});
        const int wood = s.AddMaterial({"boat", {0.5f, 0.35f, 0.2f}, 0.0f, 0.5f, {}});
        const int beaconMat = s.AddMaterial({"beacon", {0.1f, 0.1f, 0.1f}, 0.0f, 0.3f, {0.2f, 1.0f, 1.2f}});
        const int dinoMats[2] = {s.AddMaterial({"dino_green", {0.2f, 0.5f, 0.15f}, 0.0f, 0.6f, {}}),
                                 s.AddMaterial({"dino_olive", {0.5f, 0.45f, 0.15f}, 0.0f, 0.6f, {}})};

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

        add("Terrain", {0, 0, 0}, glm::vec3(1.0f), terrain, grass);
        add("Sea", {0, Terrain::kSeaLevel, 0}, glm::vec3(1.0f), sea, water);

        const glm::vec2 startXZ{-62.0f, 40.0f};
        const glm::vec2 beaconXZ{65.0f, -45.0f};

        // Player: params = yaw, pitch (degrees); faces the beacon.
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
                if (glm::distance(p, startXZ) < minFromStart || glm::distance(p, beaconXZ) < 8.0f)
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

        // Dinosaur params = speed, bite damage.
        for (int i = 0; i < dinosaurs; ++i)
        {
            const glm::vec2 p = randomLandPoint(1.0f, 30.0f);
            auto &d = add("Dinosaur", {p.x, Terrain::Height(p.x, p.y) + 1.0f, p.y}, {3.0f, 1.5f, 1.2f}, cube, dinoMats[i % 2]);
            d.hasBody = true;
            d.halfExtents = {1.2f, 0.75f, 1.0f};
            d.health = 100.0f;
            d.script = kScriptDinosaur;
            d.scriptParams = {7.0f, 15.0f};
        }

        return s;
    }

    eng::SceneData MakeTestScene() { return MakeIslandScene(8, 3, 2); }

} // namespace game
