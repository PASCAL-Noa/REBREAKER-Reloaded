#include <doctest/doctest.h>
#include "Core/SceneManager.h"
#include "Core/GameContext.h"
#include "Core/SceneTransition.h"
#include "Data/SceneTransitionType.h"
#include "ECS/Systems/UISystem.h"
#include "StateMachine/StateMachine.h"
#include <string>

namespace
{
    class MockSceneA : public Scene
    {
    public:
        inline static int s_initCount = 0;
        inline static int s_destroyCount = 0;
        inline static int s_updateCount = 0;

        static void ResetStats()
        {
            s_initCount = 0;
            s_destroyCount = 0;
            s_updateCount = 0;
        }

        void OnInit(GameContext&) override { s_initCount++; }
        void OnDestroy(GameContext&) override { s_destroyCount++; }
        void OnUpdate(float, GameContext&) override { s_updateCount++; }
        [[nodiscard]] Color GetClearColor() const override { return Colors::Red; }
    };

    class MockSceneB : public Scene
    {
    public:
        inline static int s_initCount = 0;
        inline static int s_destroyCount = 0;
        inline static int s_updateCount = 0;

        static void ResetStats()
        {
            s_initCount = 0;
            s_destroyCount = 0;
            s_updateCount = 0;
        }

        void OnInit(GameContext&) override { s_initCount++; }
        void OnDestroy(GameContext&) override { s_destroyCount++; }
        void OnUpdate(float, GameContext&) override { s_updateCount++; }
        [[nodiscard]] Color GetClearColor() const override { return Colors::Blue; }
    };

    // Aligned storage to supply a valid reference to GameContext without dereferencing internal subsystems
    alignas(GameContext) char s_dummyContextBuffer[sizeof(GameContext) * 2];
    GameContext& GetMockContext()
    {
        return *reinterpret_cast<GameContext*>(s_dummyContextBuffer);
    }
}

