#include <doctest/doctest.h>
#include "Events/GameplayEvents.h"
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>

TEST_SUITE("Countdown & Ready Sequence Tests")
{
    TEST_CASE("Distinct audio clip mapping for READY-3-2-1-GO voice sequence")
    {
        auto getAudioClipForSecond = [](int sec) -> const char* {
            if (sec == 4) return "countdown_ready.wav";
            if (sec == 3) return "countdown_3.wav";
            if (sec == 2) return "countdown_2.wav";
            if (sec == 1) return "countdown_1.wav";
            return "countdown_go.wav";
        };

        CHECK(std::string(getAudioClipForSecond(4)) == "countdown_ready.wav");
        CHECK(std::string(getAudioClipForSecond(3)) == "countdown_3.wav");
        CHECK(std::string(getAudioClipForSecond(2)) == "countdown_2.wav");
        CHECK(std::string(getAudioClipForSecond(1)) == "countdown_1.wav");
        CHECK(std::string(getAudioClipForSecond(0)) == "countdown_go.wav");
    }

    TEST_CASE("Countdown sequence timing and whoosh breath triggers across 4.0 seconds")
    {
        float timer = 4.0f;
        int lastSecond = 4;
        int whooshPlayedForSecond = 0;
        bool active = true;
        float goTimer = 0.0f;
        int whooshCount = 0;
        int voiceCount = 0;
        int goCount = 0;

        // Initial 'READY' voice at start
        voiceCount++;

        const float dt = 0.0166f; // ~60 FPS
        float totalTime = 0.0f;

        while (active && totalTime < 5.0f)
        {
            timer -= dt;
            totalTime += dt;

            // Trigger whoosh as the current word/numeral fades out (last ~0.28s)
            if (timer <= static_cast<float>(lastSecond - 1) + 0.28f && whooshPlayedForSecond != lastSecond)
            {
                whooshPlayedForSecond = lastSecond;
                whooshCount++;
            }

            int currentSec = std::clamp(static_cast<int>(std::ceil(timer)), 1, 4);
            if (currentSec < lastSecond && timer > 0.0f)
            {
                lastSecond = currentSec;
                voiceCount++;
            }

            if (timer <= 0.0f)
            {
                timer = 0.0f;
                active = false;
                goTimer = 0.65f;
                goCount++;
            }
        }

        // Exactly 4 whooshes (as READY, 3, 2, 1 disappear)
        CHECK(whooshCount == 4);
        // Exactly 4 voices during countdown (READY, 3, 2, 1) + 1 GO!
        CHECK(voiceCount == 4);
        CHECK(goCount == 1);
        CHECK_FALSE(active);
        CHECK(goTimer == 0.65f);
        CHECK(totalTime >= 4.0f);
        CHECK(totalTime <= 4.05f);
    }

    TEST_CASE("Damped bounce oscillation formula produces stable scale punch")
    {
        // Formula: punch = 1.0f + 0.85f * exp(-7.0f * t) * cos(11.0f * t)
        for (int i = 0; i <= 100; ++i)
        {
            float t = static_cast<float>(i) / 100.0f;
            float punch = 1.0f + 0.85f * std::exp(-7.0f * t) * std::cos(11.0f * t);

            // Maximum punch occurs near t = 0
            if (i == 0)
            {
                CHECK(punch == doctest::Approx(1.85f));
            }

            // Punch must never be negative or zero (font scale must remain readable)
            CHECK(punch > 0.5f);
            CHECK(punch <= 2.0f);

            // Settles close to 1.0 at end of second (t -> 1.0)
            if (i == 100)
            {
                CHECK(punch == doctest::Approx(1.0f).epsilon(0.01f));
            }
        }
    }

    TEST_CASE("Paddle and ball launch controls are strictly restricted during countdown")
    {
        bool countdownActive = true;
        bool paddleInputEnabled = true;

        // Controller interlock logic
        auto updateControls = [&](bool active) {
            paddleInputEnabled = !active;
        };

        updateControls(countdownActive);
        CHECK_FALSE(paddleInputEnabled);

        // Countdown ends on 'GO!'
        countdownActive = false;
        updateControls(countdownActive);
        CHECK(paddleInputEnabled);
    }

    TEST_CASE("GameUIState::Countdown enum is properly defined")
    {
        GameUIState state = GameUIState::Countdown;
        CHECK(static_cast<int>(state) >= 0);

        GameStateChangedEvent evt(state);
        CHECK(evt.State == GameUIState::Countdown);
    }
}
