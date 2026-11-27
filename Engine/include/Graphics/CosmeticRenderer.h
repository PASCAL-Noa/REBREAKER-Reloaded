#pragma once

#include "Core/ProfileManager.h"
#include "ECS/Components/TrailComponent.h"
#include "Data/Color.h"
#include "Data/Vertex.h"
#include "Math/Vector2.h"
#include <vector>
#include <cstdint>

class Renderer;

struct CosmeticBurstParticle
{
    Vector2f Position{0.0f, 0.0f};
    Vector2f Velocity{0.0f, 0.0f};
    float    Angle = 0.0f;
    float    AngularVel = 0.0f;
    float    Size = 6.0f;
    float    Life = 0.0f;
    float    MaxLife = 0.5f;
    float    Gravity = 0.0f;
    Color    Tint = Colors::White;
    uint8_t  ShapeKind = 0; // 0=square, 1=diamond, 2=line/bar, 3=binary0, 4=binary1
};

struct CosmeticExplosionBurst
{
    Vector2f         Origin{0.0f, 0.0f};
    DeathEffectStyle Style = DeathEffectStyle::ClassicBurst;
    Color            Primary{0, 230, 255, 255};
    Color            Secondary{255, 255, 255, 255};
    float            Age = 0.0f;
    float            Duration = 0.65f;
    float            Scale = 1.0f;
    std::vector<CosmeticBurstParticle> Particles;
};

class CosmeticExplosionPool
{
public:
    void Spawn(const Vector2f& origin, DeathEffectStyle style, Color primary, Color secondary, float scale = 1.0f);
    void Update(float dt);
    void Render(Renderer& renderer) const;
    void Clear();

    [[nodiscard]] size_t GetActiveCount() const noexcept { return m_bursts.size(); }

private:
    std::vector<CosmeticExplosionBurst> m_bursts;
};

class CosmeticRenderer
{
public:
    static void DrawPaddle(
        Renderer&       renderer,
        const Vector2f& pos,
        const Vector2f& size,
        float           rotationDeg,
        PaddleSkinStyle style,
        Color           primary,
        Color           secondary,
        float           animTime,
        float           alphaScale = 1.0f,
        uint32_t        spriteTexId = 0);

    static void DrawBall(
        Renderer&       renderer,
        const Vector2f& pos,
        float           radius,
        BallSkinStyle   style,
        Color           primary,
        Color           secondary,
        float           animTime,
        float           alphaScale = 1.0f,
        bool            isFireBall = false,
        bool            isGhost = false,
        bool            isTempo = false,
        uint32_t        spriteTexId = 0);

    [[nodiscard]] static TrailConfig BuildTrailConfig(TrailStyle style, Color primary, Color secondary);
    [[nodiscard]] static TrailConfig BuildEquippedTrailConfig(const ProfileManager& profile);

    static void RenderTrail(
        Renderer&            renderer,
        const TrailComponent& trail,
        float                animTime,
        std::vector<Vertex>& scratch);

    static void DrawBinaryDigit(
        Renderer&       renderer,
        const Vector2f& pos,
        float           size,
        bool            isOne,
        Color           color,
        BlendMode       blend = BlendMode::Add);

    static void DrawTrailPreviewIcon(
        Renderer&       renderer,
        const Vector2f& center,
        const Vector2f& boxSize,
        TrailStyle      style,
        Color           primary,
        Color           secondary,
        float           animTime);

    static void DrawDeathEffectPreviewIcon(
        Renderer&        renderer,
        const Vector2f&  center,
        float            radius,
        DeathEffectStyle style,
        Color            primary,
        Color            secondary,
        float            animTime);

    static void DrawLockIcon(
        Renderer&       renderer,
        const Vector2f& pos,
        float           size,
        Color           color);
};
