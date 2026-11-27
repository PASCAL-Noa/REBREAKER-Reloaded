#include "Graphics/CosmeticRenderer.h"
#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDegToRad = kPi / 180.0f;
    constexpr float kRadToDeg = 180.0f / kPi;

    Color ScaleAlpha(Color c, float alphaScale)
    {
        float a = static_cast<float>(c.a) * std::clamp(alphaScale, 0.0f, 1.0f);
        c.a = static_cast<uint8_t>(std::clamp(a, 0.0f, 255.0f));
        return c;
    }

    Color WithAlpha(Color c, uint8_t alpha, float alphaScale = 1.0f)
    {
        float a = static_cast<float>(alpha) * std::clamp(alphaScale, 0.0f, 1.0f);
        c.a = static_cast<uint8_t>(std::clamp(a, 0.0f, 255.0f));
        return c;
    }

    Color BlendColor(Color a, Color b, float t, float alphaScale = 1.0f)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        Color out;
        out.r = static_cast<uint8_t>(a.r * (1.0f - t) + b.r * t);
        out.g = static_cast<uint8_t>(a.g * (1.0f - t) + b.g * t);
        out.b = static_cast<uint8_t>(a.b * (1.0f - t) + b.b * t);
        float alpha = (a.a * (1.0f - t) + b.a * t) * std::clamp(alphaScale, 0.0f, 1.0f);
        out.a = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
        return out;
    }

    Color Darken(Color c, float factor, float alphaScale = 1.0f)
    {
        factor = std::clamp(factor, 0.0f, 1.0f);
        return Color{
            static_cast<uint8_t>(c.r * factor),
            static_cast<uint8_t>(c.g * factor),
            static_cast<uint8_t>(c.b * factor),
            static_cast<uint8_t>(std::clamp(static_cast<float>(c.a) * alphaScale, 0.0f, 255.0f))
        };
    }

    Vector2f LocalToWorld(const Vector2f& origin, float cosR, float sinR, float lx, float ly)
    {
        return Vector2f{
            origin.X + lx * cosR - ly * sinR,
            origin.Y + lx * sinR + ly * cosR
        };
    }

    void DrawMiniGear(Renderer& renderer, const Vector2f& center, float outerR, float angleDeg, Color brass, Color hub, float alphaScale)
    {
        const float toothW = std::max(2.0f, outerR * 0.42f);
        const float toothH = outerR * 2.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float a = angleDeg + static_cast<float>(i) * 45.0f;
            renderer.DrawRectangle(toothW, toothH, Transform2D{center, a}, ScaleAlpha(brass, alphaScale));
        }
        renderer.DrawCircle(outerR * 0.72f, Transform2D{center}, ScaleAlpha(brass, alphaScale));
        renderer.DrawCircle(outerR * 0.38f, Transform2D{center}, ScaleAlpha(Color{42, 26, 16, 255}, alphaScale));
        renderer.DrawCircle(outerR * 0.22f, Transform2D{center}, ScaleAlpha(hub, alphaScale));
    }

    float DeterministicHash(size_t index, int salt)
    {
        uint32_t x = static_cast<uint32_t>(index * 2654435761u ^ static_cast<uint32_t>(salt) * 2246822519u);
        x ^= x >> 13;
        x *= 1274126177u;
        x ^= x >> 16;
        return static_cast<float>(x & 0xFFFFu) / 65535.0f;
    }
}

// =============================================================================
// COSMETIC EXPLOSION POOL (DEATH / EXPLOSION EFFECTS)
// =============================================================================

void CosmeticExplosionPool::Spawn(const Vector2f& origin, DeathEffectStyle style, Color primary, Color secondary, float scale)
{
    if (m_bursts.size() >= 18)
    {
        m_bursts.erase(m_bursts.begin());
    }

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    CosmeticExplosionBurst burst;
    burst.Origin = origin;
    burst.Style = style;
    burst.Primary = primary;
    burst.Secondary = secondary;
    burst.Age = 0.0f;
    burst.Scale = std::max(0.25f, scale);

    switch (style)
    {
        case DeathEffectStyle::ClassicMinimal:
        {
            burst.Duration = 0.42f;
            const int count = 12;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const float ang = (static_cast<float>(i) / count) * 2.0f * kPi + (u01(rng) - 0.5f) * 0.45f;
                const float spd = (75.0f + u01(rng) * 165.0f) * burst.Scale;
                CosmeticBurstParticle p;
                p.Position = origin;
                p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd};
                p.Size = (3.5f + u01(rng) * 3.5f) * burst.Scale;
                p.MaxLife = 0.24f + u01(rng) * 0.18f;
                p.Life = p.MaxLife;
                p.Angle = 0.0f;
                p.AngularVel = 0.0f;
                p.Tint = (i % 3 == 0) ? Colors::White : primary;
                p.ShapeKind = 0; // clean classic arcade square spark
                burst.Particles.push_back(p);
            }
            break;
        }
        case DeathEffectStyle::ClassicBurst:
        {
            burst.Duration = 0.55f;
            const int count = 22;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const float ang = (static_cast<float>(i) / count) * 2.0f * kPi + (u01(rng) - 0.5f) * 0.35f;
                const float spd = (90.0f + u01(rng) * 220.0f) * burst.Scale;
                CosmeticBurstParticle p;
                p.Position = origin;
                p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd};
                p.Size = (4.5f + u01(rng) * 5.5f) * burst.Scale;
                p.MaxLife = 0.32f + u01(rng) * 0.22f;
                p.Life = p.MaxLife;
                p.Angle = u01(rng) * 360.0f;
                p.AngularVel = (u01(rng) - 0.5f) * 360.0f;
                p.Tint = (i % 3 == 0) ? secondary : primary;
                p.ShapeKind = (i % 2 == 0) ? 1 : 0;
                burst.Particles.push_back(p);
            }
            break;
        }
        case DeathEffectStyle::ShockwaveRing:
        {
            burst.Duration = 0.62f;
            const int count = 16;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const float ang = (static_cast<float>(i) / count) * 2.0f * kPi;
                const float spd = (170.0f + u01(rng) * 140.0f) * burst.Scale;
                CosmeticBurstParticle p;
                p.Position = origin;
                p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd};
                p.Size = (7.0f + u01(rng) * 5.0f) * burst.Scale;
                p.MaxLife = 0.36f + u01(rng) * 0.20f;
                p.Life = p.MaxLife;
                p.Angle = ang * kRadToDeg;
                p.Tint = (i % 2 == 0) ? primary : secondary;
                p.ShapeKind = 2; // directional shard bar
                burst.Particles.push_back(p);
            }
            break;
        }
        case DeathEffectStyle::PixelShatter:
        {
            burst.Duration = 0.72f;
            const int count = 26;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const float ang = u01(rng) * 2.0f * kPi;
                const float spd = (80.0f + u01(rng) * 210.0f) * burst.Scale;
                CosmeticBurstParticle p;
                p.Position = origin + Vector2f{(u01(rng) - 0.5f) * 16.0f * burst.Scale, (u01(rng) - 0.5f) * 16.0f * burst.Scale};
                p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd - 90.0f * burst.Scale};
                p.Size = (6.0f + std::floor(u01(rng) * 3.0f) * 3.5f) * burst.Scale;
                p.MaxLife = 0.42f + u01(rng) * 0.28f;
                p.Life = p.MaxLife;
                p.Gravity = 480.0f * burst.Scale;
                p.Angle = 0.0f; // Keep crisp 8-bit axis-aligned pixels!
                p.Tint = (i % 4 == 0) ? Colors::White : ((i % 2 == 0) ? primary : secondary);
                p.ShapeKind = 0;
                burst.Particles.push_back(p);
            }
            break;
        }
        case DeathEffectStyle::PrismConfetti:
        {
            burst.Duration = 0.80f;
            const int count = 30;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const float ang = u01(rng) * 2.0f * kPi;
                const float spd = (95.0f + u01(rng) * 230.0f) * burst.Scale;
                CosmeticBurstParticle p;
                p.Position = origin;
                p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd - 70.0f * burst.Scale};
                p.Size = (5.5f + u01(rng) * 5.5f) * burst.Scale;
                p.MaxLife = 0.48f + u01(rng) * 0.30f;
                p.Life = p.MaxLife;
                p.Gravity = 240.0f * burst.Scale;
                p.Angle = u01(rng) * 360.0f;
                p.AngularVel = (u01(rng) - 0.5f) * 540.0f;
                p.Tint = TrailPresets::HsvToRgb(std::fmod(static_cast<float>(i) * 31.0f, 360.0f), 0.92f, 1.0f);
                p.ShapeKind = (i % 3 == 0) ? 2 : 1;
                burst.Particles.push_back(p);
            }
            break;
        }
        case DeathEffectStyle::NovaImplosion:
        {
            burst.Duration = 0.68f;
            const int count = 24;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const float ang = (static_cast<float>(i) / count) * 2.0f * kPi;
                const float spd = ((i % 4 == 0) ? 280.0f : (110.0f + u01(rng) * 140.0f)) * burst.Scale;
                CosmeticBurstParticle p;
                p.Position = origin;
                p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd};
                p.Size = ((i % 4 == 0) ? 9.0f : 5.0f) * burst.Scale;
                p.MaxLife = 0.38f + u01(rng) * 0.24f;
                p.Life = p.MaxLife;
                p.Angle = 45.0f;
                p.AngularVel = 180.0f;
                p.Tint = (i % 4 == 0) ? Colors::White : ((i % 2 == 0) ? primary : secondary);
                p.ShapeKind = 1;
                burst.Particles.push_back(p);
            }
            break;
        }
        case DeathEffectStyle::MatrixGlitch:
        {
            burst.Duration = 0.68f;
            const int count = 22;
            burst.Particles.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const bool isBar = (i < 8);
                CosmeticBurstParticle p;
                if (isBar)
                {
                    const float yOff = (u01(rng) - 0.5f) * 54.0f * burst.Scale;
                    const float dir = (i % 2 == 0) ? 1.0f : -1.0f;
                    p.Position = origin + Vector2f{(u01(rng) - 0.5f) * 24.0f * burst.Scale, yOff};
                    p.Velocity = Vector2f{dir * (160.0f + u01(rng) * 180.0f) * burst.Scale, 0.0f};
                    p.Size = (18.0f + u01(rng) * 24.0f) * burst.Scale;
                    p.MaxLife = 0.25f + u01(rng) * 0.20f;
                    p.Life = p.MaxLife;
                    p.Tint = (i % 3 == 0) ? secondary : primary;
                    p.ShapeKind = 2;
                }
                else
                {
                    const float ang = u01(rng) * 2.0f * kPi;
                    const float spd = (70.0f + u01(rng) * 170.0f) * burst.Scale;
                    p.Position = origin;
                    p.Velocity = Vector2f{std::cos(ang) * spd, std::sin(ang) * spd};
                    p.Size = (7.5f + u01(rng) * 4.5f) * burst.Scale;
                    p.MaxLife = 0.36f + u01(rng) * 0.28f;
                    p.Life = p.MaxLife;
                    p.Gravity = 90.0f * burst.Scale;
                    p.Tint = (i % 2 == 0) ? primary : secondary;
                    p.ShapeKind = (i % 2 == 0) ? 3 : 4; // binary 0 / 1
                }
                burst.Particles.push_back(p);
            }
            break;
        }
    }

    m_bursts.push_back(std::move(burst));
}

