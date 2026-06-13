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
    bool    Yoyo = false;
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

        if (m_elapsed >= m_config.Duration)
        {
            m_config.Setter(m_config.End);
            
            if (m_config.Yoyo)
            {
                T temp = m_config.Start;
                m_config.Start = m_config.End;
                m_config.End = temp;
                
                m_elapsed -= m_config.Duration;
            }
            else
            {
                if (m_config.OnComplete) m_config.OnComplete();
                m_completed = true;
                return;
            }
        }

        float t = m_elapsed / m_config.Duration;
        if (t > 1.0f) t = 1.0f;
        else if (t < 0.0f) t = 0.0f;
        
        float easedT = m_easeFunc(t);

        T currentValue = m_config.Start + (m_config.End - m_config.Start) * easedT;
        m_config.Setter(currentValue);
    }

    [[nodiscard]] bool IsFinished() const override
    {
        return m_completed;
    }

private:
    TweenConfig<T>  m_config;
    float   m_elapsed = 0.0f;
    bool    m_completed = false;
    EasingFunctions::EasingFunction     m_easeFunc;
};