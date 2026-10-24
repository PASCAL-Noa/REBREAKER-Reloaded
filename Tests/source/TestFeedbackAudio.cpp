#include <doctest/doctest.h>
#include <string>
#include <fstream>
#include "ECS/Components/UI/ButtonComponent.h"

#include "ECS/Systems/UISystem.h"
#include "ECS/Components/PowerUpComponent.h"

TEST_SUITE("UI Feedback & Audio Tests")
{
    TEST_CASE("ButtonComponent default audio state")
    {
        ButtonComponent button;
        CHECK(button.HoverSoundId == 0);
        CHECK(button.ClickSoundId == 0);
        CHECK_FALSE(button.SuppressUniversalSfx);
        CHECK(button.State == ButtonState::Normal);
    }

    TEST_CASE("UISystem default audio configuration")
    {
        // Store previous
        uint32_t prevHover = UISystem::GetDefaultHoverSound();
        uint32_t prevClick = UISystem::GetDefaultClickSound();

        UISystem::SetDefaultAudio(101, 202);
        CHECK(UISystem::GetDefaultHoverSound() == 101);
        CHECK(UISystem::GetDefaultClickSound() == 202);

        // Restore
        UISystem::SetDefaultAudio(prevHover, prevClick);
        CHECK(UISystem::GetDefaultHoverSound() == prevHover);
        CHECK(UISystem::GetDefaultClickSound() == prevClick);
    }

    TEST_CASE("ButtonComponent custom audio override")
    {
        ButtonComponent btn;
        btn.HoverSoundId = 55;
        btn.ClickSoundId = 99;
        btn.SuppressUniversalSfx = true;

        CHECK(btn.HoverSoundId == 55);
        CHECK(btn.ClickSoundId == 99);
        CHECK(btn.SuppressUniversalSfx);
    }

    TEST_CASE("PowerUpType enum validation and archetype categories")
    {
        // Ensure all power-up types exist and can be categorized
        PowerUpType offensive1 = PowerUpType::LaserPaddle;
        PowerUpType offensive2 = PowerUpType::FireBall;
        PowerUpType buff1 = PowerUpType::MultiBall;
        PowerUpType buff2 = PowerUpType::ExpandPaddle;
        PowerUpType buff3 = PowerUpType::BigBall;
        PowerUpType hazard = PowerUpType::ShrinkPaddle;
        PowerUpType utility1 = PowerUpType::ExtraLife;
        PowerUpType utility2 = PowerUpType::TempoBall;

        CHECK(offensive1 != offensive2);
        CHECK(buff1 != buff2);
        CHECK(buff2 != buff3);
        CHECK(hazard != utility1);
        CHECK(utility1 != utility2);


        auto getArchetype = [](PowerUpType type) -> const char* {
            switch (type)
            {
                case PowerUpType::LaserPaddle:
                case PowerUpType::FireBall:
                case PowerUpType::GhostBall:
                    return "Offensive";
                case PowerUpType::MultiBall:
                case PowerUpType::ExpandPaddle:
                case PowerUpType::BigBall:
                case PowerUpType::StickyPaddle:
                    return "Buff";
                case PowerUpType::ShrinkPaddle:
                    return "Hazard";
                case PowerUpType::ExtraLife:
                case PowerUpType::TempoBall:
                case PowerUpType::BottomBarrier:
                case PowerUpType::MagnetPaddle:
                    return "Utility";
                default:
                    return "Unknown";
            }
        };

        CHECK(std::string(getArchetype(PowerUpType::LaserPaddle)) == "Offensive");
        CHECK(std::string(getArchetype(PowerUpType::Storm)) == "Offensive");
        CHECK(std::string(getArchetype(PowerUpType::FireBall)) == "Offensive");
        CHECK(std::string(getArchetype(PowerUpType::GhostBall)) == "Offensive");
        CHECK(std::string(getArchetype(PowerUpType::MultiBall)) == "Buff");
        CHECK(std::string(getArchetype(PowerUpType::ExpandPaddle)) == "Buff");
        CHECK(std::string(getArchetype(PowerUpType::BigBall)) == "Buff");
        CHECK(std::string(getArchetype(PowerUpType::StickyPaddle)) == "Buff");
        CHECK(std::string(getArchetype(PowerUpType::ShrinkPaddle)) == "Hazard");
        CHECK(std::string(getArchetype(PowerUpType::ExtraLife)) == "Utility");
        CHECK(std::string(getArchetype(PowerUpType::TempoBall)) == "Utility");
        CHECK(std::string(getArchetype(PowerUpType::BottomBarrier)) == "Utility");
        CHECK(std::string(getArchetype(PowerUpType::MagnetPaddle)) == "Utility");
    }

    TEST_CASE("Storm audio SFX assets verification (lightning.wav and conduction.wav)")
    {
        auto fileExists = [](const std::string& path) -> bool {
            const std::string candidates[] = {
                path,
                "../" + path,
                "../../" + path,
                "bin/" + path,
                "../bin/" + path,
                "../../bin/" + path,
                "bin/Release/" + path,
                "../bin/Release/" + path,
                "../../bin/Release/" + path
            };
            for (const auto& candidate : candidates)
            {
                std::ifstream f(candidate, std::ios::binary);
                if (f.is_open()) return true;
            }
            return false;
        };

        CHECK(fileExists("Resources/audio/sfx/lightning.wav"));
        CHECK(fileExists("Resources/audio/sfx/conduction.wav"));
    }
}
