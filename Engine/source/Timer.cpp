#include "Core/Timer.h"
#include <thread>

Timer::Timer(unsigned int targetFPS)
    : m_deltaTime(0.0f), m_fps(0.0f), m_frameCount(0)
{
    m_targetDeltaTime = targetFPS > 0 ? 1.0f / static_cast<float>(targetFPS) : 0.0f;
    m_lastTime = std::chrono::high_resolution_clock::now();
    m_lastFpsTime = m_lastTime;
}

void Timer::Update()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = currentTime - m_lastTime;

    while (m_targetDeltaTime > 0.0f && elapsed.count() < m_targetDeltaTime)
    {
        float timeRemaining = m_targetDeltaTime - elapsed.count();

        if (timeRemaining > 0.002f)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        currentTime = std::chrono::high_resolution_clock::now();
        elapsed = currentTime - m_lastTime;
    }

    m_deltaTime = elapsed.count();
    m_lastTime = currentTime;

    m_frameCount++;
    std::chrono::duration<float> fpsElapsed = currentTime - m_lastFpsTime;
    if (fpsElapsed.count() >= 1.0f)
    {
        m_fps = static_cast<float>(m_frameCount) / fpsElapsed.count();
        m_frameCount = 0;
        m_lastFpsTime = currentTime;
    }
}

float Timer::GetDeltaTime() const
{
    return m_deltaTime;
}

float Timer::GetFPS() const
{
    return m_fps;
}