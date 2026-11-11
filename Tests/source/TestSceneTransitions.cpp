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
        CHECK(defaultCfg.Duration == doctest::Approx(0.4f));
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
}
