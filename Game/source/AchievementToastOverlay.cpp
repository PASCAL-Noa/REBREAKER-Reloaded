#include "UI/AchievementToastOverlay.h"
#include "AudioMixer.h"
#include "Core/GameContext.h"
#include "ECS/Components/Transform2D.h"
#include "Graphics/Renderer.h"
#include "Math/EasingFunctions.h"
#include "Resources/ResourceManager.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float kSlideInDuration = 0.42f;
    constexpr float kHoldDuration = 3.45f;
    constexpr float kSlideOutDuration = 0.48f;
    constexpr float kTotalDuration = kSlideInDuration + kHoldDuration + kSlideOutDuration;

    constexpr float kCardWidth = 446.0f;
    constexpr float kCardHeight = 108.0f;
    constexpr float kMarginRight = 24.0f;
    constexpr float kMarginTop = 24.0f;

    Color ScaleAlpha(Color c, float factor) noexcept
    {
        const float clamped = std::clamp(factor, 0.0f, 1.0f);
        return Color{c.r, c.g, c.b, static_cast<uint8_t>(static_cast<float>(c.a) * clamped)};
    }

    float NextUnitFloat(uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    }

    std::string TruncateText(const std::string& text, size_t maxChars)
    {
        if (text.size() <= maxChars) return text;
        if (maxChars <= 3) return text.substr(0, maxChars);
        return text.substr(0, maxChars - 3) + "...";
    }
}

void AchievementToastOverlay::Init(GameContext& context)
{
    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_unlockSfxId = context.Resources.LoadResource("Resources/audio/sfx/achievement_unlock.wav");
    if (m_unlockSfxId == 0)
    {
        m_unlockSfxId = context.Resources.LoadResource("Resources/audio/sfx/combo.wav");
    }
}

void AchievementToastOverlay::Update(float dt, GameContext& context)
{
    const float clampedDt = std::clamp(dt, 0.0f, 0.05f);

    if (!m_active)
    {
        AchievementToast next{};
        if (AchievementManager::Get().PopNextToast(next))
        {
            StartToast(next, context);
        }
        return;
    }

    m_elapsed += clampedDt;

    const Color accent = GetAccentColor(m_currentToast.Category, m_currentToast.IsSecret);
    if (m_elapsed < kSlideInDuration + kHoldDuration)
    {
        m_sparkleAccum += clampedDt;
        while (m_sparkleAccum >= 0.085f)
        {
            m_sparkleAccum -= 0.085f;
            SpawnAmbientSparkle(accent);
        }
    }

    for (auto it = m_particles.begin(); it != m_particles.end(); )
    {
        it->Age += clampedDt;
        if (it->Age >= it->Lifetime)
        {
            it = m_particles.erase(it);
            continue;
        }
        it->Offset.X += it->Velocity.X * clampedDt;
        it->Offset.Y += it->Velocity.Y * clampedDt;
        it->Velocity.X *= std::max(0.0f, 1.0f - clampedDt * 1.8f);
        it->Velocity.Y *= std::max(0.0f, 1.0f - clampedDt * 1.4f);
        ++it;
    }

    if (m_elapsed >= kTotalDuration)
    {
        m_active = false;
        m_particles.clear();
        AchievementToast next{};
        if (AchievementManager::Get().PopNextToast(next))
        {
            StartToast(next, context);
        }
    }
}

void AchievementToastOverlay::StartToast(const AchievementToast& toast, GameContext& context)
{
    m_currentToast = toast;
    m_active = true;
    m_elapsed = 0.0f;
    m_sparkleAccum = 0.0f;
    m_particles.clear();

    const Color accent = GetAccentColor(toast.Category, toast.IsSecret);
    SpawnBurstParticles(accent);

    if (m_unlockSfxId != 0)
    {
        context.Audio.PlaySfx(m_unlockSfxId, 72.0f, 1.0f);
    }
}

void AchievementToastOverlay::SpawnBurstParticles(Color accent)
{
    for (int i = 0; i < 18; ++i)
    {
        const float angle = (static_cast<float>(i) / 18.0f) * 6.2831853f + NextUnitFloat(m_rngState) * 0.25f;
        const float speed = 42.0f + NextUnitFloat(m_rngState) * 78.0f;
        ShimmerParticle p{};
        p.Offset = Vector2f{-kCardWidth * 0.5f + 50.0f, 0.0f};
        p.Velocity = Vector2f{std::cos(angle) * speed, std::sin(angle) * speed};
        p.Age = 0.0f;
        p.Lifetime = 0.55f + NextUnitFloat(m_rngState) * 0.55f;
        p.Size = 3.2f + NextUnitFloat(m_rngState) * 3.8f;
        p.Phase = NextUnitFloat(m_rngState) * 6.2831853f;
        p.ParticleColor = (i % 3 == 0) ? Colors::White : ((i % 3 == 1) ? Color{255, 224, 92, 255} : accent);
        m_particles.push_back(p);
    }
}

