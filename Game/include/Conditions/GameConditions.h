#pragma once
#include "StateMachine/Condition.h"

template<typename T>
class LivesCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override
    {
        if constexpr (requires { pOwner->IsGameOver(); })
        {
            return pOwner->IsGameOver();
        }
        else
        {
            return pOwner->GetLives() <= 0;
        }
    }
};

template<typename T>
class VictoryCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override
    {
        bool cleared = false;
        if constexpr (requires { pOwner->IsLevelCleared(); })
        {
            cleared = pOwner->IsLevelCleared();
        }
        else
        {
            cleared = pOwner->GetBrickCount() <= 0;
        }

        if constexpr (requires { pOwner->HasNextLevel(); })
        {
            return cleared && !pOwner->HasNextLevel();
        }
        else
        {
            return cleared;
        }
    }
};

template<typename T>
class LevelClearedCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override
    {
        bool cleared = false;
        if constexpr (requires { pOwner->IsLevelCleared(); })
        {
            cleared = pOwner->IsLevelCleared();
        }
        else
        {
            cleared = pOwner->GetBrickCount() <= 0;
        }

        if constexpr (requires { pOwner->HasNextLevel(); })
        {
            return cleared && pOwner->HasNextLevel();
        }
        else
        {
            return false;
        }
    }
};

template<typename T>
class LevelTransitionCompleteCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override
    {
        if constexpr (requires { pOwner->IsLevelTransitionComplete(); })
        {
            return pOwner->IsLevelTransitionComplete();
        }
        else
        {
            return true;
        }
    }
};

template<typename T>
class GameOverReplayCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override
    {
        if constexpr (requires { pOwner->IsTypingName(); })
        {
            if (pOwner->IsTypingName())
            {
                return false;
            }
        }
        const bool replayTriggered = pOwner->GetContext()->Input.IsActionTriggered(InputAction::LaunchBall) || pOwner->GetContext()->Input.IsKeyPress(KeyCode::Space);
        if (replayTriggered)
        {
            if constexpr (requires { pOwner->TryConfirmNameBeforeProceeding(); })
            {
                return pOwner->TryConfirmNameBeforeProceeding();
            }
            else
            {
                return true;
            }
        }
        return false;
    }
};
