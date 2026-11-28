#pragma once

#include "Core/AchievementManager.h"
#include "Data/Color.h"
#include "Math/Vector2.h"
#include <vector>
#include <cstdint>

struct GameContext;
class Renderer;

class AchievementToastOverlay
{
public:
    AchievementToastOverlay() = default;

    void Init(GameContext& context);
    void Update(float dt, GameContext& context);
    void Render(Renderer& renderer) const;

    [[nodiscard]] bool IsActive() const noexcept { return m_active; }

private:
    struct ShimmerParticle
    {
        Vector2f Offset{0.0f, 0.0f};
        Vector2f Velocity{0.0f, 0.0f};
        float    Age = 0.0f;
        float    Lifetime = 1.0f;
        float    Size = 4.0f;
        float    Phase = 0.0f;
        Color    ParticleColor{255, 230, 120, 255};
    };

    void StartToast(const AchievementToast& toast, GameContext& context);
    void SpawnBurstParticles(Color accent);
    void SpawnAmbientSparkle(Color accent);

    static Color GetAccentColor(AchievementCategory category, bool isSecret) noexcept;
    static void  DrawTrophyIcon(Renderer& renderer, const Vector2f& center, float scale, Color gold, Color highlight, float alpha);
    static void  DrawSparkleDiamond(Renderer& renderer, const Vector2f& pos, float radius, Color color);

    AchievementToast             m_currentToast{};
    bool                         m_active = false;
    float                        m_elapsed = 0.0f;
    float                        m_sparkleAccum = 0.0f;
    uint32_t                     m_rngState = 0xA7C319E5u;
    std::vector<ShimmerParticle> m_particles;

    uint32_t m_fontId = 0;
    uint32_t m_unlockSfxId = 0;
};