void AchievementToastOverlay::SpawnAmbientSparkle(Color accent)
{
    if (m_particles.size() >= 36) return;

    ShimmerParticle p{};
    p.Offset = Vector2f{
        (NextUnitFloat(m_rngState) - 0.5f) * (kCardWidth - 16.0f),
        (NextUnitFloat(m_rngState) - 0.5f) * (kCardHeight - 14.0f)
    };
    p.Velocity = Vector2f{
        (NextUnitFloat(m_rngState) - 0.5f) * 18.0f,
        -16.0f - NextUnitFloat(m_rngState) * 24.0f
    };
    p.Age = 0.0f;
    p.Lifetime = 0.65f + NextUnitFloat(m_rngState) * 0.55f;
    p.Size = 2.4f + NextUnitFloat(m_rngState) * 3.2f;
    p.Phase = NextUnitFloat(m_rngState) * 6.2831853f;
    p.ParticleColor = (NextUnitFloat(m_rngState) < 0.55f) ? Color{255, 232, 115, 255} : accent;
    m_particles.push_back(p);
}

Color AchievementToastOverlay::GetAccentColor(AchievementCategory category, bool isSecret) noexcept
{
    if (isSecret) return Color{235, 115, 255, 255};
    switch (category)
    {
        case AchievementCategory::Combat:      return Color{255, 115, 75, 255};
        case AchievementCategory::Endless:     return Color{165, 115, 255, 255};
        case AchievementCategory::Campaign:    return Color{255, 205, 65, 255};
        case AchievementCategory::PowerUps:    return Color{55, 235, 255, 255};
        case AchievementCategory::Progression: return Color{95, 245, 165, 255};
        case AchievementCategory::Secret:      return Color{235, 115, 255, 255};
        default:                               return Color{255, 215, 70, 255};
    }
}

void AchievementToastOverlay::DrawTrophyIcon(
    Renderer& renderer,
    const Vector2f& center,
    float scale,
    Color gold,
    Color highlight,
    float alpha)
{
    const Color g = ScaleAlpha(gold, alpha);
    const Color h = ScaleAlpha(highlight, alpha);
    const Color rim = ScaleAlpha(Color{155, 98, 22, 255}, alpha);

    renderer.DrawRectangle(26.0f * scale, 5.0f * scale, Transform2D{Vector2f{center.X, center.Y + 17.5f * scale}}, rim);
    renderer.DrawRectangle(22.0f * scale, 4.0f * scale, Transform2D{Vector2f{center.X, center.Y + 14.0f * scale}}, g);
    renderer.DrawRectangle(7.0f * scale, 10.0f * scale, Transform2D{Vector2f{center.X, center.Y + 8.0f * scale}}, g);

    renderer.DrawRectangle(26.0f * scale, 17.0f * scale, Transform2D{Vector2f{center.X, center.Y - 4.0f * scale}}, g);
    renderer.DrawRectangle(18.0f * scale, 6.0f * scale, Transform2D{Vector2f{center.X, center.Y + 4.5f * scale}}, g);
    renderer.DrawRectangle(28.0f * scale, 3.5f * scale, Transform2D{Vector2f{center.X, center.Y - 12.0f * scale}}, h);

    renderer.DrawRectangleOutline(8.0f * scale, 10.0f * scale, Transform2D{Vector2f{center.X - 15.5f * scale, center.Y - 5.0f * scale}}, g, 2.2f);
    renderer.DrawRectangleOutline(8.0f * scale, 10.0f * scale, Transform2D{Vector2f{center.X + 15.5f * scale, center.Y - 5.0f * scale}}, g, 2.2f);

    DrawSparkleDiamond(renderer, Vector2f{center.X, center.Y - 4.0f * scale}, 4.8f * scale, h);
}