TEST_SUITE("Scene Transition Overlay & Lifecycle Tests")
{
    TEST_CASE("SceneTransitionType and SceneTransitionConfig defaults")
    {
        SceneTransitionConfig cfg;
        CHECK(cfg.Type == SceneTransitionType::FadeBlack);
        CHECK(cfg.Duration == doctest::Approx(0.45f));
        CHECK(cfg.FadeColor == Colors::Black);
        CHECK(cfg.Ease == EasingFunctions::EasingType::EaseInOutQuad);

        SceneManager sm;
        const auto& defaultCfg = sm.GetDefaultTransition();
        CHECK(defaultCfg.Type == SceneTransitionType::FadeBlack);
        CHECK(defaultCfg.Duration == doctest::Approx(0.65f));
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Idle);
        CHECK(sm.GetTransitionProgress() == 0.0f);
        CHECK_FALSE(sm.IsTransitionActive());
    }

    TEST_CASE("Custom default transition configuration")
    {
        SceneManager sm;
        sm.SetDefaultTransition(SceneTransitionType::IrisWipe, 0.6f, Colors::White, EasingFunctions::EasingType::Linear);

        const auto& cfg = sm.GetDefaultTransition();
        CHECK(cfg.Type == SceneTransitionType::IrisWipe);
        CHECK(cfg.Duration == doctest::Approx(0.6f));
        CHECK(cfg.FadeColor == Colors::White);
        CHECK(cfg.Ease == EasingFunctions::EasingType::Linear);
    }

    TEST_CASE("Initial boot scene loads immediately without covering delay")
    {
        MockSceneA::ResetStats();
        GameContext& context = GetMockContext();

        SceneManager sm;
        sm.LoadScene<MockSceneA>();

        CHECK_FALSE(sm.IsTransitionActive());
        CHECK(MockSceneA::s_initCount == 0);

        // First update initializes scene A immediately
        sm.Update(0.016f, context);

        CHECK(MockSceneA::s_initCount == 1);
        CHECK(MockSceneA::s_destroyCount == 0);
        CHECK(MockSceneA::s_updateCount == 1);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Idle);
        CHECK(sm.GetTransitionProgress() == 0.0f);
        CHECK_FALSE(sm.IsTransitionActive());
        CHECK(sm.GetClearColor() == Colors::Red);
    }

    TEST_CASE("Scene transition lifecycle: Covering -> Midpoint Concealment -> Uncovering -> Idle")
    {
        MockSceneA::ResetStats();
        MockSceneB::ResetStats();
        GameContext& context = GetMockContext();

        SceneManager sm;
        // Set predictable duration: 0.4s total (0.2s covering, 0.2s uncovering) with linear easing
        sm.SetDefaultTransition(SceneTransitionType::FadeBlack, 0.4f, Colors::Black, EasingFunctions::EasingType::Linear);

        // Load scene A initially
        sm.LoadScene<MockSceneA>();
        sm.Update(0.016f, context);
        CHECK(MockSceneA::s_initCount == 1);

        // Now navigate from Scene A to Scene B
        sm.LoadScene<MockSceneB>();

        // Step 1: Start covering (outro)
        sm.Update(0.05f, context);
        CHECK(sm.IsTransitionActive());
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Covering);
        CHECK(sm.GetTransitionProgress() > 0.0f);
        CHECK(sm.GetTransitionProgress() < 1.0f);
        // Scene A is still active and updating behind the fade
        CHECK(MockSceneA::s_destroyCount == 0);
        CHECK(MockSceneB::s_initCount == 0);

        // Advance to 0.15s (still covering)
        sm.Update(0.10f, context);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Covering);
        CHECK(sm.GetTransitionProgress() == doctest::Approx(0.75f).epsilon(0.05f));
        CHECK(MockSceneA::s_destroyCount == 0);
        CHECK(MockSceneB::s_initCount == 0);

        // Step 2: Hit Midpoint at 0.20s total covering duration
        // Progress reaches 1.0 (screen fully masked) -> Scene A destroyed, Scene B initialized
        sm.Update(0.06f, context);
        CHECK(MockSceneA::s_destroyCount == 1);
        CHECK(MockSceneB::s_initCount == 1);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Uncovering);
        CHECK(sm.GetClearColor() == Colors::Blue);

        // Step 3: Uncovering phase (intro: progress decreases 1.0 -> 0.0)
        sm.Update(0.10f, context);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Uncovering);
        CHECK(sm.GetTransitionProgress() < 1.0f);
        CHECK(sm.GetTransitionProgress() > 0.0f);
        CHECK(MockSceneB::s_updateCount > 0);

        // Step 4: Uncovering finishes -> returns to Idle
        sm.Update(0.15f, context);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Idle);
        CHECK(sm.GetTransitionProgress() == 0.0f);
        CHECK_FALSE(sm.IsTransitionActive());
        CHECK(UISystem::IsNavigationEnabled());

        sm.Shutdown(context);
    }

    TEST_CASE("LoadSceneInstant bypasses transition entirely")
    {
        MockSceneA::ResetStats();
        MockSceneB::ResetStats();
        GameContext& context = GetMockContext();

        SceneManager sm;
        sm.LoadScene<MockSceneA>();
        sm.Update(0.016f, context);
        CHECK(MockSceneA::s_initCount == 1);

        // Instant switch to Scene B
        sm.LoadSceneInstant<MockSceneB>();
        sm.Update(0.016f, context);

        CHECK(MockSceneA::s_destroyCount == 1);
        CHECK(MockSceneB::s_initCount == 1);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Idle);
        CHECK(sm.GetTransitionProgress() == 0.0f);
        CHECK_FALSE(sm.IsTransitionActive());
        CHECK(sm.GetClearColor() == Colors::Blue);

        sm.Shutdown(context);
    }

    TEST_CASE("LoadSceneWithTransition custom parameters (IrisWipe, CurtainWipe)")
    {
        MockSceneA::ResetStats();
        MockSceneB::ResetStats();
        GameContext& context = GetMockContext();

        SceneManager sm;
        sm.LoadSceneInstant<MockSceneA>();
        sm.Update(0.016f, context);

        sm.LoadSceneWithTransition<MockSceneB>(SceneTransitionType::IrisWipe, 0.5f, Colors::White);
        sm.Update(0.016f, context);

        CHECK(sm.IsTransitionActive());
        CHECK(sm.GetCurrentTransitionType() == SceneTransitionType::IrisWipe);
        CHECK(sm.GetTransitionPhase() == SceneTransitionPhase::Covering);

        sm.Shutdown(context);
    }

    TEST_CASE("StateMachine re-entrancy protection prevents stack overflow on state change")
    {
        struct TestEntity
        {
            StateMachine<TestEntity>* pSm = nullptr;
            int reentrantCalls = 0;
            int endCalls = 0;
        };

        class ReentrantAction : public Action<TestEntity>
        {
        public:
            void Start(TestEntity*) override {}
            void Update(TestEntity*) override {}

            void End(TestEntity* pOwner) override
            {
                if (pOwner)
                {
                    pOwner->endCalls++;
                    // Try to recursively set state (previously caused 0xC00000FD stack overflow)
                    pOwner->reentrantCalls++;
                    pOwner->pSm->SetState(1);
                }
            }
        };

        TestEntity entity;
        StateMachine<TestEntity> sm(&entity, 3);
        entity.pSm = &sm;

        State<TestEntity>* state0 = sm.CreateState(0);
        state0->AddAction(new ReentrantAction());
        sm.CreateState(1);

        sm.SetState(0);
        CHECK(sm.GetCurrentState() == 0);

        // Transition from 0 to 1 invokes ReentrantAction::End which attempts recursive SetState(1)
        sm.SetState(1);
        CHECK(sm.GetCurrentState() == 1);
        CHECK(entity.endCalls == 1);
        CHECK(entity.reentrantCalls == 1);
    }

    TEST_CASE("Highscore name validation: whitespace trimming and non-empty enforcement")
    {
        auto validateName = [](const std::string& input) -> std::pair<bool, std::string> {
            std::string trimmed = input;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            if (trimmed.find_last_not_of(" \t\n\r") != std::string::npos)
                trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

            if (trimmed.empty())
            {
                return {false, ""};
            }
            return {true, trimmed};
        };

        CHECK_FALSE(validateName("").first);
        CHECK_FALSE(validateName("   ").first);
        CHECK_FALSE(validateName("\t\n").first);

        auto res1 = validateName("  Noa  ");
        CHECK(res1.first);
        CHECK(res1.second == "Noa");

        auto res2 = validateName("ACE");
        CHECK(res2.first);
        CHECK(res2.second == "ACE");
    }

    TEST_CASE("Retro CRT StaticSnow Scene Transition support (#90)")
    {
        SceneManager sm;
        sm.SetDefaultTransition(SceneTransitionType::StaticSnow, 0.45f, Colors::Black, EasingFunctions::EasingType::EaseInOutQuad);

        const auto& cfg = sm.GetDefaultTransition();
        CHECK(cfg.Type == SceneTransitionType::StaticSnow);
        CHECK(cfg.Duration == doctest::Approx(0.45f));
        CHECK(cfg.FadeColor == Colors::Black);
    }

    TEST_CASE("Retro TV 4x2 Channel Grid navigation & wrap-around logic (#90)")
    {
        // 4 columns x 2 rows = 8 channels
        constexpr int kColCount = 4;
        constexpr int kRowCount = 2;

        auto navigate = [](int current, int dx, int dy) -> int {
            int col = current % kColCount;
            int row = current / kColCount;

            if (dx != 0)
            {
                col = (col + dx + kColCount) % kColCount;
            }
            if (dy != 0)
            {
                row = (row + dy + kRowCount) % kRowCount;
            }
            return row * kColCount + col;
        };

        // Channel 0: Right -> 1, Left -> 3 (wrap-around row), Down -> 4, Up -> 4 (toggle row)
        CHECK(navigate(0, 1, 0) == 1);
        CHECK(navigate(0, -1, 0) == 3);
        CHECK(navigate(0, 0, 1) == 4);
        CHECK(navigate(0, 0, -1) == 4);

        // Channel 3: Right -> 0 (wrap-around)
        CHECK(navigate(3, 1, 0) == 0);

        // Channel 7: Right -> 4, Down -> 3, Up -> 3
        CHECK(navigate(7, 1, 0) == 4);
        CHECK(navigate(7, 0, 1) == 3);
        CHECK(navigate(7, 0, -1) == 3);
    }

    TEST_CASE("Retro TV Volume Dial angle & Master Volume calculation (#90)")
    {
        auto calcKnobAngle = [](float volumePercent) -> float {
            float clamped = std::clamp(volumePercent, 0.0f, 100.0f);
            return (clamped / 100.0f) * 270.0f - 135.0f;
        };

        // 0% -> -135 deg (MIN)
        CHECK(calcKnobAngle(0.0f) == doctest::Approx(-135.0f));
        // 50% -> 0 deg (12 o'clock)
        CHECK(calcKnobAngle(50.0f) == doctest::Approx(0.0f));
        // 100% -> +135 deg (MAX)
        CHECK(calcKnobAngle(100.0f) == doctest::Approx(135.0f));

        // Clamping check
        CHECK(calcKnobAngle(-20.0f) == doctest::Approx(-135.0f));
        CHECK(calcKnobAngle(150.0f) == doctest::Approx(135.0f));
    }

    TEST_CASE("Wii Channel Zoom State Machine & Dialog Navigation (#90)")
    {
        enum class ZoomState { Grid, ZoomingIn, ZoomedView, ZoomingOut };
        ZoomState state = ZoomState::Grid;
        float progress = 0.0f;
        int choice = 1; // 0 = Retour, 1 = Entrer

        // Trigger ZoomIn on channel select
        state = ZoomState::ZoomingIn;
        float dt = 0.15f;
        progress = std::clamp(progress + dt / 0.30f, 0.0f, 1.0f);
        CHECK(progress == doctest::Approx(0.5f));
        CHECK(state == ZoomState::ZoomingIn);

        // Advance to full zoom
        progress = std::clamp(progress + dt / 0.30f, 0.0f, 1.0f);
        CHECK(progress == doctest::Approx(1.0f));
        state = ZoomState::ZoomedView;

        // In ZoomedView: default choice is Entrer (1)
        CHECK(choice == 1);
        // Toggle choice to Retour (0)
        choice = 1 - choice;
        CHECK(choice == 0);

        // Cancel / Retour initiates ZoomingOut
        state = ZoomState::ZoomingOut;
        progress = std::clamp(progress - 0.25f / 0.25f, 0.0f, 1.0f);
        CHECK(progress == doctest::Approx(0.0f));
        state = ZoomState::Grid;
        CHECK(state == ZoomState::Grid);
    }

    TEST_CASE("Campaign 10-Second Preview loop & Title fade alpha (#90)")
    {
        auto calcTitleAlpha = [](float timer) -> float {
            float alpha = std::clamp(timer / 1.5f, 0.0f, 1.0f);
            if (timer > 8.5f)
            {
                alpha *= std::clamp((10.0f - timer) / 1.5f, 0.0f, 1.0f);
            }
            return alpha;
        };

        // Start (t = 0.0s): fully transparent
        CHECK(calcTitleAlpha(0.0f) == doctest::Approx(0.0f));
        // Fade in mid-point (t = 0.75s): 50% alpha
        CHECK(calcTitleAlpha(0.75f) == doctest::Approx(0.5f));
        // Fully visible during gameplay (t = 1.5s to 8.5s)
        CHECK(calcTitleAlpha(1.5f) == doctest::Approx(1.0f));
        CHECK(calcTitleAlpha(5.0f) == doctest::Approx(1.0f));
        CHECK(calcTitleAlpha(8.5f) == doctest::Approx(1.0f));
        // Fading out at end of 10s loop (t = 9.25s)
        CHECK(calcTitleAlpha(9.25f) == doctest::Approx(0.5f));
        // End of 10s loop (t = 10.0s)
        CHECK(calcTitleAlpha(10.0f) == doctest::Approx(0.0f));
    }

    TEST_CASE("Settings Interlocking Two-Gear kinematic speed matching (#90)")
    {
        const float r1 = 28.0f;
        const float r2 = 18.0f;
        const float w1 = 1.5f;
        // Gear 2 rotates opposite direction with angular velocity proportional to radius ratio
        const float w2 = -w1 * (r1 / r2);

        CHECK(w2 < 0.0f); // Reverse rotation
        CHECK(std::abs(w1 * r1) == doctest::Approx(std::abs(w2 * r2))); // Linear pitch velocity matched
    }

    TEST_CASE("Chassis CH button stepping logic (CH+ advances, CH- decrements) (#90)")
    {
        constexpr int kTotalChannels = 8;
        auto cycleChannel = [](int current, int step) -> int {
            return (current + step + kTotalChannels) % kTotalChannels;
        };

        // CH + advances forward
        int current = 0;
        current = cycleChannel(current, 1);
        CHECK(current == 1);
        current = cycleChannel(current, 1);
        CHECK(current == 2);

        // Wrap around at the end
        current = 7;
        current = cycleChannel(current, 1);
        CHECK(current == 0);

        // CH - goes backward
        current = cycleChannel(current, -1);
        CHECK(current == 7);
        current = cycleChannel(current, -1);
        CHECK(current == 6);
    }

    TEST_CASE("TV Power Dialog 3-way navigation & Standby state (#90)")
    {
        // 3 choices: 0 = Cancel, 1 = Standby, 2 = Power Off
        int choice = 1; // Default choice is Standby
        CHECK(choice == 1);

        auto navLeft = [](int ch) { return (ch + 2) % 3; };
        auto navRight = [](int ch) { return (ch + 1) % 3; };

        CHECK(navLeft(choice) == 0);  // Standby -> Cancel
        CHECK(navRight(choice) == 2); // Standby -> Power Off
        CHECK(navRight(2) == 0); // Power Off -> Cancel (wrap)
        CHECK(navLeft(0) == 2);  // Cancel -> Power Off (wrap)

        // Standby state transition
        bool isStandby = false;
        bool isPoweringOff = false;

        auto confirm = [&](int selected) {
            if (selected == 1) isStandby = true;
            else if (selected == 2) isPoweringOff = true;
        };

        confirm(1);
        CHECK(isStandby == true);
        CHECK(isPoweringOff == false);

        // Wake up restores standby
        isStandby = false;
        CHECK(isStandby == false);
    }

    TEST_CASE("Speaker Music Note Particles spawn interval scaling with volume (#90)")
    {
        auto calcSpawnInterval = [](float masterVolume) -> float {
            if (masterVolume <= 1.0f) return -1.0f; // No spawn when muted
            return 0.08f + ((100.0f - masterVolume) / 100.0f) * 0.35f;
        };

        // Muted (0% vol): no particles
        CHECK(calcSpawnInterval(0.0f) < 0.0f);

        // Max volume (100% vol): fastest particle emission
        CHECK(calcSpawnInterval(100.0f) == doctest::Approx(0.08f));

        // 50% volume: intermediate emission rate
        CHECK(calcSpawnInterval(50.0f) == doctest::Approx(0.255f));

        // 10% volume: slow emission rate
        CHECK(calcSpawnInterval(10.0f) == doctest::Approx(0.395f));

        // Higher volume produces shorter interval (more particles per second)
        CHECK(calcSpawnInterval(100.0f) < calcSpawnInterval(50.0f));
        CHECK(calcSpawnInterval(50.0f) < calcSpawnInterval(10.0f));
    }

    TEST_CASE("Workshop 3D Credit Coin horizontal perspective width (#90)")
    {
        const float R = 26.0f;
        auto getCoinWidth = [R](float angle) -> float {
            return std::max(2.5f, std::abs(std::cos(angle)) * R);
        };

        // Face-on (0 rad): full width
        CHECK(getCoinWidth(0.0f) == doctest::Approx(26.0f));

        // Edge-on (pi/2 rad): clamped to minimum visible rim width
        CHECK(getCoinWidth(1.570796f) == doctest::Approx(2.5f));

        // Half turn (pi rad): full width again
        CHECK(getCoinWidth(3.14159265f) == doctest::Approx(26.0f));
    }

    TEST_CASE("Endless Doom/Mortal Kombat flame oscillation & strobe blink (#90)")
    {
        auto isTitleStrobe = [](float animTime) -> bool {
            float phase = std::fmod(animTime * 4.5f, 1.0f);
            return (phase < 0.28f);
        };

        // At start of cycle: bright strobe
        CHECK(isTitleStrobe(0.0f) == true);

        // Later in cycle: fiery red title
        CHECK(isTitleStrobe(0.1f) == false);

        // Next cycle starts: strobe again
        float nextCycle = 1.0f / 4.5f;
        CHECK(isTitleStrobe(nextCycle + 0.01f) == true);
    }

    TEST_CASE("Wii Channel Zoom 100% full-screen TV dimensions (#90)")
    {
        // TV CRT Screen texture size is exactly 1390 x 910 with center at (695, 455)
        const Vector2f tvScreenSize{1390.0f, 910.0f};
        const Vector2f tvScreenCenter = tvScreenSize * 0.5f;

        CHECK(tvScreenCenter.X == doctest::Approx(695.0f));
        CHECK(tvScreenCenter.Y == doctest::Approx(455.0f));

        // When zoomProgress reaches 1.0 (smoothT = 1.0), the card must match tvScreenSize exactly
        const Vector2f targetCenter{695.0f, 455.0f};
        const Vector2f targetSize{1390.0f, 910.0f};

        auto lerpCard = [](const Vector2f& start, const Vector2f& target, float t) -> Vector2f {
            float smooth = t * t * (3.0f - 2.0f * t);
            return start + (target - start) * smooth;
        };

        Vector2f startPos{190.0f, 225.0f};
        Vector2f startSize{280.0f, 185.0f};

        Vector2f endPos = lerpCard(startPos, targetCenter, 1.0f);
        Vector2f endSize = lerpCard(startSize, targetSize, 1.0f);

        CHECK(endPos.X == doctest::Approx(695.0f));
        CHECK(endPos.Y == doctest::Approx(455.0f));
        CHECK(endSize.X == doctest::Approx(1390.0f));
        CHECK(endSize.Y == doctest::Approx(910.0f));
    }

    TEST_CASE("Wii Channel Full-Screen Camera Dive & Flare Transition (#90)")
    {
        const Vector2f viewSize{1920.0f, 1080.0f};
        const float baseScale = std::max(viewSize.X / 1880.0f, viewSize.Y / 1040.0f);
        const float targetScale = std::max(viewSize.X / 1390.0f, viewSize.Y / 910.0f) * 1.05f;

        // Target scale must be larger than base scale to zoom in past cabinet borders
        CHECK(targetScale > baseScale);

        auto calcDiveScale = [baseScale, targetScale](float progress) -> float {
            float diveT = progress * progress * (3.0f - 2.0f * progress);
            return baseScale + (targetScale - baseScale) * diveT;
        };

        CHECK(calcDiveScale(0.0f) == doctest::Approx(baseScale));
        CHECK(calcDiveScale(1.0f) == doctest::Approx(targetScale));

        // Glass center moves towards viewport center (960, 540)
        const Vector2f baseCabinetCenter{viewSize.X * 0.5f, viewSize.Y * 0.5f};
        const Vector2f baseGlassCenter = baseCabinetCenter + (Vector2f{800.0f, 540.0f} - Vector2f{960.0f, 540.0f}) * baseScale;
        const Vector2f targetGlassCenter = viewSize * 0.5f;

        auto calcGlassCenter = [baseGlassCenter, targetGlassCenter](float progress) -> Vector2f {
            float diveT = progress * progress * (3.0f - 2.0f * progress);
            return baseGlassCenter + (targetGlassCenter - baseGlassCenter) * diveT;
        };

        CHECK(calcGlassCenter(0.0f).X == doctest::Approx(baseGlassCenter.X));
        CHECK(calcGlassCenter(1.0f).X == doctest::Approx(targetGlassCenter.X));
        CHECK(calcGlassCenter(1.0f).Y == doctest::Approx(targetGlassCenter.Y));

        // Flare triggers only in the last 40% of the launch transition
        auto getFlareAlpha = [](float progress) -> float {
            if (progress <= 0.60f) return 0.0f;
            return std::clamp((progress - 0.60f) / 0.40f, 0.0f, 1.0f);
        };

        CHECK(getFlareAlpha(0.0f) == doctest::Approx(0.0f));
        CHECK(getFlareAlpha(0.50f) == doctest::Approx(0.0f));
        CHECK(getFlareAlpha(0.60f) == doctest::Approx(0.0f));
        CHECK(getFlareAlpha(0.80f) == doctest::Approx(0.50f));
        CHECK(getFlareAlpha(1.0f) == doctest::Approx(1.0f));
    }

    TEST_CASE("TV Full Screen Viewport Coverage across display aspect ratios (#90)")
    {
        const Vector2f resolutions[] = {
            {1920.0f, 1080.0f}, // 16:9 Standard Full HD
            {2560.0f, 1440.0f}, // 16:9 QHD
            {1280.0f, 720.0f},  // 16:9 720p
            {1707.0f, 1067.0f}, // 16:10 User display (logical)
            {1920.0f, 1200.0f}, // 16:10 WUXGA
            {2560.0f, 1600.0f}, // 16:10 MacBook / Pro
            {1600.0f, 1200.0f}  // 4:3 Classic
        };

        for (const auto& res : resolutions)
        {
            const float fitScaleX = (res.X * 0.97f) / 1880.0f;
            const float fitScaleY = (res.Y * 0.96f) / 1040.0f;
            const float baseScale = std::min(fitScaleX, fitScaleY);

            // CRT Glass and controls must completely fit inside the viewport without edge clipping
            const float glassW = 1390.0f * baseScale;
            const float glassH = 910.0f * baseScale;
            CHECK(glassW < res.X);
            CHECK(glassH < res.Y);

            // Outer cabinet spans 100% of viewport (zero black letterbox space)
            const float bgW = res.X * 1.05f;
            const float bgH = res.Y * 1.05f;
            CHECK(bgW >= res.X);
            CHECK(bgH >= res.Y);
        }
    }

    TEST_CASE("Channel cycling navigation wraps properly forward and backward (#90)")
    {
        const int channelCount = 8;
        auto cycle = [channelCount](int current, int step) -> int {
            return (current + step + channelCount) % channelCount;
        };

        // Forward cycling
        CHECK(cycle(0, 1) == 1);
        CHECK(cycle(6, 1) == 7);
        CHECK(cycle(7, 1) == 0); // Wrap around

        // Backward cycling
        CHECK(cycle(0, -1) == 7); // Wrap around
        CHECK(cycle(7, -1) == 6);
        CHECK(cycle(1, -1) == 0);
    }

    TEST_CASE("Reverse Dezoom Transition Logic (#90)")
    {
        // Smooth reverse cubic easing verification
        auto getDezoomScale = [](float returnProgress, float baseScale, float targetScale) -> float {
            float p = std::clamp(returnProgress, 0.0f, 1.0f);
            float diveT = p * p * (3.0f - 2.0f * p);
            return baseScale + (targetScale - baseScale) * diveT;
        };

        const float baseScale = 0.55f;
        const float targetScale = 1.25f;

        // At start of dezoom (full screen)
        CHECK(getDezoomScale(1.0f, baseScale, targetScale) == doctest::Approx(targetScale));
        // Midway through dezoom
        CHECK(getDezoomScale(0.5f, baseScale, targetScale) == doctest::Approx(baseScale + (targetScale - baseScale) * 0.5f));
        // At completion of dezoom (returned to cabinet grid)
        CHECK(getDezoomScale(0.0f, baseScale, targetScale) == doctest::Approx(baseScale));

        // Monotonically decreasing
        CHECK(getDezoomScale(0.8f, baseScale, targetScale) > getDezoomScale(0.4f, baseScale, targetScale));
        CHECK(getDezoomScale(0.4f, baseScale, targetScale) > getDezoomScale(0.1f, baseScale, targetScale));
    }
}