void CosmeticExplosionPool::Update(float dt)
{
    for (auto it = m_bursts.begin(); it != m_bursts.end(); )
    {
        it->Age += dt;
        for (auto& p : it->Particles)
        {
            if (p.Life <= 0.0f) continue;
            p.Life -= dt;
            p.Velocity.Y += p.Gravity * dt;
            p.Position.X += p.Velocity.X * dt;
            p.Position.Y += p.Velocity.Y * dt;
            p.Velocity.X *= std::max(0.0f, 1.0f - 2.2f * dt);
            if (p.Gravity <= 0.0f)
            {
                p.Velocity.Y *= std::max(0.0f, 1.0f - 2.2f * dt);
            }
            p.Angle += p.AngularVel * dt;
        }

        if (it->Age >= it->Duration)
        {
            it = m_bursts.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CosmeticExplosionPool::Render(Renderer& renderer) const
{
    for (const auto& burst : m_bursts)
    {
        const float t = std::clamp(burst.Age / std::max(0.01f, burst.Duration), 0.0f, 1.0f);
        const float invT = 1.0f - t;

        // Style-specific centerpiece flash / shockwave rings / nova cross
        switch (burst.Style)
        {
            case DeathEffectStyle::ClassicMinimal:
            {
                // Original minimalist arcade feel: crisp subtle flash, no extra rings
                renderer.DrawCircle(14.0f * invT * burst.Scale, Transform2D{burst.Origin}, WithAlpha(Colors::White, static_cast<uint8_t>(110.0f * invT)), BlendMode::Add);
                break;
            }
            case DeathEffectStyle::ClassicBurst:
            {
                const float r = (10.0f + 42.0f * t) * burst.Scale;
                renderer.DrawCircle(r * 0.65f, Transform2D{burst.Origin}, WithAlpha(burst.Primary, static_cast<uint8_t>(140.0f * invT)), BlendMode::Add);
                renderer.DrawCircleOutline(r, Transform2D{burst.Origin}, WithAlpha(burst.Secondary, static_cast<uint8_t>(180.0f * invT)), std::max(1.2f, 2.8f * invT * burst.Scale));
                break;
            }
            case DeathEffectStyle::ShockwaveRing:
            {
                const float r1 = (12.0f + 92.0f * (1.0f - invT * invT)) * burst.Scale;
                const float r2 = (6.0f + 62.0f * t) * burst.Scale;
                renderer.DrawCircleOutline(r1, Transform2D{burst.Origin}, WithAlpha(burst.Primary, static_cast<uint8_t>(235.0f * invT)), std::max(1.5f, 5.0f * invT * burst.Scale));
                renderer.DrawCircleOutline(r2, Transform2D{burst.Origin}, WithAlpha(burst.Secondary, static_cast<uint8_t>(205.0f * invT)), std::max(1.2f, 3.0f * invT * burst.Scale));
                renderer.DrawCircle(24.0f * invT * burst.Scale, Transform2D{burst.Origin}, WithAlpha(Colors::White, static_cast<uint8_t>(180.0f * invT)), BlendMode::Add);
                break;
            }
            case DeathEffectStyle::PixelShatter:
            {
                const float box = (14.0f + 44.0f * t) * burst.Scale;
                renderer.DrawRectangleOutline(box, box, Transform2D{burst.Origin}, WithAlpha(burst.Primary, static_cast<uint8_t>(210.0f * invT)), std::max(1.5f, 3.5f * invT * burst.Scale));
                break;
            }
            case DeathEffectStyle::PrismConfetti:
            {
                const Color prism = TrailPresets::HsvToRgb(std::fmod(t * 360.0f, 360.0f), 0.9f, 1.0f);
                const float r = (14.0f + 56.0f * t) * burst.Scale;
                renderer.DrawCircleOutline(r, Transform2D{burst.Origin}, WithAlpha(prism, static_cast<uint8_t>(175.0f * invT)), std::max(1.2f, 3.0f * invT * burst.Scale));
                break;
            }
            case DeathEffectStyle::NovaImplosion:
            {
                const float flareLen = (26.0f + 115.0f * (1.0f - invT * invT)) * burst.Scale;
                const float flareThick = std::max(1.5f, 11.0f * invT * burst.Scale);
                renderer.DrawRectangle(flareLen, flareThick, Transform2D{burst.Origin}, WithAlpha(burst.Primary, static_cast<uint8_t>(220.0f * invT)), BlendMode::Add);
                renderer.DrawRectangle(flareThick, flareLen, Transform2D{burst.Origin}, WithAlpha(burst.Primary, static_cast<uint8_t>(220.0f * invT)), BlendMode::Add);
                renderer.DrawRectangle(flareLen * 0.65f, flareThick * 0.55f, Transform2D{burst.Origin, 45.0f}, WithAlpha(burst.Secondary, static_cast<uint8_t>(210.0f * invT)), BlendMode::Add);
                renderer.DrawRectangle(flareLen * 0.65f, flareThick * 0.55f, Transform2D{burst.Origin, -45.0f}, WithAlpha(burst.Secondary, static_cast<uint8_t>(210.0f * invT)), BlendMode::Add);
                renderer.DrawCircle(30.0f * invT * burst.Scale, Transform2D{burst.Origin}, WithAlpha(Colors::White, static_cast<uint8_t>(235.0f * invT)), BlendMode::Add);
                break;
            }
            case DeathEffectStyle::MatrixGlitch:
            {
                const float w = (40.0f + 75.0f * t) * burst.Scale;
                const float h = (18.0f + 26.0f * t) * burst.Scale;
                renderer.DrawRectangleOutline(w, h, Transform2D{burst.Origin + Vector2f{4.0f * invT * burst.Scale, 0.0f}}, WithAlpha(burst.Primary, static_cast<uint8_t>(200.0f * invT)), 2.0f);
                renderer.DrawRectangleOutline(w * 0.85f, h * 0.65f, Transform2D{burst.Origin + Vector2f{-4.0f * invT * burst.Scale, 2.0f}}, WithAlpha(burst.Secondary, static_cast<uint8_t>(185.0f * invT)), 1.5f);
                break;
            }
        }

        // Particles
        for (const auto& p : burst.Particles)
        {
            if (p.Life <= 0.0f || p.MaxLife <= 0.0f) continue;
            const float ratio = std::clamp(p.Life / p.MaxLife, 0.0f, 1.0f);
            Color c = p.Tint;
            c.a = static_cast<uint8_t>(255.0f * ratio);

            switch (p.ShapeKind)
            {
                case 0: // Square / Pixel
                    renderer.DrawRectangle(p.Size * (0.4f + 0.6f * ratio), p.Size * (0.4f + 0.6f * ratio), Transform2D{p.Position, p.Angle}, c, BlendMode::Add);
                    break;
                case 1: // Diamond
                    renderer.DrawRectangle(p.Size, p.Size, Transform2D{p.Position, p.Angle + 45.0f}, c, BlendMode::Add);
                    break;
                case 2: // Directional shard / glitch bar
                    renderer.DrawRectangle(p.Size * 1.8f, std::max(2.0f, p.Size * 0.28f), Transform2D{p.Position, p.Angle}, c, BlendMode::Add);
                    break;
                case 3: // Binary 0
                    CosmeticRenderer::DrawBinaryDigit(renderer, p.Position, p.Size, false, c, BlendMode::Add);
                    break;
                case 4: // Binary 1
                    CosmeticRenderer::DrawBinaryDigit(renderer, p.Position, p.Size, true, c, BlendMode::Add);
                    break;
                default:
                    break;
            }
        }
    }
}

void CosmeticExplosionPool::Clear()
{
    m_bursts.clear();
}

// =============================================================================
// PADDLE SKIN RENDERER (8 ARCHETYPES — ORIGINAL SPRITE + 7 PROCEDURAL)
// =============================================================================

void CosmeticRenderer::DrawPaddle(
    Renderer&       renderer,
    const Vector2f& pos,
    const Vector2f& size,
    float           rotationDeg,
    PaddleSkinStyle style,
    Color           primary,
    Color           secondary,
    float           animTime,
    float           alphaScale,
    uint32_t        spriteTexId)
{
    if (alphaScale <= 0.01f || size.X <= 1.0f || size.Y <= 1.0f) return;

    const float w = size.X;
    const float h = size.Y;
    const float rad = rotationDeg * kDegToRad;
    const float cosR = std::cos(rad);
    const float sinR = std::sin(rad);
    const auto toW = [&](float lx, float ly) { return LocalToWorld(pos, cosR, sinR, lx, ly); };

    switch (style)
    {
        case PaddleSkinStyle::ClassicSprite:
        {
            if (spriteTexId != 0)
            {
                const Vector2f texSize = renderer.GetTextureSize(spriteTexId);
                if (texSize.X > 0.0f && texSize.Y > 0.0f)
                {
                    SpriteComponent sp;
                    sp.TextureId = spriteTexId;
                    sp.Tint = ScaleAlpha(primary, alphaScale);
                    renderer.DrawSprite(sp, Transform2D{pos, rotationDeg, Vector2f{w / texSize.X, h / texSize.Y}}, BlendMode::Alpha);
                    break;
                }
            }
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(primary, alphaScale));
            break;
        }

        case PaddleSkinStyle::Cyber:
        {
            // Subtle outer additive neon aura
            renderer.DrawRectangle(w + 8.0f, h + 6.0f, Transform2D{pos, rotationDeg}, WithAlpha(primary, 42, alphaScale), BlendMode::Add);

            // Dark metallic hull body
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, WithAlpha(Color{18, 26, 40, 255}, 250, alphaScale));
            // Top glossy bevel highlight
            renderer.DrawRectangle(w * 0.94f, h * 0.34f, Transform2D{toW(0.0f, -h * 0.24f), rotationDeg}, WithAlpha(primary, 95, alphaScale));
            // Primary outer frame
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(primary, alphaScale), std::max(1.8f, h * 0.11f));

            // Twin glowing thruster side caps
            const float capW = std::max(10.0f, w * 0.13f);
            renderer.DrawRectangle(capW, h * 0.84f, Transform2D{toW(-w * 0.5f + capW * 0.55f, 0.0f), rotationDeg}, ScaleAlpha(primary, alphaScale));
            renderer.DrawRectangle(capW, h * 0.84f, Transform2D{toW( w * 0.5f - capW * 0.55f, 0.0f), rotationDeg}, ScaleAlpha(primary, alphaScale));
            renderer.DrawRectangle(capW * 0.45f, h * 0.52f, Transform2D{toW(-w * 0.5f + capW * 0.55f, 0.0f), rotationDeg}, ScaleAlpha(secondary, alphaScale));
            renderer.DrawRectangle(capW * 0.45f, h * 0.52f, Transform2D{toW( w * 0.5f - capW * 0.55f, 0.0f), rotationDeg}, ScaleAlpha(secondary, alphaScale));

            // Pulsing central reactor bar
            const float pulse = 0.72f + 0.28f * std::sin(animTime * 4.5f);
            renderer.DrawRectangle(w * 0.42f, h * 0.30f, Transform2D{pos, rotationDeg}, WithAlpha(secondary, static_cast<uint8_t>(220 * pulse), alphaScale), BlendMode::Add);
            renderer.DrawRectangle(w * 0.22f, h * 0.16f, Transform2D{pos, rotationDeg}, WithAlpha(Colors::White, static_cast<uint8_t>(225 * pulse), alphaScale), BlendMode::Add);
            break;
        }

        case PaddleSkinStyle::Neon:
        {
            const float pulse = 0.78f + 0.22f * std::sin(animTime * 6.0f);
            // Wide double neon tube bloom
            renderer.DrawRectangle(w + 14.0f, h + 10.0f, Transform2D{pos, rotationDeg}, WithAlpha(primary, static_cast<uint8_t>(48 * pulse), alphaScale), BlendMode::Add);
            renderer.DrawRectangle(w + 6.0f, h + 4.0f, Transform2D{pos, rotationDeg}, WithAlpha(primary, static_cast<uint8_t>(75 * pulse), alphaScale), BlendMode::Add);

            // Hollow dark neon glass interior
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, WithAlpha(Color{10, 14, 24, 215}, 215, alphaScale));
            // Outer neon tube border
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(primary, alphaScale), std::max(2.4f, h * 0.16f));
            // Inner white-hot neon filament line
            renderer.DrawRectangleOutline(w * 0.92f, h * 0.72f, Transform2D{pos, rotationDeg}, WithAlpha(Colors::White, static_cast<uint8_t>(205 * pulse), alphaScale), std::max(1.0f, h * 0.06f));

            // Animated scanning plasma pulse bar inside tube
            const float scanX = std::sin(animTime * 3.0f) * (w * 0.26f);
            renderer.DrawRectangle(w * 0.30f, h * 0.34f, Transform2D{toW(scanX, 0.0f), rotationDeg}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            renderer.DrawCircle(h * 0.34f, Transform2D{toW(-w * 0.44f, 0.0f)}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            renderer.DrawCircle(h * 0.34f, Transform2D{toW( w * 0.44f, 0.0f)}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            break;
        }

        case PaddleSkinStyle::RetroWood:
        {
            // Authentic 1980s warm walnut woodgrain body
            const Color woodBase{116, 66, 32, 255};
            const Color woodDark{72, 38, 16, 255};
            const Color woodLight{152, 92, 48, 255};
            const Color brass{212, 162, 74, 255};

            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(woodBase, alphaScale));
            // Horizontal woodgrain planks/streaks
            renderer.DrawRectangle(w * 0.96f, h * 0.16f, Transform2D{toW(0.0f, -h * 0.30f), rotationDeg}, ScaleAlpha(woodLight, alphaScale));
            renderer.DrawRectangle(w * 0.92f, h * 0.11f, Transform2D{toW(-w * 0.02f, -h * 0.04f), rotationDeg}, ScaleAlpha(woodDark, alphaScale));
            renderer.DrawRectangle(w * 0.94f, h * 0.14f, Transform2D{toW( w * 0.01f,  h * 0.28f), rotationDeg}, ScaleAlpha(woodDark, alphaScale));

            // Brushed brass corner end-caps with dark screws
            const float capW = std::max(11.0f, w * 0.12f);
            renderer.DrawRectangle(capW, h, Transform2D{toW(-w * 0.5f + capW * 0.5f, 0.0f), rotationDeg}, ScaleAlpha(brass, alphaScale));
            renderer.DrawRectangle(capW, h, Transform2D{toW( w * 0.5f - capW * 0.5f, 0.0f), rotationDeg}, ScaleAlpha(brass, alphaScale));
            renderer.DrawCircle(std::max(1.8f, h * 0.12f), Transform2D{toW(-w * 0.5f + capW * 0.5f, 0.0f)}, ScaleAlpha(Color{50, 32, 16, 255}, alphaScale));
            renderer.DrawCircle(std::max(1.8f, h * 0.12f), Transform2D{toW( w * 0.5f - capW * 0.5f, 0.0f)}, ScaleAlpha(Color{50, 32, 16, 255}, alphaScale));

            // Glowing arcade cabinet recessed stripe in Primary & Secondary
            renderer.DrawRectangle(w * 0.64f, h * 0.34f, Transform2D{pos, rotationDeg}, ScaleAlpha(primary, alphaScale));
            renderer.DrawRectangle(w * 0.32f, h * 0.16f, Transform2D{pos, rotationDeg}, ScaleAlpha(secondary, alphaScale));
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(Color{52, 28, 12, 255}, alphaScale), std::max(1.5f, h * 0.10f));
            break;
        }

        case PaddleSkinStyle::Golden:
        {
            const Color goldMid{232, 180, 42, 255};
            const Color goldBright{255, 232, 120, 255};
            const Color goldShadow{148, 102, 16, 255};

            // Warm golden champion glow
            renderer.DrawRectangle(w + 10.0f, h + 6.0f, Transform2D{pos, rotationDeg}, WithAlpha(goldBright, 45, alphaScale), BlendMode::Add);

            // Chamfered gold alloy layers
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(goldMid, alphaScale));
            renderer.DrawRectangle(w * 0.96f, h * 0.36f, Transform2D{toW(0.0f, -h * 0.24f), rotationDeg}, ScaleAlpha(goldBright, alphaScale));
            renderer.DrawRectangle(w * 0.96f, h * 0.24f, Transform2D{toW(0.0f,  h * 0.32f), rotationDeg}, ScaleAlpha(goldShadow, alphaScale));

            // Side jewel inlays in Primary color
            const float inlayW = w * 0.18f;
            renderer.DrawRectangle(inlayW, h * 0.36f, Transform2D{toW(-w * 0.28f, 0.0f), rotationDeg}, ScaleAlpha(primary, alphaScale));
            renderer.DrawRectangle(inlayW, h * 0.36f, Transform2D{toW( w * 0.28f, 0.0f), rotationDeg}, ScaleAlpha(primary, alphaScale));

            // Animated specular glint sweep across chassis
            const float sweepPhase = std::fmod(animTime * 0.55f, 1.6f);
            if (sweepPhase < 1.0f)
            {
                const float gx = (-0.48f + sweepPhase * 0.96f) * w;
                renderer.DrawRectangle(w * 0.12f, h * 0.92f, Transform2D{toW(gx, 0.0f), rotationDeg + 18.0f}, WithAlpha(Colors::White, 160, alphaScale), BlendMode::Add);
            }

            // Central diamond medallion in Secondary
            const float dia = h * 0.56f;
            renderer.DrawRectangle(dia, dia, Transform2D{pos, rotationDeg + 45.0f}, ScaleAlpha(secondary, alphaScale));
            renderer.DrawRectangle(dia * 0.52f, dia * 0.52f, Transform2D{pos, rotationDeg + 45.0f}, ScaleAlpha(Colors::White, alphaScale));
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(goldBright, alphaScale), std::max(1.8f, h * 0.10f));
            break;
        }

        case PaddleSkinStyle::Steampunk:
        {
            const Color copperDark{118, 62, 32, 255};
            const Color copperMid{176, 102, 52, 255};
            const Color brassBright{232, 186, 86, 255};

            // Copper pressure-vessel hull
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(copperDark, alphaScale));
            renderer.DrawRectangle(w * 0.94f, h * 0.68f, Transform2D{pos, rotationDeg}, ScaleAlpha(copperMid, alphaScale));

            // Copper top & bottom steam pipes
            renderer.DrawRectangle(w * 0.86f, h * 0.15f, Transform2D{toW(0.0f, -h * 0.38f), rotationDeg}, ScaleAlpha(brassBright, alphaScale));
            renderer.DrawRectangle(w * 0.86f, h * 0.15f, Transform2D{toW(0.0f,  h * 0.38f), rotationDeg}, ScaleAlpha(brassBright, alphaScale));

            // Dual spinning clockwork gears flanking the central steam gauge!
            const float gearR = std::max(5.5f, h * 0.44f);
            DrawMiniGear(renderer, toW(-w * 0.30f, 0.0f), gearR,  animTime * 95.0f + rotationDeg, brassBright, primary, alphaScale);
            DrawMiniGear(renderer, toW( w * 0.30f, 0.0f), gearR, -animTime * 95.0f + rotationDeg, brassBright, primary, alphaScale);

            // Rivets along flanks
            const float rivetR = std::max(1.5f, h * 0.09f);
            renderer.DrawCircle(rivetR, Transform2D{toW(-w * 0.44f, -h * 0.24f)}, ScaleAlpha(brassBright, alphaScale));
            renderer.DrawCircle(rivetR, Transform2D{toW(-w * 0.44f,  h * 0.24f)}, ScaleAlpha(brassBright, alphaScale));
            renderer.DrawCircle(rivetR, Transform2D{toW( w * 0.44f, -h * 0.24f)}, ScaleAlpha(brassBright, alphaScale));
            renderer.DrawCircle(rivetR, Transform2D{toW( w * 0.44f,  h * 0.24f)}, ScaleAlpha(brassBright, alphaScale));

            // Glowing pressurized glass core in Primary & Secondary
            const float pulse = 0.78f + 0.22f * std::sin(animTime * 5.0f);
            renderer.DrawRectangle(w * 0.24f, h * 0.54f, Transform2D{pos, rotationDeg}, WithAlpha(primary, static_cast<uint8_t>(235 * pulse), alphaScale));
            renderer.DrawRectangle(w * 0.14f, h * 0.26f, Transform2D{pos, rotationDeg}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            renderer.DrawRectangleOutline(w * 0.26f, h * 0.60f, Transform2D{pos, rotationDeg}, ScaleAlpha(brassBright, alphaScale), 1.5f);
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(Color{62, 32, 16, 255}, alphaScale), std::max(1.5f, h * 0.10f));
            break;
        }

        case PaddleSkinStyle::VoidObsidian:
        {
            const float pulse = 0.70f + 0.30f * std::sin(animTime * 4.0f);
            // Stealth obsidian dark core + glowing void aura
            renderer.DrawRectangle(w + 10.0f, h + 6.0f, Transform2D{pos, rotationDeg}, WithAlpha(primary, static_cast<uint8_t>(42 * pulse), alphaScale), BlendMode::Add);
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(Color{16, 14, 24, 255}, alphaScale));
            renderer.DrawRectangle(w * 0.88f, h * 0.62f, Transform2D{pos, rotationDeg}, ScaleAlpha(Color{28, 22, 42, 255}, alphaScale));

            // Slanted plasma armor vents
            for (int side = -1; side <= 1; side += 2)
            {
                const float s = static_cast<float>(side);
                renderer.DrawRectangle(w * 0.11f, h * 0.22f, Transform2D{toW(s * w * 0.26f, 0.0f), rotationDeg + s * 28.0f}, ScaleAlpha(primary, alphaScale), BlendMode::Add);
                renderer.DrawRectangle(w * 0.08f, h * 0.18f, Transform2D{toW(s * w * 0.38f, 0.0f), rotationDeg + s * 28.0f}, ScaleAlpha(primary, alphaScale), BlendMode::Add);
            }

            // Pulsing singularity diamond core
            renderer.DrawCircle(h * 0.42f, Transform2D{pos}, WithAlpha(secondary, static_cast<uint8_t>(185 * pulse), alphaScale), BlendMode::Add);
            renderer.DrawCircleOutline(h * 0.46f, Transform2D{pos}, ScaleAlpha(primary, alphaScale), 1.8f);
            renderer.DrawCircle(h * 0.22f, Transform2D{pos}, ScaleAlpha(Color{14, 10, 22, 255}, alphaScale));
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(primary, alphaScale), std::max(1.8f, h * 0.10f));
            break;
        }

        case PaddleSkinStyle::PrismStriker:
        {
            const Color prismA = TrailPresets::HsvToRgb(std::fmod(animTime * 90.0f, 360.0f), 0.85f, 1.0f);
            const Color prismB = TrailPresets::HsvToRgb(std::fmod(animTime * 90.0f + 120.0f, 360.0f), 0.85f, 1.0f);

            renderer.DrawRectangle(w + 10.0f, h + 6.0f, Transform2D{pos, rotationDeg}, WithAlpha(prismA, 50, alphaScale), BlendMode::Add);
            renderer.DrawRectangle(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(Darken(primary, 0.32f), alphaScale));

            // Faceted crystalline segments
            renderer.DrawRectangle(w * 0.32f, h * 0.78f, Transform2D{toW(-w * 0.32f, 0.0f), rotationDeg}, BlendColor(primary, prismA, 0.45f, alphaScale));
            renderer.DrawRectangle(w * 0.32f, h * 0.78f, Transform2D{toW( w * 0.32f, 0.0f), rotationDeg}, BlendColor(primary, prismB, 0.45f, alphaScale));
            renderer.DrawRectangle(w * 0.36f, h * 0.84f, Transform2D{pos, rotationDeg}, ScaleAlpha(secondary, alphaScale));
            renderer.DrawRectangle(h * 0.58f, h * 0.58f, Transform2D{pos, rotationDeg + 45.0f}, ScaleAlpha(Colors::White, alphaScale), BlendMode::Add);
            renderer.DrawRectangleOutline(w, h, Transform2D{pos, rotationDeg}, ScaleAlpha(prismA, alphaScale), std::max(1.8f, h * 0.11f));
            break;
        }
    }
}

