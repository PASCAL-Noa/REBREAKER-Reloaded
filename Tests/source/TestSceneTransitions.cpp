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
}
