#include "AudioMixer.h"
#include "Resources/ResourceManager.h"
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
    TEST_CASE("AudioMixer ducking and volume controls")
    {
        ResourceManager rm;
        AudioMixer mixer(rm);

        CHECK(mixer.IsDuckingEnabled());
        CHECK(mixer.GetDuckingMultiplier() == doctest::Approx(1.0f));

        mixer.TriggerDucking(0.35f, 0.6f);
        CHECK(mixer.GetDuckingMultiplier() <= 0.65f);

        mixer.SetDuckingEnabled(false);
        CHECK_FALSE(mixer.IsDuckingEnabled());
        CHECK(mixer.GetDuckingMultiplier() == doctest::Approx(1.0f));

        mixer.SetSfxVolume(80.0f);
        CHECK(mixer.GetSfxVolume() == doctest::Approx(80.0f));

        mixer.SetMusicVolume(70.0f);
        CHECK(mixer.GetMusicVolume() == doctest::Approx(70.0f));
    }

    TEST_CASE("Load startup audio assets into SoundBuffer and Music")
    {
        sf::SoundBuffer buf;
        CHECK(buf.loadFromFile("Resources/audio/sfx/ui_click.wav"));
        CHECK(buf.loadFromFile("Resources/audio/sfx/paddle_hit.wav"));

        sf::Music music;
        CHECK(music.openFromFile("Resources/audio/music/Game-menu.ogg"));
    }

    TEST_CASE("AudioMixer centralized music gain synchronization & seamless same-track reuse")
    {
        ResourceManager rm;
        AudioMixer mixer(rm);
        mixer.SetDuckingEnabled(false);

        mixer.SetMasterVolume(100.0f);
        mixer.SetMusicVolume(100.0f);
        CHECK(mixer.GetEffectiveMusicGain() == doctest::Approx(60.0f)); // 100 * 0.6 bus headroom

        mixer.SetMasterVolume(80.0f);
        mixer.SetMusicVolume(50.0f);
        CHECK(mixer.GetEffectiveMusicGain() == doctest::Approx(24.0f)); // 100 * 0.6 * 0.8 * 0.5

        mixer.PlayMusic("Resources/audio/music/Game-1.ogg");
        CHECK(mixer.GetCurrentMusicPath() == "Resources/audio/music/Game-1.ogg");
        CHECK(mixer.GetMusicLevel() == doctest::Approx(AudioMixer::kNominalMusicLevel));
        CHECK(mixer.GetEffectiveMusicGain() == doctest::Approx(24.0f));

        // Seamless mute toggle (Edit/Play environment mute) keeps track path & updates gain to 0
        mixer.PlayMusic("Resources/audio/music/Game-1.ogg", 0.0f, true);
        CHECK(mixer.GetCurrentMusicPath() == "Resources/audio/music/Game-1.ogg");
        CHECK(mixer.GetMusicLevel() == doctest::Approx(0.0f));
        CHECK(mixer.GetEffectiveMusicGain() == doctest::Approx(0.0f));

        // Unmuting restores the exact centralized gain without changing tracks
        mixer.PlayMusic("Resources/audio/music/Game-1.ogg", AudioMixer::kNominalMusicLevel, true);
        CHECK(mixer.GetMusicLevel() == doctest::Approx(AudioMixer::kNominalMusicLevel));
        CHECK(mixer.GetEffectiveMusicGain() == doctest::Approx(24.0f));

        mixer.StopMusic();
    }
}