// =============================================================================
// BALL SKIN RENDERER (8 ARCHETYPES — ORIGINAL SPRITE + 7 PROCEDURAL)
// =============================================================================

void CosmeticRenderer::DrawBall(
    Renderer&       renderer,
    const Vector2f& pos,
    float           radius,
    BallSkinStyle   style,
    Color           primary,
    Color           secondary,
    float           animTime,
    float           alphaScale,
    bool            isFireBall,
    bool            isGhost,
    bool            isTempo,
    uint32_t        spriteTexId)
{
    if (alphaScale <= 0.01f || radius <= 0.5f) return;

    // Gameplay power-up visual clarity overrides (FireBall / Tempo / Ghost)
    if (isFireBall)
    {
        const float pulse = 0.85f + 0.15f * std::sin(animTime * 22.0f);
        renderer.DrawCircle(radius * 1.45f * pulse, Transform2D{pos}, WithAlpha(Color{255, 110, 20, 255}, 125, alphaScale), BlendMode::Add);
        renderer.DrawCircle(radius, Transform2D{pos}, WithAlpha(Color{255, 175, 40, 255}, 255, alphaScale));
        renderer.DrawCircle(radius * 0.62f, Transform2D{pos}, WithAlpha(Color{255, 245, 160, 255}, 255, alphaScale));
        return;
    }

    if (isGhost)
    {
        alphaScale *= 0.52f;
        primary = BlendColor(primary, Color{150, 235, 255, 255}, 0.55f);
    }
    else if (isTempo)
    {
        primary = BlendColor(primary, Color{210, 145, 255, 255}, 0.65f);
        secondary = Color{240, 210, 255, 255};
    }

    switch (style)
    {
        case BallSkinStyle::ClassicSprite:
        {
            if (spriteTexId != 0)
            {
                const Vector2f texSize = renderer.GetTextureSize(spriteTexId);
                if (texSize.X > 0.0f && texSize.Y > 0.0f)
                {
                    const float diameter = radius * 2.0f;
                    SpriteComponent sp;
                    sp.TextureId = spriteTexId;
                    sp.Tint = ScaleAlpha(primary, alphaScale);
                    renderer.DrawSprite(sp, Transform2D{pos, 0.0f, Vector2f{diameter / texSize.X, diameter / texSize.Y}}, BlendMode::Alpha);
                    break;
                }
            }
            renderer.DrawCircle(radius, Transform2D{pos}, ScaleAlpha(primary, alphaScale));
            break;
        }

        case BallSkinStyle::ClassicCore:
        {
            renderer.DrawCircle(radius * 1.38f, Transform2D{pos}, WithAlpha(primary, 60, alphaScale), BlendMode::Add);
            renderer.DrawCircle(radius, Transform2D{pos}, ScaleAlpha(primary, alphaScale));
            renderer.DrawCircleOutline(radius, Transform2D{pos}, ScaleAlpha(secondary, alphaScale), std::max(1.4f, radius * 0.14f));
            // Crisp specular highlight dot
            renderer.DrawCircle(radius * 0.34f, Transform2D{pos + Vector2f{-radius * 0.28f, -radius * 0.28f}}, WithAlpha(Colors::White, 225, alphaScale));
            break;
        }

        case BallSkinStyle::CyberGyro:
        {
            renderer.DrawCircle(radius * 1.35f, Transform2D{pos}, WithAlpha(primary, 55, alphaScale), BlendMode::Add);
            renderer.DrawCircle(radius * 0.94f, Transform2D{pos}, ScaleAlpha(Color{16, 22, 34, 255}, alphaScale));
            renderer.DrawCircleOutline(radius, Transform2D{pos}, ScaleAlpha(primary, alphaScale), std::max(1.8f, radius * 0.16f));

            // Rotating inner diamond gyro reticle
            const float rBox = radius * 1.12f;
            renderer.DrawRectangleOutline(rBox, rBox, Transform2D{pos, animTime * 150.0f}, ScaleAlpha(secondary, alphaScale), std::max(1.3f, radius * 0.11f));
            renderer.DrawRectangleOutline(rBox * 0.72f, rBox * 0.72f, Transform2D{pos, -animTime * 210.0f}, ScaleAlpha(primary, alphaScale), std::max(1.2f, radius * 0.10f));
            renderer.DrawCircle(radius * 0.38f, Transform2D{pos}, ScaleAlpha(Colors::White, alphaScale), BlendMode::Add);
            break;
        }

        case BallSkinStyle::NovaStar:
        {
            renderer.DrawCircle(radius * 1.45f, Transform2D{pos}, WithAlpha(primary, 65, alphaScale), BlendMode::Add);
            renderer.DrawCircle(radius * 0.88f, Transform2D{pos}, WithAlpha(Darken(primary, 0.35f), 220, alphaScale));
            renderer.DrawCircleOutline(radius * 0.94f, Transform2D{pos}, ScaleAlpha(secondary, alphaScale), std::max(1.3f, radius * 0.10f));

            // Spinning 4-point starflare
            const float spin = animTime * 165.0f;
            renderer.DrawRectangle(radius * 1.85f, radius * 0.42f, Transform2D{pos, spin}, ScaleAlpha(primary, alphaScale), BlendMode::Add);
            renderer.DrawRectangle(radius * 0.42f, radius * 1.85f, Transform2D{pos, spin}, ScaleAlpha(primary, alphaScale), BlendMode::Add);
            renderer.DrawRectangle(radius * 1.05f, radius * 0.32f, Transform2D{pos, spin + 45.0f}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            renderer.DrawRectangle(radius * 0.32f, radius * 1.05f, Transform2D{pos, spin + 45.0f}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            renderer.DrawCircle(radius * 0.32f, Transform2D{pos}, ScaleAlpha(Colors::White, alphaScale));
            break;
        }

        case BallSkinStyle::RetroPixel:
        {
            const float s = radius * 1.72f;
            // Stepped pixel-art outline (cross of two rectangles to look like chunky 8-bit orb!)
            renderer.DrawRectangle(s * 1.08f, s * 0.72f, Transform2D{pos}, ScaleAlpha(Color{18, 22, 30, 255}, alphaScale));
            renderer.DrawRectangle(s * 0.72f, s * 1.08f, Transform2D{pos}, ScaleAlpha(Color{18, 22, 30, 255}, alphaScale));
            renderer.DrawRectangle(s * 0.92f, s * 0.92f, Transform2D{pos}, ScaleAlpha(Color{18, 22, 30, 255}, alphaScale));

            // Stepped pixel fill in primary
            renderer.DrawRectangle(s * 0.92f, s * 0.56f, Transform2D{pos}, ScaleAlpha(primary, alphaScale));
            renderer.DrawRectangle(s * 0.56f, s * 0.92f, Transform2D{pos}, ScaleAlpha(primary, alphaScale));
            renderer.DrawRectangle(s * 0.74f, s * 0.74f, Transform2D{pos}, ScaleAlpha(primary, alphaScale));

            // Pixel shading block in secondary & white glint pixel
            renderer.DrawRectangle(s * 0.28f, s * 0.28f, Transform2D{pos + Vector2f{ s * 0.16f,  s * 0.16f}}, ScaleAlpha(secondary, alphaScale));
            renderer.DrawRectangle(s * 0.24f, s * 0.24f, Transform2D{pos + Vector2f{-s * 0.20f, -s * 0.20f}}, ScaleAlpha(Colors::White, alphaScale));
            break;
        }

        case BallSkinStyle::NeonSawblade:
        {
            // Geometry Dash iconic spinning sawblade!
            const float baseAngle = animTime * 4.8f; // radians for vertices
            constexpr int kTeeth = 8;
            std::vector<Vertex> teeth;
            teeth.reserve(kTeeth * 3);
            const Color toothCol = ScaleAlpha(primary, alphaScale);
            const Color tipCol = ScaleAlpha(secondary, alphaScale);

            for (int i = 0; i < kTeeth; ++i)
            {
                const float a0 = baseAngle + (static_cast<float>(i) / kTeeth) * 2.0f * kPi;
                const float a1 = a0 + (2.0f * kPi / kTeeth) * 0.65f;
                const float aTip = a0 + (2.0f * kPi / kTeeth) * 0.20f;
                const float innerR = radius * 0.68f;
                const float outerR = radius * 1.18f;

                teeth.push_back(Vertex{pos.X + std::cos(a0) * innerR, pos.Y + std::sin(a0) * innerR, toothCol, 0.0f, 0.0f});
                teeth.push_back(Vertex{pos.X + std::cos(a1) * innerR, pos.Y + std::sin(a1) * innerR, toothCol, 0.0f, 0.0f});
                teeth.push_back(Vertex{pos.X + std::cos(aTip) * outerR, pos.Y + std::sin(aTip) * outerR, tipCol, 0.0f, 0.0f});
            }
            renderer.DrawVertices(teeth, PrimitiveType::Triangles, 0, BlendMode::Alpha);

            renderer.DrawCircle(radius * 0.74f, Transform2D{pos}, ScaleAlpha(Color{18, 22, 34, 255}, alphaScale));
            renderer.DrawCircleOutline(radius * 0.74f, Transform2D{pos}, ScaleAlpha(primary, alphaScale), std::max(1.5f, radius * 0.14f));
            renderer.DrawCircle(radius * 0.34f, Transform2D{pos}, ScaleAlpha(secondary, alphaScale));
            renderer.DrawCircle(radius * 0.16f, Transform2D{pos}, ScaleAlpha(Colors::White, alphaScale));
            break;
        }

        case BallSkinStyle::ClockworkGear:
        {
            const Color brass{226, 174, 76, 255};
            DrawMiniGear(renderer, pos, radius * 1.08f, animTime * 120.0f, brass, secondary, alphaScale);
            renderer.DrawCircleOutline(radius * 0.66f, Transform2D{pos}, ScaleAlpha(primary, alphaScale), std::max(1.5f, radius * 0.16f));
            break;
        }

        case BallSkinStyle::OmegaCore:
        {
            const float pulse = 0.80f + 0.20f * std::sin(animTime * 6.5f);
            renderer.DrawCircle(radius * 1.42f * pulse, Transform2D{pos}, WithAlpha(primary, 68, alphaScale), BlendMode::Add);
            renderer.DrawCircle(radius, Transform2D{pos}, ScaleAlpha(Color{14, 10, 24, 255}, alphaScale));
            renderer.DrawCircleOutline(radius, Transform2D{pos}, ScaleAlpha(secondary, alphaScale), std::max(1.8f, radius * 0.15f));
            // Piercing glowing vertical iris slit
            renderer.DrawRectangle(radius * 0.34f, radius * 1.16f, Transform2D{pos}, ScaleAlpha(primary, alphaScale), BlendMode::Add);
            renderer.DrawCircle(radius * 0.26f, Transform2D{pos}, ScaleAlpha(Colors::White, alphaScale));
            // Twin orbiting satellite sparks
            const float orbA = animTime * 4.2f;
            const Vector2f s1 = pos + Vector2f{std::cos(orbA) * radius * 1.25f, std::sin(orbA) * radius * 1.25f};
            const Vector2f s2 = pos + Vector2f{std::cos(orbA + kPi) * radius * 1.25f, std::sin(orbA + kPi) * radius * 1.25f};
            renderer.DrawCircle(std::max(2.0f, radius * 0.20f), Transform2D{s1}, ScaleAlpha(primary, alphaScale), BlendMode::Add);
            renderer.DrawCircle(std::max(2.0f, radius * 0.20f), Transform2D{s2}, ScaleAlpha(secondary, alphaScale), BlendMode::Add);
            break;
        }
    }
}

// =============================================================================
// PARTICLE TRAIL BUILDER & RENDERER
// =============================================================================

TrailConfig CosmeticRenderer::BuildTrailConfig(TrailStyle style, Color primary, Color secondary)
{
    TrailConfig cfg;
    cfg.Blend = BlendMode::Add;
    cfg.FadeWidth = true;
    cfg.FadeAlpha = true;
    cfg.MinPointDistance = 4.0f;
    cfg.MaxPointDistance = 42.0f;

    switch (style)
    {
        case TrailStyle::ClassicWhite:
            return TrailPresets::GetPreset(TrailCosmeticId::DefaultNeon);

        case TrailStyle::Ribbon:
            cfg.CosmeticId = TrailCosmeticId::Custom;
            cfg.Shape = TrailShape::Ribbon;
            cfg.Lifetime = 0.26f;
            cfg.StartWidth = 19.0f;
            cfg.EndWidth = 2.0f;
            cfg.StartColor = Color{primary.r, primary.g, primary.b, 225};
            cfg.EndColor   = Color{secondary.r, secondary.g, secondary.b, 0};
            break;

        case TrailStyle::Stardust:
            cfg.CosmeticId = TrailCosmeticId::Custom;
            cfg.Shape = TrailShape::Stardust;
            cfg.Lifetime = 0.42f;
            cfg.StartWidth = 18.0f;
            cfg.EndWidth = 2.0f;
            cfg.StartColor = Color{primary.r, primary.g, primary.b, 235};
            cfg.EndColor   = Color{secondary.r, secondary.g, secondary.b, 0};
            break;

        case TrailStyle::Flame:
            cfg.CosmeticId = TrailCosmeticId::FireStreak;
            cfg.Shape = TrailShape::Flame;
            cfg.Lifetime = 0.30f;
            cfg.StartWidth = 23.0f;
            cfg.EndWidth = 3.0f;
            cfg.StartColor = Color{255, 195, 45, 240};
            cfg.EndColor   = Color{255, 40, 10, 0};
            break;

        case TrailStyle::MatrixBinary:
            cfg.CosmeticId = TrailCosmeticId::Custom;
            cfg.Shape = TrailShape::MatrixBinary;
            cfg.Lifetime = 0.44f;
            cfg.StartWidth = 18.0f;
            cfg.EndWidth = 5.0f;
            cfg.MinPointDistance = 10.0f;
            cfg.StartColor = Color{primary.r, primary.g, primary.b, 235};
            cfg.EndColor   = Color{secondary.r, secondary.g, secondary.b, 0};
            break;

        case TrailStyle::Rainbow:
            cfg.CosmeticId = TrailCosmeticId::Rainbow;
            cfg.Shape = TrailShape::Ribbon;
            cfg.Lifetime = 0.32f;
            cfg.StartWidth = 21.0f;
            cfg.EndWidth = 3.0f;
            cfg.StartColor = Color{255, 50, 150, 230};
            cfg.EndColor   = Color{50, 200, 255, 0};
            break;

        case TrailStyle::ArcPulse:
            cfg.CosmeticId = TrailCosmeticId::Custom;
            cfg.Shape = TrailShape::ArcPulse;
            cfg.Lifetime = 0.28f;
            cfg.StartWidth = 16.0f;
            cfg.EndWidth = 2.0f;
            cfg.StartColor = Color{primary.r, primary.g, primary.b, 240};
            cfg.EndColor   = Color{secondary.r, secondary.g, secondary.b, 0};
            break;
    }

    return cfg;
}

TrailConfig CosmeticRenderer::BuildEquippedTrailConfig(const ProfileManager& profile)
{
    return BuildTrailConfig(
        profile.GetEquippedTrailStyle(),
        profile.GetEquippedPrimaryColor(),
        profile.GetEquippedSecondaryColor());
}

void CosmeticRenderer::DrawBinaryDigit(Renderer& renderer, const Vector2f& pos, float size, bool isOne, Color color, BlendMode blend)
{
    const float w = std::max(3.5f, size * 0.62f);
    const float h = std::max(5.0f, size);
    const float stroke = std::max(1.2f, size * 0.16f);

    if (isOne)
    {
        renderer.DrawRectangle(stroke, h, Transform2D{pos}, color, blend);
        renderer.DrawRectangle(w * 0.55f, stroke, Transform2D{pos + Vector2f{-w * 0.14f, -h * 0.36f}}, color, blend);
        renderer.DrawRectangle(w * 0.72f, stroke, Transform2D{pos + Vector2f{0.0f, h * 0.44f}}, color, blend);
    }
    else
    {
        renderer.DrawRectangleOutline(w, h, Transform2D{pos}, color, stroke);
    }
}

void CosmeticRenderer::RenderTrail(
    Renderer&             renderer,
    const TrailComponent& trail,
    float                 animTime,
    std::vector<Vertex>&  scratch)
{
    if (trail.Points.size() < 2) return;

    const auto& cfg = trail.Config;
    const size_t numPoints = trail.Points.size();

    auto sampleColor = [&](float t, float age) -> Color {
        Color c = cfg.StartColor;
        if (cfg.CosmeticId == TrailCosmeticId::Rainbow)
        {
            const float hue = std::fmod(t * 360.0f + animTime * 210.0f + age * 140.0f, 360.0f);
            c = TrailPresets::HsvToRgb(hue, 0.92f, 1.0f);
        }
        else
        {
            c.r = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.r + t * cfg.EndColor.r);
            c.g = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.g + t * cfg.EndColor.g);
            c.b = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.b + t * cfg.EndColor.b);
        }
        if (cfg.FadeAlpha)
        {
            c.a = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.a + t * cfg.EndColor.a);
        }
        else
        {
            c.a = cfg.StartColor.a;
        }
        return c;
    };

    if (cfg.Shape == TrailShape::Particles)
    {
        for (size_t i = 0; i < numPoints; ++i)
        {
            const auto& pt = trail.Points[i];
            const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;
            const float width = cfg.FadeWidth ? ((1.0f - t) * pt.Width + t * cfg.EndWidth) : pt.Width;
            renderer.DrawCircle(width * 0.5f, Transform2D{pt.Position}, sampleColor(t, pt.Age), cfg.Blend);
        }
        return;
    }

    if (cfg.Shape == TrailShape::Stardust)
    {
        // Soft faint ribbon core + scattered twinkling diamond star sparkles
        scratch.clear();
        scratch.reserve(numPoints * 2);
        for (size_t i = 0; i < numPoints; ++i)
        {
            const auto& pt = trail.Points[i];
            const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;
            Vector2f dir = (i == 0) ? (trail.Points[0].Position - trail.Points[1].Position)
                         : (i == numPoints - 1) ? (trail.Points[i - 1].Position - trail.Points[i].Position)
                                                : (trail.Points[i - 1].Position - trail.Points[i + 1].Position);
            const float len = dir.Length();
            const Vector2f normal = (len > 0.001f) ? Vector2f{-dir.Y / len, dir.X / len} : Vector2f{0.0f, 1.0f};
            const float halfW = ((1.0f - t) * pt.Width * 0.35f + t * cfg.EndWidth) * 0.5f;
            Color ribbonCol = sampleColor(t, pt.Age);
            ribbonCol.a = static_cast<uint8_t>(ribbonCol.a * 0.38f);
            scratch.push_back(Vertex{pt.Position.X + normal.X * halfW, pt.Position.Y + normal.Y * halfW, ribbonCol, 0.0f, t});
            scratch.push_back(Vertex{pt.Position.X - normal.X * halfW, pt.Position.Y - normal.Y * halfW, ribbonCol, 1.0f, t});
        }
        if (scratch.size() >= 4)
        {
            renderer.DrawVertices(scratch, PrimitiveType::TriangleStrip, 0, cfg.Blend);
        }

        // Twinkling diamond sparkles scattered along trajectory
        for (size_t i = 0; i < numPoints; ++i)
        {
            const auto& pt = trail.Points[i];
            const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;
            const size_t seed = static_cast<size_t>(std::abs(pt.Position.X * 13.0f + pt.Position.Y * 29.0f));
            const float h1 = DeterministicHash(seed, 1) * 2.0f - 1.0f;
            const float h2 = DeterministicHash(seed, 2) * 2.0f - 1.0f;
            const float spread = (5.0f + t * 22.0f);
            const Vector2f sparkPos = pt.Position + Vector2f{h1 * spread, h2 * spread};
            const float twinkle = 0.55f + 0.45f * std::sin(animTime * 16.0f + static_cast<float>(seed));
            const float starSize = std::max(2.0f, (1.0f - t) * 8.5f * twinkle);

            Color c = (i % 3 == 0) ? Colors::White : sampleColor(t, pt.Age);
            c.a = static_cast<uint8_t>(std::clamp((1.0f - t) * 245.0f * twinkle, 0.0f, 255.0f));
            renderer.DrawRectangle(starSize, starSize, Transform2D{sparkPos, 45.0f + animTime * 90.0f}, c, BlendMode::Add);
            if (i % 2 == 0)
            {
                renderer.DrawRectangle(starSize * 1.9f, std::max(1.2f, starSize * 0.25f), Transform2D{sparkPos}, c, BlendMode::Add);
                renderer.DrawRectangle(std::max(1.2f, starSize * 0.25f), starSize * 1.9f, Transform2D{sparkPos}, c, BlendMode::Add);
            }
        }
        return;
    }

    if (cfg.Shape == TrailShape::MatrixBinary)
    {
        for (size_t i = 0; i < numPoints; ++i)
        {
            const auto& pt = trail.Points[i];
            const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;
            const size_t seed = static_cast<size_t>(std::abs(pt.Position.X * 7.0f + pt.Position.Y * 19.0f));
            const float driftX = (DeterministicHash(seed, 3) * 2.0f - 1.0f) * (6.0f + t * 14.0f);
            const float fallY = t * 18.0f;
            const Vector2f glyphPos = pt.Position + Vector2f{driftX, fallY};

            Color c = sampleColor(t, pt.Age);
            const float glyphSize = std::max(5.5f, (1.0f - t * 0.45f) * 12.0f);
            const bool isOne = ((seed + static_cast<size_t>(animTime * 4.0f)) % 2u) == 1u;
            DrawBinaryDigit(renderer, glyphPos, glyphSize, isOne, c, BlendMode::Add);

            // Tiny trailing digital scan block
            if (i % 2 == 0)
            {
                Color blockCol = c;
                blockCol.a = static_cast<uint8_t>(c.a * 0.45f);
                renderer.DrawRectangle(glyphSize * 0.45f, glyphSize * 0.45f, Transform2D{pt.Position}, blockCol, BlendMode::Add);
            }
        }
        return;
    }

    if (cfg.Shape == TrailShape::ArcPulse)
    {
        Vector2f prevPos = trail.Points[0].Position;
        for (size_t i = 1; i < numPoints; ++i)
        {
            const auto& pt = trail.Points[i];
            const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;
            const Vector2f seg = pt.Position - prevPos;
            const float len = seg.Length();
            const Vector2f normal = (len > 0.001f) ? Vector2f{-seg.Y / len, seg.X / len} : Vector2f{0.0f, 1.0f};
            const float jitter = std::sin(animTime * 42.0f + static_cast<float>(i) * 2.7f) * (1.0f - t) * 11.0f;
            const Vector2f curPos = pt.Position + normal * jitter;

            Color c = sampleColor(t, pt.Age);
            const float thick = std::max(1.2f, (1.0f - t) * 4.2f);
            renderer.DrawLine(prevPos, curPos, c, thick);
            Color core = Colors::White;
            core.a = static_cast<uint8_t>(c.a * 0.8f);
            renderer.DrawLine(prevPos, curPos, core, std::max(0.8f, thick * 0.42f));
            prevPos = curPos;
        }
        return;
    }

    // Ribbon & Flame (continuous tapered TriangleStrip + extra Flame embers)
    scratch.clear();
    scratch.reserve(numPoints * 2);

    for (size_t i = 0; i < numPoints; ++i)
    {
        const auto& pt = trail.Points[i];
        const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;

        Vector2f dir{0.0f, 0.0f};
        if (i == 0)
            dir = trail.Points[0].Position - trail.Points[1].Position;
        else if (i == numPoints - 1)
            dir = trail.Points[i - 1].Position - trail.Points[i].Position;
        else
            dir = trail.Points[i - 1].Position - trail.Points[i + 1].Position;

        const float len = dir.Length();
        const Vector2f normal = (len > 0.001f) ? Vector2f{-dir.Y / len, dir.X / len} : Vector2f{0.0f, 1.0f};

        float width = cfg.FadeWidth ? ((1.0f - t) * pt.Width + t * cfg.EndWidth) : pt.Width;
        if (cfg.Shape == TrailShape::Flame)
        {
            width *= 0.85f + 0.25f * std::sin(animTime * 24.0f + static_cast<float>(i) * 1.3f);
        }
        const float halfWidth = width * 0.5f;

        const Color c = sampleColor(t, pt.Age);
        const Vector2f left = pt.Position + normal * halfWidth;
        const Vector2f right = pt.Position - normal * halfWidth;

        scratch.push_back(Vertex{left.X, left.Y, c, 0.0f, t});
        scratch.push_back(Vertex{right.X, right.Y, c, 1.0f, t});
    }

    if (scratch.size() >= 4)
    {
        renderer.DrawVertices(scratch, PrimitiveType::TriangleStrip, 0, cfg.Blend);
    }

    if (cfg.Shape == TrailShape::Flame)
    {
        // Additional flickering ember sparks dancing off the flame ribbon
        for (size_t i = 1; i < numPoints; i += 2)
        {
            const auto& pt = trail.Points[i];
            const float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;
            const size_t seed = static_cast<size_t>(std::abs(pt.Position.X * 11.0f + pt.Position.Y * 17.0f));
            const float ox = (DeterministicHash(seed, 5) * 2.0f - 1.0f) * (6.0f + t * 16.0f);
            const float oy = (DeterministicHash(seed, 6) * 2.0f - 1.0f) * (6.0f + t * 14.0f) - t * 12.0f;
            const float emberR = std::max(1.5f, (1.0f - t) * 4.5f);
            Color emberCol = (i % 4 == 0) ? Color{255, 240, 120, 255} : Color{255, 110, 20, 255};
            emberCol.a = static_cast<uint8_t>((1.0f - t) * 230.0f);
            renderer.DrawCircle(emberR, Transform2D{pt.Position + Vector2f{ox, oy}}, emberCol, BlendMode::Add);
        }
    }
}

