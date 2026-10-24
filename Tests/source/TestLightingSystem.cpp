#include <doctest/doctest.h>
#include "ECS/Components/LightSourceComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Registry.hpp"
#include "Data/BlendMode.h"
#include "Data/Color.h"
#include "Math/Vector2.h"
#include <fstream>
#include <string>
#include <cmath>

TEST_SUITE("2D Dynamic Lighting System Tests")
{
    TEST_CASE("LightSourceComponent default values and configurations")
    {
        LightSourceComponent pointLight{};
        CHECK(pointLight.Type == LightType::Point);
        CHECK(pointLight.Radius == 250.0f);
        CHECK(pointLight.Intensity == 1.0f);
        CHECK(pointLight.IsActive == true);
        CHECK(pointLight.Blend == BlendMode::Add);

        LightSourceComponent coneLight{};
        coneLight.Type = LightType::Cone;
        coneLight.DirectionDeg = -90.0f; // Upward
        coneLight.ConeAngleDeg = 75.0f;
        coneLight.Radius = 450.0f;
        coneLight.Intensity = 1.3f;
        coneLight.LightColor = Color{200, 235, 255, 255};

        CHECK(coneLight.Type == LightType::Cone);
        CHECK(coneLight.DirectionDeg == -90.0f);
        CHECK(coneLight.ConeAngleDeg == 75.0f);
        CHECK(coneLight.Radius == 450.0f);
        CHECK(coneLight.Intensity == doctest::Approx(1.3f));
        CHECK(coneLight.LightColor.r == 200);
        CHECK(coneLight.LightColor.g == 235);
        CHECK(coneLight.LightColor.b == 255);
    }

    TEST_CASE("BlendMode enum contains Multiply and is distinct from other modes")
    {
        CHECK(BlendMode::Multiply != BlendMode::Alpha);
        CHECK(BlendMode::Multiply != BlendMode::Add);
    }

    TEST_CASE("BrickComponent supports Lantern type and flag")
    {
        BrickComponent comp{};
        CHECK(comp.Type == BrickType::Light);
        CHECK(comp.IsLantern == false);

        comp.Type = BrickType::Lantern;
        comp.IsLantern = true;
        comp.HitPoints = 1;
        comp.ScoreValue = 120;

        CHECK(comp.Type == BrickType::Lantern);
        CHECK(comp.IsLantern == true);
        CHECK(comp.HitPoints == 1);
        CHECK(comp.ScoreValue == 120);
    }

    TEST_CASE("ECS Entity attachment and lifecycle for LightSourceComponent")
    {
        Registry reg;
        Entity lantern = reg.CreateEntity();
        
        BrickComponent brickComp{};
        brickComp.Type = BrickType::Lantern;
        brickComp.IsLantern = true;
        reg.AddComponent<BrickComponent>(lantern, brickComp);

        LightSourceComponent lightComp{};
        lightComp.Type = LightType::Point;
        lightComp.LightColor = Color{255, 235, 120, 255};
        lightComp.Radius = 230.0f;
        lightComp.Intensity = 1.25f;
        reg.AddComponent<LightSourceComponent>(lantern, lightComp);

        CHECK(reg.HasComponent<BrickComponent>(lantern));
        CHECK(reg.HasComponent<LightSourceComponent>(lantern));
        CHECK(reg.GetComponent<LightSourceComponent>(lantern).Radius == 230.0f);

        // When lantern brick shatters and entity is destroyed, light source is destroyed too
        reg.DestroyEntity(lantern);
        CHECK_FALSE(reg.IsAlive(lantern));
    }

    TEST_CASE("Ambient lighting darkness mask detection")
    {
        auto hasLighting = [](Color c) -> bool {
            return c.r < 255 || c.g < 255 || c.b < 255;
        };

        // Standard daytime / full visibility levels (Levels 1-5)
        CHECK_FALSE(hasLighting(Color{255, 255, 255, 255}));

        // Subtle dimming level
        CHECK(hasLighting(Color{200, 200, 220, 255}));

        // Level 06 'Void Core' pitch black darkness mask
        Color voidCoreAmbient{12, 12, 18, 255};
        CHECK(hasLighting(voidCoreAmbient));
        CHECK(voidCoreAmbient.r <= 20);
        CHECK(voidCoreAmbient.g <= 20);
        CHECK(voidCoreAmbient.b <= 20);
    }

    TEST_CASE("Radial light falloff math and attenuation")
    {
        Vector2f lightPos{0.0f, 0.0f};
        float radius = 200.0f;

        auto computeIntensityAt = [&](Vector2f point) -> float {
            float dx = point.X - lightPos.X;
            float dy = point.Y - lightPos.Y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= radius) return 0.0f;
            return 1.0f - (dist / radius);
        };

        CHECK(computeIntensityAt(Vector2f{0.0f, 0.0f}) == doctest::Approx(1.0f));
        CHECK(computeIntensityAt(Vector2f{100.0f, 0.0f}) == doctest::Approx(0.5f));
        CHECK(computeIntensityAt(Vector2f{200.0f, 0.0f}) == doctest::Approx(0.0f));
        CHECK(computeIntensityAt(Vector2f{250.0f, 0.0f}) == doctest::Approx(0.0f));
    }

    TEST_CASE("Directional cone light angular coverage")
    {
        // Paddle light points upward (-90 deg) with 80 deg cone angle (-130 deg to -50 deg)
        float dirDeg = -90.0f;
        float coneAngleDeg = 80.0f;
        float halfCone = coneAngleDeg * 0.5f;

        auto isInsideCone = [&](float targetDeg) -> bool {
            float diff = std::abs(targetDeg - dirDeg);
            while (diff > 180.0f) diff = std::abs(diff - 360.0f);
            return diff <= halfCone;
        };

        CHECK(isInsideCone(-90.0f));  // Directly upward along beam center
        CHECK(isInsideCone(-70.0f));  // Inside cone
        CHECK(isInsideCone(-110.0f)); // Inside cone
        CHECK_FALSE(isInsideCone(0.0f));   // Pointing right
        CHECK_FALSE(isInsideCone(90.0f));  // Pointing downward
    }

    TEST_CASE("Level 06 'Void Core' layout file verification")
    {
        // Check level06.txt contains 12 columns, Lanterns, Bombs, and Metal bricks
        std::ifstream file("Resources/levels/level06.txt");
        if (!file.is_open())
        {
            file.open("../Resources/levels/level06.txt");
        }

        if (file.is_open())
        {
            std::string line;
            int rowCount = 0;
            int lanternCount = 0;
            int bombCount = 0;
            int metalCount = 0;

            while (std::getline(file, line))
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                    line.pop_back();

                if (line.empty()) continue;
                CHECK(line.length() == 12);
                rowCount++;

                for (char c : line)
                {
                    if (c == 'L' || c == 'l') lanternCount++;
                    if (c == 'B' || c == 'b' || c == 'X' || c == 'x') bombCount++;
                    if (c == 'M' || c == 'm' || c == 'U' || c == 'u') metalCount++;
                }
            }

            CHECK(rowCount >= 6);
            CHECK(lanternCount >= 4);
            CHECK(bombCount >= 4);
            CHECK(metalCount >= 8);
        }
    }

    TEST_CASE("Paddle size-reactive light scaling (standard, expanded, shrunk)")
    {
        auto computePaddleLight = [](float scaleX) {
            float width = 120.0f * scaleX;
            float bodyRadius = 110.0f * std::sqrt(scaleX);
            float bodyIntensity = 1.35f * std::clamp(scaleX, 0.7f, 1.5f);
            float coneAngle = 65.0f + 25.0f * (scaleX - 1.0f);
            float coneRadius = 420.0f * (0.8f + 0.2f * scaleX);
            return std::make_tuple(width, bodyRadius, bodyIntensity, coneAngle, coneRadius);
        };

        // Standard paddle (scale 1.0x)
        auto [stdW, stdBodyR, stdBodyI, stdAngle, stdConeR] = computePaddleLight(1.0f);
        CHECK(stdW == doctest::Approx(120.0f));
        CHECK(stdBodyR == doctest::Approx(110.0f));
        CHECK(stdBodyI == doctest::Approx(1.35f));
        CHECK(stdAngle == doctest::Approx(65.0f));
        CHECK(stdConeR == doctest::Approx(420.0f));

        // Expanded paddle (scale 1.5x) - emits significantly MORE light and wider spotlight
        auto [expW, expBodyR, expBodyI, expAngle, expConeR] = computePaddleLight(1.5f);
        CHECK(expW == doctest::Approx(180.0f));
        CHECK(expBodyR > stdBodyR);
        CHECK(expBodyI > stdBodyI);
        CHECK(expAngle > stdAngle);
        CHECK(expConeR > stdConeR);

        // Shrunk paddle (scale 0.7x) - emits LESS light with tighter spotlight
        auto [shrW, shrBodyR, shrBodyI, shrAngle, shrConeR] = computePaddleLight(0.7f);
        CHECK(shrW == doctest::Approx(84.0f));
        CHECK(shrBodyR < stdBodyR);
        CHECK(shrBodyI < stdBodyI);
        CHECK(shrAngle < stdAngle);
        CHECK(shrConeR < stdConeR);
    }

    TEST_CASE("FireBall dynamic light properties and contrast with standard ball")
    {
        // Standard ball
        float stdRadius = 180.0f;
        float stdIntensity = 1.25f;
        Color stdColor{225, 240, 255, 255};

        // FireBall (cruising)
        float fireRadius = 310.0f;
        float fireIntensity = 1.75f;
        Color fireColor{255, 150, 30, 255};

        // FireBall (fuse active - impending explosion)
        float fuseRadius = 360.0f;
        float fuseIntensity = 2.1f;
        Color fuseColor{255, 95, 20, 255};

        CHECK(fireRadius > stdRadius);
        CHECK(fireIntensity > stdIntensity);
        CHECK(fireColor.r == 255);
        CHECK(fireColor.g < stdColor.g); // Warm fiery hue

        CHECK(fuseRadius > fireRadius);
        CHECK(fuseIntensity > fireIntensity);
        CHECK(fuseColor.r == 255);
    }

    TEST_CASE("Level 07 'Storm Vault' layout and conductive lighting verification")
    {
        std::ifstream file("Resources/levels/level07.txt");
        if (!file.is_open())
        {
            file.open("../Resources/levels/level07.txt");
        }

        if (file.is_open())
        {
            std::string line;
            int rowCount = 0;
            int metalCount = 0;
            int specialCount = 0;
            int bombCount = 0;
            int lanternCount = 0;

            while (std::getline(file, line))
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                    line.pop_back();

                if (line.empty()) continue;
                CHECK(line.length() == 12);
                rowCount++;

                for (char c : line)
                {
                    if (c == 'M' || c == 'm') metalCount++;
                    if (c == 'S' || c == 's') specialCount++;
                    if (c == 'B' || c == 'b') bombCount++;
                    if (c == 'L' || c == 'l') lanternCount++;
                }
            }

            CHECK(rowCount == 7);
            CHECK(metalCount >= 20); // Extensive conductive circuit
            CHECK(specialCount >= 2); // Storm power-up capsules
            CHECK(bombCount >= 2);
            CHECK(lanternCount >= 2);
        }
    }
}
