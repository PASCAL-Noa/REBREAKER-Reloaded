#pragma once

#include "Scenes/DefaultScene.h"
#include "Core/ProfileManager.h"
#include "ECS/Components/TrailComponent.h"
#include "Graphics/CosmeticRenderer.h"
#include "Data/Vertex.h"
#include "Math/Vector2.h"
#include <array>
#include <string>
#include <vector>

class ProfileScene : public DefaultScene
{
public:
    ProfileScene() = default;
    ~ProfileScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    enum class ViewMode : uint8_t
    {
        Garage = 0,
        Career = 1
    };

    struct Rect
    {
        Vector2f Center{0.0f, 0.0f};
        Vector2f Size{0.0f, 0.0f};

        [[nodiscard]] bool Contains(const Vector2f& p) const noexcept
        {
            return (p.X >= Center.X - Size.X * 0.5f && p.X <= Center.X + Size.X * 0.5f &&
                    p.Y >= Center.Y - Size.Y * 0.5f && p.Y <= Center.Y + Size.Y * 0.5f);
        }
    };

    static constexpr int kTabCount = 6;

    // Helpers
    void SwitchTab(int tabIdx, GameContext* context = nullptr);
    void RefreshCategoryItems();
    void SelectEquippedInCurrentTab();
    void UpdateShowroomSimulation(float dt, const Vector2f& viewSize, const Vector2f& mouse, bool mouseInArena);
    void TriggerShowroomBurst(const Vector2f& pos, float scale = 1.15f);
    void TriggerPrimaryActionForSelected(GameContext& context);
    void RandomizeEquippedLoadout(GameContext& context);
    void PerformFullSaveReset(GameContext& context);
    void ShowToast(const std::string& text, Color color);
    void ReturnToMenu(GameContext& context);

    void HandleInput(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);
    void HandleNicknameInput(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);

    // Layout helpers
    [[nodiscard]] Rect GetShowroomRect(const Vector2f& viewSize) const;
    [[nodiscard]] Rect GetTabRect(int tabIdx, const Vector2f& viewSize) const;
    [[nodiscard]] Rect GetGridItemRect(int itemIdx, int itemCount, bool isColorTab, const Vector2f& viewSize) const;
    [[nodiscard]] Rect GetInspectorRect(const Vector2f& viewSize) const;

    // Rendering
    void RenderBackground(GameContext& context, const Vector2f& viewSize, Color primary, Color secondary);
    void RenderHeaderBar(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary);
    void RenderGarageMode(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);
    void RenderShowroom(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);
    void RenderCategoryTabs(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary);
    void RenderItemGrid(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color previewPri, Color previewSec);
    void RenderInspectorBar(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary);
    void RenderCareerMode(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary, Color secondary);
    void RenderToast(GameContext& context, const Vector2f& viewSize);

    // Effective preview values (equipped overridden by hovered item for live showroom preview)
    [[nodiscard]] Color            GetPreviewPrimaryColor() const;
    [[nodiscard]] Color            GetPreviewSecondaryColor() const;
    [[nodiscard]] PaddleSkinStyle  GetPreviewPaddleStyle() const;
    [[nodiscard]] BallSkinStyle    GetPreviewBallStyle() const;
    [[nodiscard]] TrailStyle       GetPreviewTrailStyle() const;
    [[nodiscard]] DeathEffectStyle GetPreviewDeathStyle() const;
    [[nodiscard]] const CosmeticItem* GetInspectedItem() const;

    static CosmeticSlot TabIndexToSlot(int tabIdx);
    static const char*  TabIndexToLabel(int tabIdx);
    static const char*  SlotToShortTag(CosmeticSlot slot);

    // Assets
    uint32_t m_fontId = 0;
    uint32_t m_paddleTexId = 0;
    uint32_t m_ballTexId = 0;
    uint32_t m_clickSfxId = 0;
    uint32_t m_switchSfxId = 0;
    uint32_t m_bounceSfxId = 0;
    uint32_t m_explosionSfxId = 0;

    // Scene state
    ViewMode m_viewMode = ViewMode::Garage;
    int      m_activeTab = 0; // 0=Primary, 1=Secondary, 2=Paddle, 3=Ball, 4=Trail, 5=Death
    std::vector<CosmeticItem> m_currentItems;
    int      m_selectedIndex = 0;
    int      m_hoveredIndex = -1;
    float    m_animTime = 0.0f;

    // Nickname inline edit
    bool        m_editingNickname = false;
    std::string m_nicknameBuffer;

    // Career scroll & 2-step reset confirmation
    float m_careerScrollY = 0.0f;
    bool  m_resetArmed = false;
    float m_resetArmTimer = 0.0f;

    // Toast notification
    std::string m_toastText;
    Color       m_toastColor{0, 235, 255, 255};
    float       m_toastTimer = 0.0f;

    // Live 60fps Showroom simulation state
    Vector2f m_paddlePos{0.0f, -95.0f};
    float    m_paddleVelX = 0.0f;
    float    m_paddleTiltDeg = 0.0f;
    Vector2f m_ballPos{-180.0f, -290.0f};
    Vector2f m_ballVel{370.0f, 245.0f};
    TrailComponent m_ballTrail{};
    TrailStyle     m_cachedTrailStyle = TrailStyle::Ribbon;
    Color          m_cachedTrailPrimary{0, 230, 255, 255};
    Color          m_cachedTrailSecondary{255, 255, 255, 255};
    std::vector<Vertex> m_trailScratch;
    CosmeticExplosionPool m_explosions;
    float    m_autoBurstTimer = 0.8f;
    float    m_paddleHitPulse = 0.0f;
};
