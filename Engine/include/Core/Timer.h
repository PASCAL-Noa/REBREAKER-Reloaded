#pragma once
#include <chrono>

class Timer
{
public:
    explicit Timer(unsigned int targetFPS = 0);
    ~Timer() = default;

    void Update();

    [[nodiscard]] float     GetDeltaTime() const;
    [[nodiscard]] float     GetFPS() const;

private:
    std::chrono::high_resolution_clock::time_point m_lastTime;
    std::chrono::high_resolution_clock::time_point m_lastFpsTime;

    float m_deltaTime;
    float m_targetDeltaTime;
    float m_fps;
    int m_frameCount;
};