void AchievementToastOverlay::DrawSparkleDiamond(Renderer& renderer, const Vector2f& pos, float radius, Color color)
{
    if (radius <= 0.5f || color.a == 0) return;
    const float diag = radius * 1.4142f;
    renderer.DrawRectangle(diag, diag, Transform2D{pos, 45.0f}, color);
    renderer.DrawLine(Vector2f{pos.X - radius * 1.85f, pos.Y}, Vector2f{pos.X + radius * 1.85f, pos.Y}, color, 1.3f);
    renderer.DrawLine(Vector2f{pos.X, pos.Y - radius * 1.85f}, Vector2f{pos.X, pos.Y + radius * 1.85f}, color, 1.3f);
}

void AchievementToastOverlay::Render(Renderer& renderer) const
{
    if (!m_active) return;

    renderer.ResetCamera();
    const Vector2f viewSize = renderer.GetLogicalViewSize();

    float slideProgress = 1.0f;
    float alpha = 1.0f;
    if (m_elapsed < kSlideInDuration)
    {
        const float t = std::clamp(m_elapsed / kSlideInDuration, 0.0f, 1.0f);
        slideProgress = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseOutBack)(t);
        alpha = std::clamp(t * 2.2f, 0.0f, 1.0f);
    }
    else if (m_elapsed > kSlideInDuration + kHoldDuration)
    {
        const float t = std::clamp((m_elapsed - (kSlideInDuration + kHoldDuration)) / kSlideOutDuration, 0.0f, 1.0f);
        const float eased = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseInQuad)(t);
        slideProgress = 1.0f - eased;
        alpha = 1.0f - eased;
    }

    const float targetCenterX = viewSize.X - kMarginRight - kCardWidth * 0.5f;
    const float hiddenCenterX = viewSize.X + kCardWidth * 0.6f;
    const float centerX = hiddenCenterX + (targetCenterX - hiddenCenterX) * slideProgress;
    const float centerY = kMarginTop + kCardHeight * 0.5f;
    const Vector2f cardCenter{centerX, centerY};

    const Color accent = GetAccentColor(m_currentToast.Category, m_currentToast.IsSecret);
    const Color gold{255, 214, 70, 255};
    const Color goldLight{255, 248, 185, 255};

    // Soft outer glow & layered depth shadow
    renderer.DrawRectangle(kCardWidth + 12.0f, kCardHeight + 12.0f,
                           Transform2D{Vector2f{centerX, centerY + 3.0f}},
                           ScaleAlpha(accent, 0.16f * alpha));
    renderer.DrawRectangle(kCardWidth + 4.0f, kCardHeight + 4.0f,
                           Transform2D{Vector2f{centerX + 4.0f, centerY + 6.0f}},
                           ScaleAlpha(Color{4, 7, 14, 210}, alpha));

    // Main dark bevelled card
    renderer.DrawRectangle(kCardWidth, kCardHeight, Transform2D{cardCenter}, ScaleAlpha(Color{13, 20, 34, 248}, alpha));
    renderer.DrawRectangle(kCardWidth - 6.0f, kCardHeight * 0.44f,
                           Transform2D{Vector2f{centerX, centerY - kCardHeight * 0.24f}},
                           ScaleAlpha(Color{32, 48, 76, 115}, alpha));

    // Left accent strip
    const float leftEdge = centerX - kCardWidth * 0.5f;
    const float topEdge = centerY - kCardHeight * 0.5f;
    renderer.DrawRectangle(6.0f, kCardHeight, Transform2D{Vector2f{leftEdge + 3.0f, centerY}}, ScaleAlpha(gold, alpha));

    // Diagonal shiny shimmer sweep across card during hold
    const float shimmerWindow = std::fmod(m_elapsed, 2.2f) / 0.85f;
    if (shimmerWindow >= 0.0f && shimmerWindow <= 1.0f)
    {
        const float sweepX = leftEdge + 20.0f + (kCardWidth - 40.0f) * shimmerWindow;
        const float sweepFade = std::sin(shimmerWindow * 3.14159265f);
        renderer.DrawRectangle(28.0f, kCardHeight - 8.0f,
                               Transform2D{Vector2f{sweepX, centerY}, 16.0f},
                               ScaleAlpha(Color{255, 248, 205, 58}, sweepFade * alpha));
    }

    // Crisp outer border & inner highlight
    renderer.DrawRectangleOutline(kCardWidth, kCardHeight, Transform2D{cardCenter}, ScaleAlpha(gold, 0.92f * alpha), 2.2f);
    renderer.DrawRectangleOutline(kCardWidth - 6.0f, kCardHeight - 6.0f, Transform2D{cardCenter}, ScaleAlpha(accent, 0.45f * alpha), 1.0f);

    // Trophy medallion slot on left
    const Vector2f medallionCenter{leftEdge + 54.0f, centerY};
    renderer.DrawRectangle(72.0f, 76.0f, Transform2D{medallionCenter}, ScaleAlpha(Color{20, 28, 46, 245}, alpha));
    renderer.DrawRectangleOutline(72.0f, 76.0f, Transform2D{medallionCenter}, ScaleAlpha(gold, 0.8f * alpha), 1.6f);

    const float pulse = 1.0f + 0.06f * std::sin(m_elapsed * 7.5f);
    DrawTrophyIcon(renderer, medallionCenter, 1.15f * pulse, gold, goldLight, alpha);

    // Text content
    if (m_fontId != 0)
    {
        const float textX = leftEdge + 104.0f;
        const std::string header = m_currentToast.IsSecret ? "SECRET ACHIEVEMENT UNLOCKED" : "ACHIEVEMENT UNLOCKED";
        renderer.DrawText(header, m_fontId, 16.0f, Transform2D{Vector2f{textX, topEdge + 11.0f}}, ScaleAlpha(gold, alpha));

        renderer.DrawText(TruncateText(m_currentToast.Title, 28), m_fontId, 24.0f,
                          Transform2D{Vector2f{textX, topEdge + 29.0f}}, ScaleAlpha(Colors::White, alpha));

        renderer.DrawText(TruncateText(m_currentToast.Description, 42), m_fontId, 15.0f,
                          Transform2D{Vector2f{textX, topEdge + 59.0f}}, ScaleAlpha(Color{188, 208, 236, 255}, alpha));

        // Reward chips along bottom row
        float chipX = textX;
        const float chipY = topEdge + 86.0f;
        if (m_currentToast.RewardCredits > 0)
        {
            const std::string crText = "+" + std::to_string(m_currentToast.RewardCredits) + " CR";
            const Vector2f sz = renderer.GetTextSize(crText, m_fontId, 15.0f);
            const float chipW = sz.X + 16.0f;
            renderer.DrawRectangle(chipW, 20.0f, Transform2D{Vector2f{chipX + chipW * 0.5f, chipY}}, ScaleAlpha(Color{58, 46, 14, 240}, alpha));
            renderer.DrawRectangleOutline(chipW, 20.0f, Transform2D{Vector2f{chipX + chipW * 0.5f, chipY}}, ScaleAlpha(gold, 0.85f * alpha), 1.2f);
            renderer.DrawTextCentered(crText, m_fontId, 15.0f, Transform2D{Vector2f{chipX + chipW * 0.5f, chipY - 1.0f}}, ScaleAlpha(goldLight, alpha));
            chipX += chipW + 8.0f;
        }
        if (!m_currentToast.RewardCosmeticName.empty())
        {
            const std::string cosText = "UNLOCKED: " + TruncateText(m_currentToast.RewardCosmeticName, 18);
            const Vector2f sz = renderer.GetTextSize(cosText, m_fontId, 15.0f);
            const float chipW = sz.X + 16.0f;
            renderer.DrawRectangle(chipW, 20.0f, Transform2D{Vector2f{chipX + chipW * 0.5f, chipY}}, ScaleAlpha(Color{16, 44, 58, 240}, alpha));
            renderer.DrawRectangleOutline(chipW, 20.0f, Transform2D{Vector2f{chipX + chipW * 0.5f, chipY}}, ScaleAlpha(accent, 0.85f * alpha), 1.2f);
            renderer.DrawTextCentered(cosText, m_fontId, 15.0f, Transform2D{Vector2f{chipX + chipW * 0.5f, chipY - 1.0f}}, ScaleAlpha(Colors::White, alpha));
        }
    }

    // Shimmering sparkle particles
    for (const auto& p : m_particles)
    {
        const float lifeRatio = 1.0f - (p.Age / std::max(0.001f, p.Lifetime));
        const float twinkle = 0.55f + 0.45f * std::sin(m_elapsed * 18.0f + p.Phase);
        const float pAlpha = std::clamp(lifeRatio * twinkle * alpha, 0.0f, 1.0f);
        const Vector2f worldPos{centerX + p.Offset.X, centerY + p.Offset.Y};
        DrawSparkleDiamond(renderer, worldPos, p.Size * (0.4f + 0.6f * lifeRatio), ScaleAlpha(p.ParticleColor, pAlpha));
    }
}
