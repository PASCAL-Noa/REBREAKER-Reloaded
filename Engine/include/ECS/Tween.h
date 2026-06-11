#pragma once
#include <functional>
#include <memory>
#include "Math/EasingFunctions.h"

class ITween
{
public:
    virtual ~ITween() = default;
    virtual void    Update(float dt) = 0;
    virtual bool    IsFinished() const = 0;
};

template<typename T>
struct TweenConfig
{
    T   Start{};
    T   End{};
    float   Duration = 0.0f;
    std::function<void(T)>  Setter;
    std::function<void()>   OnComplete;
    EasingFunctions::EasingType     Ease = EasingFunctions::EasingType::Linear;
};

template<typename T>
class Tween : public ITween
{
public:
    explicit Tween(const TweenConfig<T>& config) : m_config(config)
    {
        m_easeFunc = EasingFunctions::GetFunction(m_config.Ease);
    }

    void Update(float dt) override
    {
        if (m_completed) return;

        m_elapsed += dt;

        if (IsFinished())
        {
            m_config.Setter(m_config.End);
            if (m_config.OnComplete) m_config.OnComplete();
            m_completed = true;
            return;
        }

        float t = m_elapsed / m_config.Duration;
        float easedT = m_easeFunc(t);

        T currentValue = m_config.Start + (m_config.End - m_config.Start) * easedT;
        m_config.Setter(currentValue);
    }

    [[nodiscard]] bool IsFinished() const override
    {
        return m_elapsed >= m_config.Duration;
    }

private:
    TweenConfig<T>  m_config;
    float   m_elapsed = 0.0f;
    bool    m_completed = false;
    EasingFunctions::EasingFunction     m_easeFunc;
};