// =============================================================================
// SHOWROOM CARD PREVIEW ICONS & LOCK ICON
// =============================================================================

void CosmeticRenderer::DrawTrailPreviewIcon(
    Renderer&       renderer,
    const Vector2f& center,
    const Vector2f& boxSize,
    TrailStyle      style,
    Color           primary,
    Color           secondary,
    float           animTime)
{
    TrailComponent preview;
    preview.Config = BuildTrailConfig(style, primary, secondary);
    preview.Config.StartWidth = std::min(15.0f, boxSize.Y * 0.30f);
    preview.Config.EndWidth = 2.0f;

    constexpr int kSamples = 14;
    const float spanX = boxSize.X * 0.72f;
    const float ampY = boxSize.Y * 0.24f;
    for (int i = 0; i < kSamples; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(kSamples - 1);
        const float px = center.X + spanX * (0.5f - u);
        const float py = center.Y + std::sin(u * 4.5f - animTime * 4.0f) * ampY;
        preview.Points.push_back(TrailPoint{
            Vector2f{px, py},
            u * preview.Config.Lifetime,
            preview.Config.StartWidth,
            preview.Config.StartColor
        });
    }

    std::vector<Vertex> scratch;
    RenderTrail(renderer, preview, animTime, scratch);

    // Mini glowing ball head at right end
    const Vector2f headPos = preview.Points.front().Position;
    renderer.DrawCircle(std::min(7.5f, boxSize.Y * 0.16f), Transform2D{headPos}, primary);
    renderer.DrawCircle(std::min(3.5f, boxSize.Y * 0.08f), Transform2D{headPos}, Colors::White);
}

