#include <doctest/doctest.h>
#include "ECS/Components/TrailComponent.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BallComponent.h"
#include <cmath>

TEST_SUITE("Ball Trail VFX & Customization Tests")
{
    TEST_CASE("TrailPresets validation for all cosmetic archetypes")
    {
        TrailCosmeticId archetypes[] = {
            TrailCosmeticId::DefaultNeon,
            TrailCosmeticId::ElectricCyan,
            TrailCosmeticId::FireStreak,
            TrailCosmeticId::ElectricViolet,
            TrailCosmeticId::EmeraldLaser,
            TrailCosmeticId::GoldenSun,
            TrailCosmeticId::Rainbow
        };

        for (auto id : archetypes)
        {
            TrailConfig cfg = TrailPresets::GetPreset(id);
            CHECK(cfg.CosmeticId == id);
            CHECK(cfg.Lifetime > 0.1f);
            CHECK(cfg.Lifetime <= 1.0f);
            CHECK(cfg.StartWidth >= cfg.EndWidth);
            CHECK(cfg.EndWidth >= 0.0f);
            CHECK(cfg.MinPointDistance > 0.0f);
            CHECK(cfg.MaxPointDistance > cfg.MinPointDistance);
            CHECK(cfg.Blend == BlendMode::Add);
            CHECK(cfg.FadeWidth == true);
            CHECK(cfg.FadeAlpha == true);
            if (id == TrailCosmeticId::DefaultNeon)
            {
                // Must match the white ball color by default
                CHECK(cfg.StartColor.r == 255);
                CHECK(cfg.StartColor.g == 255);
                CHECK(cfg.StartColor.b == 255);
            }
        }
    }

    TEST_CASE("TrailPresets::HsvToRgb color spectrum generation")
    {
        // Primary colors at 100% saturation and value
        Color red = TrailPresets::HsvToRgb(0.0f, 1.0f, 1.0f);
        CHECK(red.r == 255);
        CHECK(red.g == 0);
        CHECK(red.b == 0);
        CHECK(red.a == 255);

        Color green = TrailPresets::HsvToRgb(120.0f, 1.0f, 1.0f);
        CHECK(green.r == 0);
        CHECK(green.g == 255);
        CHECK(green.b == 0);

        Color blue = TrailPresets::HsvToRgb(240.0f, 1.0f, 1.0f);
        CHECK(blue.r == 0);
        CHECK(blue.g == 0);
        CHECK(blue.b == 255);

        // Midpoint yellow
        Color yellow = TrailPresets::HsvToRgb(60.0f, 1.0f, 1.0f);
        CHECK(yellow.r == 255);
        CHECK(yellow.g == 255);
        CHECK(yellow.b == 0);
    }

    TEST_CASE("TrailComponent lifecycle and buffer reset")
    {
        TrailComponent tc;
        CHECK(tc.Points.empty());
        CHECK_FALSE(tc.HasRecordedFirst);
        CHECK(tc.LastRecordedPos.X == 0.0f);
        CHECK(tc.LastRecordedPos.Y == 0.0f);

        // Push test points
        tc.Points.push_front(TrailPoint{Vector2f{10.0f, 20.0f}, 0.0f, 18.0f, Colors::White});
        tc.Points.push_front(TrailPoint{Vector2f{15.0f, 25.0f}, 0.05f, 16.0f, Colors::White});
        tc.HasRecordedFirst = true;
        tc.LastRecordedPos = Vector2f{15.0f, 25.0f};

        CHECK(tc.Points.size() == 2);
        CHECK(tc.HasRecordedFirst);

        // Reset
        tc.Clear();
        CHECK(tc.Points.empty());
        CHECK_FALSE(tc.HasRecordedFirst);
        CHECK(tc.LastRecordedPos.X == 0.0f);
        CHECK(tc.LastRecordedPos.Y == 0.0f);
    }

    TEST_CASE("Trail point aging and pruning logic")
    {
        TrailComponent tc;
        tc.Config = TrailPresets::GetPreset(TrailCosmeticId::DefaultNeon);
        const float lifetime = tc.Config.Lifetime;

        tc.Points.push_back(TrailPoint{Vector2f{0.0f, 0.0f}, lifetime - 0.05f, 10.0f, Colors::White});
        tc.Points.push_back(TrailPoint{Vector2f{10.0f, 0.0f}, lifetime + 0.01f, 8.0f, Colors::White});

        // Pruning simulation:
        while (!tc.Points.empty() && tc.Points.back().Age >= tc.Config.Lifetime)
        {
            tc.Points.pop_back();
        }
        CHECK(tc.Points.size() == 1);

        // Age by another 0.1f -> remaining point expires
        tc.Points.back().Age += 0.1f;
        while (!tc.Points.empty() && tc.Points.back().Age >= tc.Config.Lifetime)
        {
            tc.Points.pop_back();
        }
        CHECK(tc.Points.empty());
    }

    TEST_CASE("Stationary / docked ball gracefully shrinks and dissolves trail")
    {
        TrailComponent tc;
        tc.Config = TrailPresets::GetPreset(TrailCosmeticId::DefaultNeon);

        // Ball emitted 3 points while moving
        tc.Points.push_front(TrailPoint{Vector2f{30.0f, 0.0f}, 0.0f, 18.0f, Colors::White});
        tc.Points.push_front(TrailPoint{Vector2f{20.0f, 0.0f}, 0.05f, 14.0f, Colors::White});
        tc.Points.push_front(TrailPoint{Vector2f{10.0f, 0.0f}, 0.10f, 10.0f, Colors::White});
        tc.HasRecordedFirst = true;
        tc.LastRecordedPos = Vector2f{30.0f, 0.0f};

        // When docked on paddle, ball velocity is 0 and no new points are pushed.
        // Simulate frames advancing by 0.1s:
        float dt = 0.1f;
        for (int frame = 0; frame < 5; ++frame)
        {
            for (auto& pt : tc.Points)
            {
                pt.Age += dt;
            }
            while (!tc.Points.empty() && tc.Points.back().Age >= tc.Config.Lifetime)
            {
                tc.Points.pop_back();
            }
        }

        // All points aged beyond lifetime (0.22s) and dissolved completely
        CHECK(tc.Points.empty());
    }

    TEST_CASE("High velocity interpolation prevents trail gaps")
    {
        TrailComponent tc;
        tc.Config = TrailPresets::GetPreset(TrailCosmeticId::DefaultNeon);
        tc.HasRecordedFirst = true;
        tc.LastRecordedPos = Vector2f{0.0f, 0.0f};

        // Ball jumps 120 pixels in one frame (fast moving ball)
        Vector2f nextPos{120.0f, 0.0f};
        Vector2f delta = nextPos - tc.LastRecordedPos;
        float dist = delta.Length();

        CHECK(dist > tc.Config.MaxPointDistance);

        int steps = static_cast<int>(std::floor(dist / tc.Config.MaxPointDistance));
        steps = std::min(steps, 4);
        CHECK(steps >= 2);

        for (int s = 1; s <= steps; ++s)
        {
            float alpha = static_cast<float>(s) / static_cast<float>(steps + 1);
            Vector2f subPos = tc.LastRecordedPos + delta * alpha;
            tc.Points.push_front(TrailPoint{subPos, 0.0f, tc.Config.StartWidth, tc.Config.StartColor});
        }
        tc.Points.push_front(TrailPoint{nextPos, 0.0f, tc.Config.StartWidth, tc.Config.StartColor});

        // 1 original + steps intermediate points inserted
        CHECK(tc.Points.size() == static_cast<size_t>(steps + 1));
    }
}
