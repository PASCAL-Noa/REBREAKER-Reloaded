#pragma once
#include "Math/Vector2.h"
#include "Data/Color.h"
#include "LevelTheme.h"
#include <vector>

class Renderer;

class AmbientBackground
{
public:
    struct DustParticle
    {
        Vector2f Position{0.0f, 0.0f};
        Vector2f Velocity{0.0f, 0.0f};
        float Size = 2.0f;
        float BaseAlpha = 35.0f;
        float PulseSpeed = 2.0f;
        float PulsePhase = 0.0f;
        float ParallaxDepth = 0.15f;
    };

    AmbientBackground() = default;
    ~AmbientBackground() = default;

    void OnInit(size_t particleCount = 60);
    void SetTheme(const LevelTheme& theme);
    void OnUpdate(float dt, const Vector2f& paddlePos, const Vector2f& cameraPos);
    void OnRender(Renderer& renderer, const Color& currentBgColor, const Vector2f& cameraPos, const Vector2f& paddlePos) const;

private:
    std::vector<DustParticle> m_particles;
    LevelTheme m_theme{};
    float m_animTime = 0.0f;
    float m_gridOffset = 0.0f;
};