void CosmeticRenderer::DrawDeathEffectPreviewIcon(
    Renderer&        renderer,
    const Vector2f&  center,
    float            radius,
    DeathEffectStyle style,
    Color            primary,
    Color            secondary,
    float            animTime)
{
    const float phase = std::fmod(animTime * 0.85f, 1.0f);
    const float inv = 1.0f - phase;

    switch (style)
    {
        case DeathEffectStyle::ClassicMinimal:
            for (int i = 0; i < 8; ++i)
            {
                const float a = static_cast<float>(i) * (2.0f * kPi / 8.0f);
                const float dist = radius * (0.18f + 0.68f * phase);
                const Vector2f p = center + Vector2f{std::cos(a) * dist, std::sin(a) * dist};
                const float sz = 3.5f * inv + 1.5f;
                renderer.DrawRectangle(sz, sz, Transform2D{p}, WithAlpha((i % 3 == 0) ? Colors::White : primary, static_cast<uint8_t>(220 * inv)), BlendMode::Add);
            }
            break;

        case DeathEffectStyle::ClassicBurst:
            for (int i = 0; i < 10; ++i)
            {
                const float a = static_cast<float>(i) * (2.0f * kPi / 10.0f);
                const float dist = radius * (0.25f + 0.75f * phase);
                const Vector2f p = center + Vector2f{std::cos(a) * dist, std::sin(a) * dist};
                renderer.DrawRectangle(5.0f * inv + 2.0f, 5.0f * inv + 2.0f, Transform2D{p, 45.0f}, WithAlpha((i % 2 == 0) ? primary : secondary, static_cast<uint8_t>(240 * inv)), BlendMode::Add);
            }
            renderer.DrawCircle(radius * 0.34f * inv, Transform2D{center}, WithAlpha(primary, static_cast<uint8_t>(200 * inv)), BlendMode::Add);
            break;

        case DeathEffectStyle::ShockwaveRing:
            renderer.DrawCircleOutline(radius * (0.25f + 0.78f * phase), Transform2D{center}, WithAlpha(primary, static_cast<uint8_t>(240 * inv)), 2.8f);
            renderer.DrawCircleOutline(radius * (0.15f + 0.52f * phase), Transform2D{center}, WithAlpha(secondary, static_cast<uint8_t>(215 * inv)), 2.0f);
            renderer.DrawCircle(radius * 0.20f, Transform2D{center}, WithAlpha(Colors::White, static_cast<uint8_t>(190 * inv)), BlendMode::Add);
            break;

        case DeathEffectStyle::PixelShatter:
            for (int i = 0; i < 8; ++i)
            {
                const float a = static_cast<float>(i) * (2.0f * kPi / 8.0f) + 0.35f;
                const float dist = radius * (0.22f + 0.72f * phase);
                const Vector2f p = center + Vector2f{std::cos(a) * dist, std::sin(a) * dist};
                renderer.DrawRectangle(6.5f, 6.5f, Transform2D{p}, WithAlpha((i % 2 == 0) ? primary : secondary, static_cast<uint8_t>(235 * inv)));
            }
            renderer.DrawRectangleOutline(radius * (0.4f + 0.8f * phase), radius * (0.4f + 0.8f * phase), Transform2D{center}, WithAlpha(primary, static_cast<uint8_t>(180 * inv)), 2.0f);
            break;

        case DeathEffectStyle::PrismConfetti:
            for (int i = 0; i < 10; ++i)
            {
                const float a = static_cast<float>(i) * (2.0f * kPi / 10.0f);
                const float dist = radius * (0.20f + 0.78f * phase);
                const Vector2f p = center + Vector2f{std::cos(a) * dist, std::sin(a) * dist};
                Color col = TrailPresets::HsvToRgb(static_cast<float>(i) * 36.0f, 0.9f, 1.0f);
                col.a = static_cast<uint8_t>(240 * inv);
                renderer.DrawRectangle(6.0f, 6.0f, Transform2D{p, animTime * 180.0f + i * 35.0f}, col, BlendMode::Add);
            }
            break;

        case DeathEffectStyle::NovaImplosion:
        {
            const float len = radius * (0.6f + 1.2f * phase);
            const float thick = std::max(2.0f, 6.5f * inv);
            renderer.DrawRectangle(len, thick, Transform2D{center}, WithAlpha(primary, static_cast<uint8_t>(240 * inv)), BlendMode::Add);
            renderer.DrawRectangle(thick, len, Transform2D{center}, WithAlpha(primary, static_cast<uint8_t>(240 * inv)), BlendMode::Add);
            renderer.DrawRectangle(len * 0.65f, thick * 0.6f, Transform2D{center, 45.0f}, WithAlpha(secondary, static_cast<uint8_t>(220 * inv)), BlendMode::Add);
            renderer.DrawRectangle(len * 0.65f, thick * 0.6f, Transform2D{center, -45.0f}, WithAlpha(secondary, static_cast<uint8_t>(220 * inv)), BlendMode::Add);
            renderer.DrawCircle(radius * 0.28f * inv + 3.0f, Transform2D{center}, WithAlpha(Colors::White, static_cast<uint8_t>(230 * inv)), BlendMode::Add);
            break;
        }

        case DeathEffectStyle::MatrixGlitch:
            renderer.DrawRectangle(radius * 1.4f, 4.0f, Transform2D{center + Vector2f{4.0f * phase, -radius * 0.32f}}, WithAlpha(primary, static_cast<uint8_t>(220 * inv)), BlendMode::Add);
            renderer.DrawRectangle(radius * 1.2f, 4.0f, Transform2D{center + Vector2f{-4.0f * phase, radius * 0.30f}}, WithAlpha(secondary, static_cast<uint8_t>(220 * inv)), BlendMode::Add);
            DrawBinaryDigit(renderer, center + Vector2f{-radius * 0.34f, 0.0f}, 11.0f, false, WithAlpha(primary, static_cast<uint8_t>(240 * inv)), BlendMode::Add);
            DrawBinaryDigit(renderer, center + Vector2f{ radius * 0.34f, 0.0f}, 11.0f, true,  WithAlpha(secondary, static_cast<uint8_t>(240 * inv)), BlendMode::Add);
            break;
    }
}

void CosmeticRenderer::DrawLockIcon(Renderer& renderer, const Vector2f& pos, float size, Color color)
{
    const float bodyW = size * 0.72f;
    const float bodyH = size * 0.54f;
    const float shackleR = size * 0.24f;
    // Padlock shackle arch
    renderer.DrawCircleOutline(shackleR, Transform2D{pos + Vector2f{0.0f, -bodyH * 0.42f}}, color, std::max(1.8f, size * 0.11f));
    // Padlock rectangular body
    renderer.DrawRectangle(bodyW, bodyH, Transform2D{pos + Vector2f{0.0f, bodyH * 0.18f}}, color);
    // Keyhole cutout
    renderer.DrawCircle(std::max(1.6f, size * 0.09f), Transform2D{pos + Vector2f{0.0f, bodyH * 0.14f}}, Color{18, 22, 32, 255});
    renderer.DrawRectangle(std::max(1.6f, size * 0.08f), bodyH * 0.30f, Transform2D{pos + Vector2f{0.0f, bodyH * 0.28f}}, Color{18, 22, 32, 255});
}
