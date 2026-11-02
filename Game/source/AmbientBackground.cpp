#include "AmbientBackground.h"
#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include <random>
#include <cmath>
#include <algorithm>

constexpr float kPlayfieldLeft = -800.0f;
constexpr float kPlayfieldRight = 800.0f;
constexpr float kPlayfieldTop = -450.0f;
constexpr float kPlayfieldBottom = 450.0f;
constexpr float kGridCellSize = 75.0f;

void AmbientBackground::OnInit(size_t particleCount)
{
    m_particles.clear();
    m_particles.reserve(particleCount);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> posX(kPlayfieldLeft - 50.0f, kPlayfieldRight + 50.0f);
    std::uniform_real_distribution<float> posY(kPlayfieldTop - 50.0f, kPlayfieldBottom + 50.0f);
    std::uniform_real_distribution<float> velX(-12.0f, 12.0f);
    std::uniform_real_distribution<float> velY(-22.0f, -8.0f);
    std::uniform_real_distribution<float> sizeDist(1.5f, 3.2f);
    std::uniform_real_distribution<float> alphaDist(22.0f, 50.0f);
    std::uniform_real_distribution<float> pulseSpeedDist(1.2f, 3.0f);
    std::uniform_real_distribution<float> phaseDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> depthDist(0.08f, 0.30f);

    for (size_t i = 0; i < particleCount; ++i)
    {
        DustParticle p;
        p.Position = Vector2f{posX(rng), posY(rng)};
        p.Velocity = Vector2f{velX(rng), velY(rng)};
        p.Size = sizeDist(rng);
        p.BaseAlpha = alphaDist(rng);
        p.PulseSpeed = pulseSpeedDist(rng);
        p.PulsePhase = phaseDist(rng);
        p.ParallaxDepth = depthDist(rng);
        m_particles.push_back(p);
    }
}

void AmbientBackground::SetTheme(const LevelTheme& theme)
{
    m_theme = theme;
    // Re-tune particles with the new theme's drift speed
    for (auto& p : m_particles)
    {
        p.Velocity.Y = theme.DustSpeedY * (0.7f + 0.6f * p.ParallaxDepth);
    }
}

void AmbientBackground::OnUpdate(float dt, const Vector2f& paddlePos, const Vector2f& cameraPos)
{
    (void)paddlePos;
    (void)cameraPos;
    m_animTime += dt;

    // Continuous vertical scrolling for the ambient grid based on theme
    const float gridSpeed = (m_theme.GridSpeed > 0.0f) ? m_theme.GridSpeed : 14.0f;
    m_gridOffset += dt * gridSpeed;
    if (m_gridOffset >= kGridCellSize)
    {
        m_gridOffset = std::fmod(m_gridOffset, kGridCellSize);
    }

    // Drift dust particles with subtle sinuous wave and paddle parallax
    const float boundLeft = kPlayfieldLeft - 60.0f;
    const float boundRight = kPlayfieldRight + 60.0f;
    const float boundTop = kPlayfieldTop - 60.0f;
    const float boundBottom = kPlayfieldBottom + 60.0f;

    for (auto& p : m_particles)
    {
        const float wave = std::sin(m_animTime * p.PulseSpeed + p.PulsePhase) * 6.0f;
        p.Position.X += (p.Velocity.X + wave) * dt;
        p.Position.Y += p.Velocity.Y * dt;

        // Wrap around bounds
        if (p.Position.Y < boundTop)
        {
            p.Position.Y = boundBottom;
            p.Position.X = boundLeft + std::fmod(std::abs(p.Position.X * 1.618f), (boundRight - boundLeft));
        }
        else if (p.Position.Y > boundBottom)
        {
            p.Position.Y = boundTop;
        }

        if (p.Position.X < boundLeft)
        {
            p.Position.X = boundRight;
        }
        else if (p.Position.X > boundRight)
        {
            p.Position.X = boundLeft;
        }
    }
}

void AmbientBackground::OnRender(Renderer& renderer, const Color& currentBgColor, const Vector2f& cameraPos, const Vector2f& paddlePos) const
{
    // 1. Subtle parallax grid themed for the current level
    Color gridColor = m_theme.GridColor;
    if (gridColor.a == 0)
    {
        gridColor = Color{45, 80, 150, 24};
    }
    // Modulate grid tint with current background brightness
    gridColor.r = static_cast<uint8_t>(std::min(255, (gridColor.r + currentBgColor.r) / 2 + 15));
    gridColor.g = static_cast<uint8_t>(std::min(255, (gridColor.g + currentBgColor.g) / 2 + 20));
    gridColor.b = static_cast<uint8_t>(std::min(255, (gridColor.b + currentBgColor.b) / 2 + 30));

    // Parallax offset based on paddle position and camera movement
    const float parallaxX = (paddlePos.X * 0.035f) + (cameraPos.X * 0.12f);
    const float parallaxY = m_gridOffset + (cameraPos.Y * 0.12f);

    const float startX = kPlayfieldLeft - kGridCellSize;
    const float endX = kPlayfieldRight + kGridCellSize;
    const float startY = kPlayfieldTop - kGridCellSize;
    const float endY = kPlayfieldBottom + kGridCellSize;

    // Vertical grid lines
    const float firstGridX = startX - std::fmod(startX + parallaxX, kGridCellSize);
    for (float x = firstGridX; x <= endX; x += kGridCellSize)
    {
        renderer.DrawLine(Vector2f{x, startY}, Vector2f{x, endY}, gridColor, 1.0f);
    }

    // Horizontal grid lines
    const float firstGridY = startY - std::fmod(startY - parallaxY, kGridCellSize);
    for (float y = firstGridY; y <= endY; y += kGridCellSize)
    {
        renderer.DrawLine(Vector2f{startX, y}, Vector2f{endX, y}, gridColor, 1.0f);
    }

    // 2. Slow floating ambient dust particles themed for the current level
    const Color baseDustColor = (m_theme.DustColor.a > 0) ? m_theme.DustColor : Color{180, 220, 255, 45};

    for (const auto& p : m_particles)
    {
        // Parallax shift based on particle depth and paddle/cam motion
        const float px = p.Position.X - (paddlePos.X * 0.04f + cameraPos.X * 0.15f) * p.ParallaxDepth;
        const float py = p.Position.Y - (cameraPos.Y * 0.15f) * p.ParallaxDepth;

        // Twinkle factor
        const float twinkle = (std::sin(m_animTime * p.PulseSpeed + p.PulsePhase) + 1.0f) * 0.5f;
        const uint8_t alpha = static_cast<uint8_t>(std::clamp(p.BaseAlpha * (0.4f + 0.6f * twinkle), 10.0f, 65.0f));

        const uint8_t pr = static_cast<uint8_t>(std::min(255, (baseDustColor.r + currentBgColor.r) / 2 + 60));
        const uint8_t pg = static_cast<uint8_t>(std::min(255, (baseDustColor.g + currentBgColor.g) / 2 + 75));
        const uint8_t pb = static_cast<uint8_t>(std::min(255, (baseDustColor.b + currentBgColor.b) / 2 + 90));
        const Color dustColor{pr, pg, pb, alpha};

        renderer.DrawRectangle(p.Size, p.Size, Transform2D{Vector2f{px, py}}, dustColor);
    }
}
