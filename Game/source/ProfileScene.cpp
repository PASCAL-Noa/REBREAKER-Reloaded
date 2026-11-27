#include "Scenes/ProfileScene.h"
#include "Scenes/MenuScene.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Core/PlayerPrefs.h"
#include "PowerUps/PowerUpConfig.h"
#include "LevelManager.h"
#include "LeaderboardManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Camera2D.h"
#include "AudioMixer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

namespace
{
    constexpr float kHeaderH = 68.0f;
    constexpr float kSideMargin = 32.0f;
    constexpr float kShowroomBallRadius = 17.0f;
    constexpr float kShowroomPaddleW = 178.0f;
    constexpr float kShowroomPaddleH = 28.0f;
    constexpr float kLoadoutBoxW = 290.0f;

    Color ScaleAlpha(Color c, float factor)
    {
        c.a = static_cast<uint8_t>(std::clamp(static_cast<float>(c.a) * factor, 0.0f, 255.0f));
        return c;
    }

    Color MixColor(Color a, Color b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color{
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t),
            static_cast<uint8_t>(a.a + (b.a - a.a) * t)
        };
    }

    std::string FormatIntWithCommas(uint32_t value)
    {
        std::string s = std::to_string(value);
        int insertPos = static_cast<int>(s.length()) - 3;
        while (insertPos > 0)
        {
            s.insert(static_cast<size_t>(insertPos), ",");
            insertPos -= 3;
        }
        return s;
    }
}

CosmeticSlot ProfileScene::TabIndexToSlot(int tabIdx)
{
    switch (tabIdx)
    {
        case 0: return CosmeticSlot::PrimaryColor;
        case 1: return CosmeticSlot::SecondaryColor;
        case 2: return CosmeticSlot::PaddleSkin;
        case 3: return CosmeticSlot::BallSkin;
        case 4: return CosmeticSlot::BallTrail;
        case 5: return CosmeticSlot::DeathEffect;
        default: return CosmeticSlot::PrimaryColor;
    }
}

const char* ProfileScene::TabIndexToLabel(int tabIdx)
{
    switch (tabIdx)
    {
        case 0: return "PRIMARY COLOR";
        case 1: return "SECONDARY COLOR";
        case 2: return "PADDLE SKINS";
        case 3: return "BALL SKINS";
        case 4: return "PARTICLE TRAILS";
        case 5: return "DEATH / EXPLOSION";
        default: return "COSMETICS";
    }
}

const char* ProfileScene::SlotToShortTag(CosmeticSlot slot)
{
    switch (slot)
    {
        case CosmeticSlot::PrimaryColor:   return "PRIMARY";
        case CosmeticSlot::SecondaryColor: return "SECONDARY";
        case CosmeticSlot::PaddleSkin:     return "PADDLE";
        case CosmeticSlot::BallSkin:       return "BALL";
        case CosmeticSlot::BallTrail:      return "TRAIL";
        case CosmeticSlot::DeathEffect:    return "EXPLOSION";
        default:                           return "ITEM";
    }
}

void ProfileScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    m_bgColor = Color{9, 13, 22, 255};

    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_switchSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_switch.wav");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/paddle_hit.wav");
    m_explosionSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_destroy.wav");

    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 0.8f, AudioMixer::kNominalMusicLevel, true);

    auto& pm = ProfileManager::Get();
    pm.LoadProfile();
    const int newlyUnlocked = pm.EvaluateAchievementUnlocks();
    if (newlyUnlocked > 0)
    {
        ShowToast("NEW MILESTONE UNLOCKED (+" + std::to_string(newlyUnlocked) + ")!", Color{255, 220, 75, 255});
    }

    m_viewMode = ViewMode::Garage;
    m_activeTab = 0;
    m_editingNickname = false;
    m_nicknameBuffer = pm.GetUsername();
    m_resetArmed = false;
    m_resetArmTimer = 0.0f;
    m_careerScrollY = 0.0f;

    RefreshCategoryItems();
    SelectEquippedInCurrentTab();

    // Initialize showroom entities
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const Rect arena = GetShowroomRect(viewSize);
    const float arenaCenterX = arena.Center.X - kLoadoutBoxW * 0.5f;
    m_paddlePos = Vector2f{arenaCenterX, arena.Center.Y + arena.Size.Y * 0.5f - 62.0f};
    m_ballPos = Vector2f{arenaCenterX - 110.0f, arena.Center.Y - 40.0f};
    m_ballVel = Vector2f{365.0f, 240.0f};

    m_cachedTrailStyle = pm.GetEquippedTrailStyle();
    m_cachedTrailPrimary = pm.GetEquippedPrimaryColor();
    m_cachedTrailSecondary = pm.GetEquippedSecondaryColor();
    m_ballTrail.Clear();
    m_ballTrail.Config = CosmeticRenderer::BuildTrailConfig(m_cachedTrailStyle, m_cachedTrailPrimary, m_cachedTrailSecondary);
    m_explosions.Clear();
    m_autoBurstTimer = 0.65f;
}

void ProfileScene::OnDestroy(GameContext& context)
{
    ProfileManager::Get().SaveProfile();
    m_explosions.Clear();
    DefaultScene::OnDestroy(context);
}

void ProfileScene::SwitchTab(int tabIdx, GameContext* context)
{
    tabIdx = std::clamp(tabIdx, 0, kTabCount - 1);
    if (m_activeTab == tabIdx && !m_currentItems.empty())
    {
        return;
    }
    m_activeTab = tabIdx;
    m_hoveredIndex = -1;
    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    if (context && m_switchSfxId != 0)
    {
        context->Audio.PlaySfx(m_switchSfxId, 16.0f);
    }

    if (TabIndexToSlot(m_activeTab) == CosmeticSlot::DeathEffect)
    {
        TriggerShowroomBurst(m_ballPos, 1.25f);
        m_autoBurstTimer = 1.65f;
    }
}

void ProfileScene::RefreshCategoryItems()
{
    m_currentItems = ProfileManager::Get().GetCosmeticsBySlot(TabIndexToSlot(m_activeTab));
    if (m_currentItems.empty())
    {
        m_selectedIndex = -1;
    }
    else
    {
        m_selectedIndex = std::clamp(m_selectedIndex, 0, static_cast<int>(m_currentItems.size()) - 1);
    }
}

void ProfileScene::SelectEquippedInCurrentTab()
{
    const std::string equippedId = ProfileManager::Get().GetEquippedCosmetic(TabIndexToSlot(m_activeTab));
    m_selectedIndex = 0;
    for (int i = 0; i < static_cast<int>(m_currentItems.size()); ++i)
    {
        if (m_currentItems[static_cast<size_t>(i)].Id == equippedId)
        {
            m_selectedIndex = i;
            break;
        }
    }
}

void ProfileScene::ShowToast(const std::string& text, Color color)
{
    m_toastText = text;
    m_toastColor = color;
    m_toastTimer = 2.4f;
}

void ProfileScene::ReturnToMenu(GameContext& context)
{
    ProfileManager::Get().SaveProfile();
    if (m_switchSfxId != 0)
    {
        context.Audio.PlaySfx(m_switchSfxId, 16.0f);
    }
    MenuScene::SetReturnChannelType(ChannelType::Achievements);
    MenuScene::SetReturnWithDezoom(true);
    context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.55f);
}

void ProfileScene::TriggerShowroomBurst(const Vector2f& pos, float scale)
{
    m_explosions.Spawn(
        pos,
        GetPreviewDeathStyle(),
        GetPreviewPrimaryColor(),
        GetPreviewSecondaryColor(),
        scale);
}

void ProfileScene::TriggerPrimaryActionForSelected(GameContext& context)
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_currentItems.size()))
    {
        return;
    }

    auto& pm = ProfileManager::Get();
    const CosmeticItem item = m_currentItems[static_cast<size_t>(m_selectedIndex)];
    const CosmeticSlot slot = TabIndexToSlot(m_activeTab);

    if (item.Unlocked)
    {
        if (pm.GetEquippedCosmetic(slot) == item.Id)
        {
            if (slot == CosmeticSlot::DeathEffect)
            {
                TriggerShowroomBurst(m_ballPos, 1.3f);
                if (m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 18.0f, 1.15f);
            }
            return;
        }
        pm.EquipCosmetic(slot, item.Id);
        RefreshCategoryItems();
        ShowToast("EQUIPPED: " + item.Name, GetPreviewPrimaryColor());
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 20.0f);
        if (slot == CosmeticSlot::DeathEffect)
        {
            TriggerShowroomBurst(m_ballPos, 1.3f);
        }
        return;
    }

    if (item.UnlockType == CosmeticUnlockType::Shop)
    {
        if (PowerUpManager::Get().GetCoins() >= item.Price)
        {
            if (pm.UnlockCosmetic(item.Id))
            {
                pm.EquipCosmetic(slot, item.Id);
                RefreshCategoryItems();
                ShowToast("UNLOCKED & EQUIPPED: " + item.Name, Color{255, 220, 75, 255});
                TriggerShowroomBurst(m_ballPos, 1.35f);
                if (m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 22.0f, 1.25f);
            }
        }
        else
        {
            const int missing = item.Price - PowerUpManager::Get().GetCoins();
            ShowToast("NEED " + std::to_string(missing) + " MORE CR TO UNLOCK", Color{255, 90, 90, 255});
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f, 0.72f);
        }
    }
    else
    {
        ShowToast("LOCKED: " + item.UnlockRequirement, Color{255, 185, 70, 255});
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f, 0.75f);
    }
}

void ProfileScene::RandomizeEquippedLoadout(GameContext& context)
{
    static std::mt19937 rng(std::random_device{}());
    auto& pm = ProfileManager::Get();

    const std::array<CosmeticSlot, 6> slots = {
        CosmeticSlot::PrimaryColor,
        CosmeticSlot::SecondaryColor,
        CosmeticSlot::PaddleSkin,
        CosmeticSlot::BallSkin,
        CosmeticSlot::BallTrail,
        CosmeticSlot::DeathEffect
    };

    for (CosmeticSlot slot : slots)
    {
        auto items = pm.GetCosmeticsBySlot(slot);
        std::vector<std::string> unlockedIds;
        for (const auto& it : items)
        {
            if (it.Unlocked) unlockedIds.push_back(it.Id);
        }
        if (!unlockedIds.empty())
        {
            std::uniform_int_distribution<size_t> dist(0, unlockedIds.size() - 1);
            pm.EquipCosmetic(slot, unlockedIds[dist(rng)]);
        }
    }

    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    TriggerShowroomBurst(m_ballPos, 1.25f);
    ShowToast("RANDOMIZED LOADOUT!", GetPreviewPrimaryColor());
    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 22.0f, 1.15f);
}

void ProfileScene::PerformFullSaveReset(GameContext& context)
{
    PowerUpManager::Get().ResetToDefaults();
    ProfileManager::Get().ResetToDefaults();
    {
        LevelManager lm;
        lm.ResetProgression();
    }
    {
        LeaderboardManager lb;
        lb.ResetToDefaults();
    }
    PlayerPrefs::Save();

    m_resetArmed = false;
    m_resetArmTimer = 0.0f;
    m_nicknameBuffer = ProfileManager::Get().GetUsername();
    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    ShowToast("SAVE DATA RESET TO DEFAULTS", Color{255, 110, 90, 255});
    if (m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 24.0f, 0.85f);
}

ProfileScene::Rect ProfileScene::GetShowroomRect(const Vector2f& viewSize) const
{
    const float halfH = viewSize.Y * 0.5f;
    const float topY = -halfH + kHeaderH + 14.0f;
    const float h = std::clamp(viewSize.Y * 0.395f, 340.0f, 435.0f);
    const float w = viewSize.X - kSideMargin * 2.0f;
    return Rect{Vector2f{0.0f, topY + h * 0.5f}, Vector2f{w, h}};
}

ProfileScene::Rect ProfileScene::GetTabRect(int tabIdx, const Vector2f& viewSize) const
{
    const Rect arena = GetShowroomRect(viewSize);
    const float tabTop = arena.Center.Y + arena.Size.Y * 0.5f + 12.0f;
    const float tabH = 48.0f;
    const float totalW = viewSize.X - kSideMargin * 2.0f;
    const float gap = 10.0f;
    const float tabW = (totalW - gap * static_cast<float>(kTabCount - 1)) / static_cast<float>(kTabCount);
    const float startX = -totalW * 0.5f + tabW * 0.5f;
    return Rect{Vector2f{startX + static_cast<float>(tabIdx) * (tabW + gap), tabTop + tabH * 0.5f}, Vector2f{tabW, tabH}};
}

ProfileScene::Rect ProfileScene::GetInspectorRect(const Vector2f& viewSize) const
{
    const float halfH = viewSize.Y * 0.5f;
    const float h = 108.0f;
    const float w = viewSize.X - kSideMargin * 2.0f;
    return Rect{Vector2f{0.0f, halfH - 14.0f - h * 0.5f}, Vector2f{w, h}};
}

ProfileScene::Rect ProfileScene::GetGridItemRect(int itemIdx, int itemCount, bool isColorTab, const Vector2f& viewSize) const
{
    const Rect tab0 = GetTabRect(0, viewSize);
    const Rect insp = GetInspectorRect(viewSize);
    const float gridTop = tab0.Center.Y + tab0.Size.Y * 0.5f + 12.0f;
    const float gridBot = insp.Center.Y - insp.Size.Y * 0.5f - 12.0f;
    const float availH = std::max(180.0f, gridBot - gridTop);
    const float totalW = viewSize.X - kSideMargin * 2.0f;

    const int cols = isColorTab ? 8 : 4;
    const int rows = std::max(1, (itemCount + cols - 1) / cols);
    const float gapX = isColorTab ? 12.0f : 16.0f;
    const float gapY = isColorTab ? 12.0f : 14.0f;

    const float cellW = (totalW - gapX * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float cellH = (availH - gapY * static_cast<float>(rows - 1)) / static_cast<float>(rows);

    const int col = itemIdx % cols;
    const int row = itemIdx / cols;

    return Rect{
        Vector2f{
            -totalW * 0.5f + cellW * 0.5f + static_cast<float>(col) * (cellW + gapX),
            gridTop + cellH * 0.5f + static_cast<float>(row) * (cellH + gapY)
        },
        Vector2f{cellW, cellH}
    };
}

Color ProfileScene::GetPreviewPrimaryColor() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::PrimaryColor)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return it->SwatchColor;
        }
    }
    return pm.GetEquippedPrimaryColor();
}

Color ProfileScene::GetPreviewSecondaryColor() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::SecondaryColor)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return it->SwatchColor;
        }
    }
    return pm.GetEquippedSecondaryColor();
}

PaddleSkinStyle ProfileScene::GetPreviewPaddleStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::PaddleSkin)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolvePaddleSkinStyle(it->Id);
        }
    }
    return pm.GetEquippedPaddleSkinStyle();
}

BallSkinStyle ProfileScene::GetPreviewBallStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::BallSkin)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolveBallSkinStyle(it->Id);
        }
    }
    return pm.GetEquippedBallSkinStyle();
}

TrailStyle ProfileScene::GetPreviewTrailStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::BallTrail)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolveTrailStyle(it->Id);
        }
    }
    return pm.GetEquippedTrailStyle();
}

DeathEffectStyle ProfileScene::GetPreviewDeathStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::DeathEffect)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolveDeathEffectStyle(it->Id);
        }
    }
    return pm.GetEquippedDeathEffectStyle();
}

const CosmeticItem* ProfileScene::GetInspectedItem() const
{
    if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_currentItems.size()))
    {
        return &m_currentItems[static_cast<size_t>(m_hoveredIndex)];
    }
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_currentItems.size()))
    {
        return &m_currentItems[static_cast<size_t>(m_selectedIndex)];
    }
    return nullptr;
}

void ProfileScene::UpdateShowroomSimulation(float dt, const Vector2f& viewSize, const Vector2f& mouse, bool mouseInArena)
{
    const float clampedDt = std::min(dt, 0.05f);
    const Rect arena = GetShowroomRect(viewSize);

    // Effective playfield inside showroom (left of the floating Loadout Summary card)
    const float leftBound = arena.Center.X - arena.Size.X * 0.5f + 26.0f;
    const float rightBound = arena.Center.X + arena.Size.X * 0.5f - kLoadoutBoxW - 34.0f;
    const float topBound = arena.Center.Y - arena.Size.Y * 0.5f + 56.0f;
    const float botBound = arena.Center.Y + arena.Size.Y * 0.5f - 22.0f;
    const float paddleY = botBound - 38.0f;

    // 1. Paddle motion (smooth mouse follow inside showroom, otherwise sleek arcade patrol)
    const float halfPaddleW = kShowroomPaddleW * 0.5f;
    float targetX = m_paddlePos.X;
    if (mouseInArena && mouse.X <= rightBound + 20.0f)
    {
        targetX = mouse.X;
    }
    else
    {
        const float playCenterX = (leftBound + rightBound) * 0.5f;
        const float trackTarget = playCenterX + (m_ballPos.X - playCenterX) * 0.62f + std::sin(m_animTime * 1.35f) * 90.0f;
        targetX = trackTarget;
    }
    targetX = std::clamp(targetX, leftBound + halfPaddleW + 6.0f, rightBound - halfPaddleW - 6.0f);

    const float prevX = m_paddlePos.X;
    const float followLerp = std::clamp(clampedDt * 12.0f, 0.0f, 1.0f);
    m_paddlePos.X += (targetX - m_paddlePos.X) * followLerp;
    m_paddlePos.Y = paddleY;
    m_paddleVelX = (clampedDt > 0.0001f) ? (m_paddlePos.X - prevX) / clampedDt : 0.0f;
    const float targetTilt = std::clamp(m_paddleVelX * 0.0105f, -9.5f, 9.5f);
    m_paddleTiltDeg += (targetTilt - m_paddleTiltDeg) * std::clamp(clampedDt * 14.0f, 0.0f, 1.0f);

    if (m_paddleHitPulse > 0.0f)
    {
        m_paddleHitPulse = std::max(0.0f, m_paddleHitPulse - clampedDt * 4.5f);
    }

    // 2. Ball motion & bounces
    m_ballPos.X += m_ballVel.X * clampedDt;
    m_ballPos.Y += m_ballVel.Y * clampedDt;

    if (m_ballPos.X - kShowroomBallRadius < leftBound)
    {
        m_ballPos.X = leftBound + kShowroomBallRadius;
        m_ballVel.X = std::abs(m_ballVel.X);
    }
    else if (m_ballPos.X + kShowroomBallRadius > rightBound)
    {
        m_ballPos.X = rightBound - kShowroomBallRadius;
        m_ballVel.X = -std::abs(m_ballVel.X);
    }

    if (m_ballPos.Y - kShowroomBallRadius < topBound)
    {
        m_ballPos.Y = topBound + kShowroomBallRadius;
        m_ballVel.Y = std::abs(m_ballVel.Y);
    }

    // Paddle collision
    const float paddleTop = m_paddlePos.Y - kShowroomPaddleH * 0.5f;
    if (m_ballVel.Y > 0.0f &&
        m_ballPos.Y + kShowroomBallRadius >= paddleTop &&
        m_ballPos.Y - kShowroomBallRadius <= m_paddlePos.Y + kShowroomPaddleH * 0.5f &&
        m_ballPos.X >= m_paddlePos.X - halfPaddleW - kShowroomBallRadius * 0.6f &&
        m_ballPos.X <= m_paddlePos.X + halfPaddleW + kShowroomBallRadius * 0.6f)
    {
        m_ballPos.Y = paddleTop - kShowroomBallRadius;
        const float relX = std::clamp((m_ballPos.X - m_paddlePos.X) / halfPaddleW, -1.0f, 1.0f);
        const float speed = 445.0f;
        const float angle = relX * 0.92f;
        m_ballVel.X = std::sin(angle) * speed;
        m_ballVel.Y = -std::cos(angle) * speed;
        if (std::abs(m_ballVel.X) < 110.0f)
        {
            m_ballVel.X = (m_ballVel.X >= 0.0f) ? 140.0f : -140.0f;
        }
        m_paddleHitPulse = 1.0f;
    }

    // Floor safety bounce (triggers cosmetic death burst if ball misses paddle!)
    if (m_ballPos.Y + kShowroomBallRadius > botBound)
    {
        TriggerShowroomBurst(m_ballPos, 1.1f);
        m_ballPos.Y = botBound - kShowroomBallRadius;
        m_ballVel.Y = -std::abs(m_ballVel.Y);
    }

    // 3. Synchronize live TrailConfig whenever previewed trail/colors change
    const TrailStyle previewTrail = GetPreviewTrailStyle();
    const Color previewPri = GetPreviewPrimaryColor();
    const Color previewSec = GetPreviewSecondaryColor();
    if (previewTrail != m_cachedTrailStyle ||
        previewPri.r != m_cachedTrailPrimary.r || previewPri.g != m_cachedTrailPrimary.g || previewPri.b != m_cachedTrailPrimary.b ||
        previewSec.r != m_cachedTrailSecondary.r || previewSec.g != m_cachedTrailSecondary.g || previewSec.b != m_cachedTrailSecondary.b)
    {
        m_cachedTrailStyle = previewTrail;
        m_cachedTrailPrimary = previewPri;
        m_cachedTrailSecondary = previewSec;
        m_ballTrail.Config = CosmeticRenderer::BuildTrailConfig(previewTrail, previewPri, previewSec);
    }

    // Update trail points (same distance-based sampling as BallSystem::UpdateBallTrails)
    for (auto& pt : m_ballTrail.Points)
    {
        pt.Age += clampedDt;
    }
    while (!m_ballTrail.Points.empty() && m_ballTrail.Points.back().Age >= m_ballTrail.Config.Lifetime)
    {
        m_ballTrail.Points.pop_back();
    }

    if (!m_ballTrail.HasRecordedFirst)
    {
        m_ballTrail.LastRecordedPos = m_ballPos;
        m_ballTrail.HasRecordedFirst = true;
    }
    else
    {
        const Vector2f delta = m_ballPos - m_ballTrail.LastRecordedPos;
        const float dist = delta.Length();
        if (dist >= m_ballTrail.Config.MinPointDistance)
        {
            if (dist > m_ballTrail.Config.MaxPointDistance && m_ballTrail.Config.MaxPointDistance > 0.0f)
            {
                const int steps = std::min(4, static_cast<int>(std::floor(dist / m_ballTrail.Config.MaxPointDistance)));
                for (int s = 1; s <= steps; ++s)
                {
                    const float alpha = static_cast<float>(s) / static_cast<float>(steps + 1);
                    const Vector2f subPos = m_ballTrail.LastRecordedPos + delta * alpha;
                    m_ballTrail.Points.push_front(TrailPoint{subPos, 0.0f, m_ballTrail.Config.StartWidth, m_ballTrail.Config.StartColor});
                }
            }
            m_ballTrail.Points.push_front(TrailPoint{m_ballPos, 0.0f, m_ballTrail.Config.StartWidth, m_ballTrail.Config.StartColor});
            m_ballTrail.LastRecordedPos = m_ballPos;
            if (m_ballTrail.Points.size() > 64)
            {
                m_ballTrail.Points.resize(64);
            }
        }
    }

    // 4. Auto-trigger explosion bursts periodically when browsing the Death / Explosion tab
    if (m_viewMode == ViewMode::Garage && TabIndexToSlot(m_activeTab) == CosmeticSlot::DeathEffect)
    {
        m_autoBurstTimer -= clampedDt;
        if (m_autoBurstTimer <= 0.0f)
        {
            m_autoBurstTimer = 1.65f;
            TriggerShowroomBurst(m_ballPos, 1.25f);
        }
    }

    m_explosions.Update(clampedDt);
}

void ProfileScene::OnUpdate(float dt, GameContext& context)
{
    m_animTime += dt;

    if (m_toastTimer > 0.0f)
    {
        m_toastTimer = std::max(0.0f, m_toastTimer - dt);
    }
    if (m_resetArmed)
    {
        m_resetArmTimer -= dt;
        if (m_resetArmTimer <= 0.0f)
        {
            m_resetArmed = false;
        }
    }

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f mouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    if (m_editingNickname)
    {
        // Bypass DefaultScene::OnUpdate while typing so Num0 doesn't exit to MenuScene
        Scene::OnUpdate(dt, context);
        HandleNicknameInput(context, viewSize, mouse);
    }
    else
    {
        DefaultScene::OnUpdate(dt, context);
        HandleInput(context, viewSize, mouse);
    }

    const Rect arena = GetShowroomRect(viewSize);
    const bool mouseInArena = (m_viewMode == ViewMode::Garage) && arena.Contains(mouse);
    UpdateShowroomSimulation(dt, viewSize, mouse, mouseInArena);
}

void ProfileScene::HandleNicknameInput(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    const std::string& entered = context.Input.GetEnteredText();
    for (char ch : entered)
    {
        if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t') continue;
        if (static_cast<unsigned char>(ch) >= 32 && m_nicknameBuffer.size() < 16)
        {
            m_nicknameBuffer.push_back(ch);
        }
    }

    if (context.Input.IsKeyPress(KeyCode::Backspace) && !m_nicknameBuffer.empty())
    {
        m_nicknameBuffer.pop_back();
    }

    auto commitName = [&]()
    {
        std::string trimmed = m_nicknameBuffer;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        if (trimmed.empty()) trimmed = "PLAYER";
        ProfileManager::Get().SetUsername(trimmed);
        m_nicknameBuffer = ProfileManager::Get().GetUsername();
        m_editingNickname = false;
        ShowToast("CALLSIGN UPDATED: " + m_nicknameBuffer, Color{0, 235, 255, 255});
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
    };

    if (context.Input.IsKeyPress(KeyCode::Enter))
    {
        commitName();
        return;
    }

    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        m_nicknameBuffer = ProfileManager::Get().GetUsername();
        m_editingNickname = false;
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        return;
    }

    // Clicking outside the nickname pill commits the edit
    if (context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        const float halfW = viewSize.X * 0.5f;
        const float halfH = viewSize.Y * 0.5f;
        const float headerY = -halfH + kHeaderH * 0.5f;
        const Rect nickPill{Vector2f{halfW - 555.0f, headerY}, Vector2f{290.0f, 40.0f}};
        if (!nickPill.Contains(mouse))
        {
            commitName();
        }
    }
}

void ProfileScene::HandleInput(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float headerY = -halfH + kHeaderH * 0.5f;
    const bool leftClick = context.Input.IsMouseButtonPress(MouseButton::Left);

    // Escape returns to TV Menu on CH 08 PROFILE
    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        ReturnToMenu(context);
        return;
    }

    // Header buttons
    const Rect backBtn{Vector2f{-halfW + kSideMargin + 72.0f, headerY}, Vector2f{144.0f, 42.0f}};
    const Rect garageBtn{Vector2f{-halfW + kSideMargin + 250.0f, headerY}, Vector2f{168.0f, 42.0f}};
    const Rect careerBtn{Vector2f{-halfW + kSideMargin + 448.0f, headerY}, Vector2f{208.0f, 42.0f}};
    const Rect nickPill{Vector2f{halfW - 555.0f, headerY}, Vector2f{290.0f, 40.0f}};

    if (leftClick)
    {
        if (backBtn.Contains(mouse))
        {
            ReturnToMenu(context);
            return;
        }
        if (garageBtn.Contains(mouse))
        {
            if (m_viewMode != ViewMode::Garage)
            {
                m_viewMode = ViewMode::Garage;
                if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
            }
            return;
        }
        if (careerBtn.Contains(mouse))
        {
            if (m_viewMode != ViewMode::Career)
            {
                m_viewMode = ViewMode::Career;
                ProfileManager::Get().EvaluateAchievementUnlocks();
                if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
            }
            return;
        }
        if (nickPill.Contains(mouse))
        {
            m_editingNickname = true;
            m_nicknameBuffer = ProfileManager::Get().GetUsername();
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return;
        }
    }

    // ---- GARAGE MODE INPUT ----
    if (m_viewMode == ViewMode::Garage)
    {
        // Keyboard shortcuts: 1-6 tabs, Left/Right/Q/E cycle tabs, Space triggers burst
        if (context.Input.IsKeyPress(KeyCode::Num1)) SwitchTab(0, &context);
        if (context.Input.IsKeyPress(KeyCode::Num2)) SwitchTab(1, &context);
        if (context.Input.IsKeyPress(KeyCode::Num3)) SwitchTab(2, &context);
        if (context.Input.IsKeyPress(KeyCode::Num4)) SwitchTab(3, &context);
        if (context.Input.IsKeyPress(KeyCode::Num5)) SwitchTab(4, &context);
        if (context.Input.IsKeyPress(KeyCode::Num6)) SwitchTab(5, &context);
        if (context.Input.IsKeyPress(KeyCode::Q) || context.Input.IsKeyPress(KeyCode::Left))
        {
            SwitchTab((m_activeTab + kTabCount - 1) % kTabCount, &context);
        }
        if (context.Input.IsKeyPress(KeyCode::E) || context.Input.IsKeyPress(KeyCode::Right))
        {
            SwitchTab((m_activeTab + 1) % kTabCount, &context);
        }
        if (context.Input.IsKeyPress(KeyCode::Space))
        {
            TriggerShowroomBurst(m_ballPos, 1.3f);
            if (m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 20.0f, 1.15f);
        }

        // Update hovered item in grid for instant live showroom preview
        const int prevHovered = m_hoveredIndex;
        m_hoveredIndex = -1;
        const bool isColorTab = (m_activeTab == 0 || m_activeTab == 1);
        const int itemCount = static_cast<int>(m_currentItems.size());
        for (int i = 0; i < itemCount; ++i)
        {
            const Rect r = GetGridItemRect(i, itemCount, isColorTab, viewSize);
            if (r.Contains(mouse))
            {
                m_hoveredIndex = i;
                break;
            }
        }
        // Trigger immediate preview explosion when hovering a new Death Effect card
        if (m_hoveredIndex != prevHovered && m_hoveredIndex >= 0 &&
            TabIndexToSlot(m_activeTab) == CosmeticSlot::DeathEffect)
        {
            TriggerShowroomBurst(m_ballPos, 1.25f);
            m_autoBurstTimer = 1.65f;
        }

        if (!leftClick) return;

        // Showroom overlay buttons & interactive burst click
        const Rect arena = GetShowroomRect(viewSize);
        const float loadoutCenterX = arena.Center.X + arena.Size.X * 0.5f - kLoadoutBoxW * 0.5f - 14.0f;
        const float arenaBot = arena.Center.Y + arena.Size.Y * 0.5f;
        const Rect testBurstBtn{Vector2f{loadoutCenterX - 66.0f, arenaBot - 36.0f}, Vector2f{124.0f, 38.0f}};
        const Rect randomizeBtn{Vector2f{loadoutCenterX + 66.0f, arenaBot - 36.0f}, Vector2f{124.0f, 38.0f}};

        if (testBurstBtn.Contains(mouse))
        {
            TriggerShowroomBurst(m_ballPos, 1.35f);
            if (m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 20.0f, 1.15f);
            return;
        }
        if (randomizeBtn.Contains(mouse))
        {
            RandomizeEquippedLoadout(context);
            return;
        }
        if (arena.Contains(mouse) && mouse.X < loadoutCenterX - kLoadoutBoxW * 0.5f - 8.0f)
        {
            TriggerShowroomBurst(mouse, 1.25f);
            if (m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 18.0f, 1.2f);
            return;
        }

        // Category tabs
        for (int t = 0; t < kTabCount; ++t)
        {
            if (GetTabRect(t, viewSize).Contains(mouse))
            {
                SwitchTab(t, &context);
                return;
            }
        }

        // Cosmetic grid items
        if (m_hoveredIndex >= 0 && m_hoveredIndex < itemCount)
        {
            const bool alreadySelected = (m_selectedIndex == m_hoveredIndex);
            m_selectedIndex = m_hoveredIndex;
            const CosmeticItem& item = m_currentItems[static_cast<size_t>(m_selectedIndex)];
            if (item.Unlocked)
            {
                TriggerPrimaryActionForSelected(context);
            }
            else if (alreadySelected && item.UnlockType == CosmeticUnlockType::Shop)
            {
                // Second click on an already-selected shop item attempts buy & equip
                TriggerPrimaryActionForSelected(context);
            }
            else
            {
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 15.0f);
                if (TabIndexToSlot(m_activeTab) == CosmeticSlot::DeathEffect)
                {
                    TriggerShowroomBurst(m_ballPos, 1.25f);
                }
            }
            return;
        }

        // Inspector primary action button
        const Rect insp = GetInspectorRect(viewSize);
        const Rect actionBtn{Vector2f{insp.Center.X + insp.Size.X * 0.5f - 165.0f, insp.Center.Y}, Vector2f{290.0f, 58.0f}};
        if (actionBtn.Contains(mouse))
        {
            TriggerPrimaryActionForSelected(context);
            return;
        }
    }
    // ---- CAREER & MILESTONES MODE INPUT ----
    else
    {
        const float wheel = context.Input.GetMouseWheelDelta();
        if (std::abs(wheel) > 0.01f)
        {
            m_careerScrollY = std::max(0.0f, m_careerScrollY - wheel * 55.0f);
        }

        if (!leftClick) return;

        const float bodyTop = -halfH + kHeaderH + 20.0f;
        const float bodyBot = halfH - 24.0f;
        const float leftColW = 540.0f;
        const float leftColX = -halfW + kSideMargin + leftColW * 0.5f;

        // Edit callsign button inside Pilot Crest card
        const Rect editNameBtn{Vector2f{leftColX, bodyTop + 188.0f}, Vector2f{220.0f, 38.0f}};
        if (editNameBtn.Contains(mouse))
        {
            m_editingNickname = true;
            m_nicknameBuffer = ProfileManager::Get().GetUsername();
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return;
        }

        // 2-step Reset Save Data button
        const Rect resetBtn{Vector2f{leftColX, bodyBot - 36.0f}, Vector2f{leftColW - 40.0f, 48.0f}};
        if (resetBtn.Contains(mouse))
        {
            if (!m_resetArmed)
            {
                m_resetArmed = true;
                m_resetArmTimer = 4.0f;
                ShowToast("CLICK AGAIN WITHIN 4S TO CONFIRM FULL RESET", Color{255, 110, 75, 255});
                if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 18.0f, 0.7f);
            }
            else
            {
                PerformFullSaveReset(context);
            }
            return;
        }

        // Quick-equip unlocked achievement cosmetics from the right column
        auto& pm = ProfileManager::Get();
        const auto& all = pm.GetAvailableCosmetics();
        std::vector<CosmeticItem> milestones;
        for (const auto& c : all)
        {
            if (c.UnlockType == CosmeticUnlockType::Achievement)
            {
                milestones.push_back(c);
            }
        }

        const float rightLeft = -halfW + kSideMargin + leftColW + 22.0f;
        const float rightRight = halfW - kSideMargin;
        const float rightW = rightRight - rightLeft;
        const float listTop = bodyTop + 56.0f;
        const float cardW = (rightW - 16.0f) * 0.5f;
        const float cardH = 114.0f;
        const float gapY = 12.0f;

        for (size_t i = 0; i < milestones.size(); ++i)
        {
            const int col = static_cast<int>(i % 2);
            const int row = static_cast<int>(i / 2);
            const float cx = rightLeft + cardW * 0.5f + static_cast<float>(col) * (cardW + 16.0f);
            const float cy = listTop + cardH * 0.5f + static_cast<float>(row) * (cardH + gapY) - m_careerScrollY;
            if (cy + cardH * 0.5f < listTop - 10.0f || cy - cardH * 0.5f > bodyBot + 10.0f) continue;

            const Rect cardRect{Vector2f{cx, cy}, Vector2f{cardW, cardH}};
            if (cardRect.Contains(mouse) && milestones[i].Unlocked)
            {
                pm.EquipCosmetic(milestones[i].Slot, milestones[i].Id);
                RefreshCategoryItems();
                ShowToast("EQUIPPED: " + milestones[i].Name, pm.GetEquippedPrimaryColor());
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 20.0f);
                return;
            }
        }
    }
}

void ProfileScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    Camera2D cam{};
    cam.Position = Vector2f{0.0f, 0.0f};
    cam.Zoom = 1.0f;
    cam.Rotation = 0.0f;
    context.Render.SetCamera(cam);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f mouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    const Color previewPri = GetPreviewPrimaryColor();
    const Color previewSec = GetPreviewSecondaryColor();

    RenderBackground(context, viewSize, previewPri, previewSec);
    RenderHeaderBar(context, viewSize, mouse, previewPri);

    if (m_viewMode == ViewMode::Garage)
    {
        RenderGarageMode(context, viewSize, mouse);
    }
    else
    {
        RenderCareerMode(context, viewSize, mouse, previewPri, previewSec);
    }

    RenderToast(context, viewSize);
}

void ProfileScene::RenderBackground(GameContext& context, const Vector2f& viewSize, Color primary, Color secondary)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;

    // Subtle geometric neon grid background tinted gently by player's colors
    const Color gridCol = ScaleAlpha(MixColor(Color{24, 34, 54, 255}, primary, 0.16f), 0.55f);
    const float scroll = std::fmod(m_animTime * 18.0f, 64.0f);
    for (float gx = -halfW + scroll; gx <= halfW; gx += 64.0f)
    {
        context.Render.DrawLine(Vector2f{gx, -halfH}, Vector2f{gx, halfH}, gridCol, 1.0f);
    }
    for (float gy = -halfH; gy <= halfH; gy += 64.0f)
    {
        context.Render.DrawLine(Vector2f{-halfW, gy}, Vector2f{halfW, gy}, gridCol, 1.0f);
    }

    // Subtle top ambient gradient glow strip
    context.Render.DrawRectangle(viewSize.X, 240.0f, Transform2D{Vector2f{0.0f, -halfH + 120.0f}}, ScaleAlpha(primary, 0.05f), BlendMode::Add);
    context.Render.DrawRectangle(viewSize.X, 160.0f, Transform2D{Vector2f{0.0f, halfH - 80.0f}}, ScaleAlpha(secondary, 0.04f), BlendMode::Add);
}

void ProfileScene::RenderHeaderBar(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float headerY = -halfH + kHeaderH * 0.5f;

    context.Render.DrawRectangle(viewSize.X, kHeaderH, Transform2D{Vector2f{0.0f, headerY}}, Color{12, 18, 30, 245});
    context.Render.DrawLine(Vector2f{-halfW, -halfH + kHeaderH}, Vector2f{halfW, -halfH + kHeaderH}, ScaleAlpha(primary, 0.55f), 2.0f);

    auto drawHeaderBtn = [&](const Rect& r, const std::string& label, bool active, Color accent)
    {
        const bool hov = r.Contains(mouse);
        const Color fill = active ? ScaleAlpha(accent, 0.25f) : (hov ? Color{32, 46, 70, 235} : Color{20, 28, 44, 230});
        const Color border = active ? accent : (hov ? Colors::White : Color{75, 95, 130, 210});
        context.Render.DrawRectangle(r.Size.X, r.Size.Y, Transform2D{r.Center}, fill);
        context.Render.DrawRectangleOutline(r.Size.X, r.Size.Y, Transform2D{r.Center}, border, active ? 2.5f : 1.5f);
        if (m_fontId != 0)
        {
            context.Render.DrawTextCentered(label, m_fontId, 22.0f, Transform2D{r.Center}, active ? Colors::White : Color{210, 225, 245, 255});
        }
    };

    const Rect backBtn{Vector2f{-halfW + kSideMargin + 72.0f, headerY}, Vector2f{144.0f, 42.0f}};
    const Rect garageBtn{Vector2f{-halfW + kSideMargin + 250.0f, headerY}, Vector2f{168.0f, 42.0f}};
    const Rect careerBtn{Vector2f{-halfW + kSideMargin + 448.0f, headerY}, Vector2f{208.0f, 42.0f}};

    drawHeaderBtn(backBtn, "< MENU", false, Color{255, 110, 110, 255});
    drawHeaderBtn(garageBtn, "GARAGE", m_viewMode == ViewMode::Garage, primary);
    drawHeaderBtn(careerBtn, "CAREER & STATS", m_viewMode == ViewMode::Career, Color{255, 210, 75, 255});

    // Pilot Callsign editable pill
    const Rect nickPill{Vector2f{halfW - 555.0f, headerY}, Vector2f{290.0f, 40.0f}};
    const bool nickHov = nickPill.Contains(mouse);
    context.Render.DrawRectangle(nickPill.Size.X, nickPill.Size.Y, Transform2D{nickPill.Center},
                                 m_editingNickname ? Color{28, 44, 72, 250} : Color{18, 26, 42, 235});
    context.Render.DrawRectangleOutline(nickPill.Size.X, nickPill.Size.Y, Transform2D{nickPill.Center},
                                        m_editingNickname ? primary : (nickHov ? Colors::White : Color{75, 100, 140, 210}),
                                        m_editingNickname ? 2.5f : 1.5f);
    if (m_fontId != 0)
    {
        const bool blink = (std::fmod(m_animTime * 2.4f, 1.0f) < 0.55f);
        std::string displayNick = m_editingNickname
            ? ("PILOT: " + m_nicknameBuffer + (blink ? "_" : " "))
            : ("PILOT: " + ProfileManager::Get().GetUsername() + " [EDIT]");
        context.Render.DrawTextCentered(displayNick, m_fontId, 20.0f, Transform2D{nickPill.Center},
                                        m_editingNickname ? Colors::White : Color{210, 232, 255, 255});
    }

    // Unlocked count pill
    const auto& pm = ProfileManager::Get();
    const Rect unlockedPill{Vector2f{halfW - 295.0f, headerY}, Vector2f{190.0f, 40.0f}};
    context.Render.DrawRectangle(unlockedPill.Size.X, unlockedPill.Size.Y, Transform2D{unlockedPill.Center}, Color{18, 28, 44, 235});
    context.Render.DrawRectangleOutline(unlockedPill.Size.X, unlockedPill.Size.Y, Transform2D{unlockedPill.Center}, Color{95, 175, 220, 210}, 1.5f);
    if (m_fontId != 0)
    {
        std::string uStr = "ITEMS " + std::to_string(pm.GetTotalUnlockedCosmetics()) + "/" + std::to_string(pm.GetTotalCosmetics());
        context.Render.DrawTextCentered(uStr, m_fontId, 20.0f, Transform2D{unlockedPill.Center}, Color{155, 230, 255, 255});
    }

    // Credits balance pill
    const Rect creditsPill{Vector2f{halfW - 110.0f, headerY}, Vector2f{156.0f, 40.0f}};
    context.Render.DrawRectangle(creditsPill.Size.X, creditsPill.Size.Y, Transform2D{creditsPill.Center}, Color{36, 28, 12, 240});
    context.Render.DrawRectangleOutline(creditsPill.Size.X, creditsPill.Size.Y, Transform2D{creditsPill.Center}, Color{255, 205, 65, 230}, 2.0f);
    if (m_fontId != 0)
    {
        std::string crStr = "[C] " + std::to_string(PowerUpManager::Get().GetCoins()) + " CR";
        context.Render.DrawTextCentered(crStr, m_fontId, 21.0f, Transform2D{creditsPill.Center}, Color{255, 224, 95, 255});
    }
}

void ProfileScene::RenderGarageMode(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    const Color previewPri = GetPreviewPrimaryColor();
    const Color previewSec = GetPreviewSecondaryColor();

    RenderShowroom(context, viewSize, mouse);
    RenderCategoryTabs(context, viewSize, mouse, previewPri);
    RenderItemGrid(context, viewSize, mouse, previewPri, previewSec);
    RenderInspectorBar(context, viewSize, mouse, previewPri);
}

void ProfileScene::RenderShowroom(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    const Rect arena = GetShowroomRect(viewSize);
    const Color pri = GetPreviewPrimaryColor();
    const Color sec = GetPreviewSecondaryColor();
    const PaddleSkinStyle paddleStyle = GetPreviewPaddleStyle();
    const BallSkinStyle ballStyle = GetPreviewBallStyle();

    const float arenaLeft = arena.Center.X - arena.Size.X * 0.5f;
    const float arenaRight = arena.Center.X + arena.Size.X * 0.5f;
    const float arenaTop = arena.Center.Y - arena.Size.Y * 0.5f;
    const float arenaBot = arena.Center.Y + arena.Size.Y * 0.5f;
    const float playRight = arenaRight - kLoadoutBoxW - 24.0f;

    // 1. Showroom Stage Backdrop
    context.Render.DrawRectangle(arena.Size.X, arena.Size.Y, Transform2D{arena.Center}, Color{11, 16, 28, 244});

    // Perspective Geometry Dash-style scrolling synth floor inside play stage
    const float horizonY = arenaTop + arena.Size.Y * 0.48f;
    const Color floorGridCol = ScaleAlpha(pri, 0.22f);
    const Color horizonCol = ScaleAlpha(sec, 0.26f);
    context.Render.DrawLine(Vector2f{arenaLeft + 8.0f, horizonY}, Vector2f{playRight, horizonY}, horizonCol, 1.5f);

    for (int i = 1; i <= 5; ++i)
    {
        const float t = static_cast<float>(i) / 5.0f;
        const float y = horizonY + (arenaBot - horizonY) * (t * t);
        context.Render.DrawLine(Vector2f{arenaLeft + 8.0f, y}, Vector2f{playRight, y}, ScaleAlpha(pri, 0.14f + 0.12f * t), 1.0f);
    }

    const float stageCenterX = (arenaLeft + playRight) * 0.5f;
    const float scrollX = std::fmod(m_animTime * 42.0f, 95.0f);
    for (int i = -9; i <= 9; ++i)
    {
        const float topX = stageCenterX + static_cast<float>(i) * 86.0f + scrollX * 0.45f;
        const float botX = stageCenterX + (topX - stageCenterX) * 1.95f;
        const float clampedTopX = std::clamp(topX, arenaLeft + 8.0f, playRight);
        const float clampedBotX = std::clamp(botX, arenaLeft + 8.0f, playRight);
        if (topX >= arenaLeft - 60.0f && topX <= playRight + 60.0f)
        {
            context.Render.DrawLine(Vector2f{clampedTopX, horizonY}, Vector2f{clampedBotX, arenaBot - 4.0f}, floorGridCol, 1.0f);
        }
    }

    // Subtle upward spotlight cone under the live paddle
    const float pulse = 1.0f + m_paddleHitPulse * 0.35f;
    context.Render.DrawCircle(98.0f * pulse, Transform2D{m_paddlePos}, ScaleAlpha(pri, 0.085f), BlendMode::Add);
    context.Render.DrawCircle(56.0f * pulse, Transform2D{m_paddlePos}, ScaleAlpha(sec, 0.075f), BlendMode::Add);

    // 2. Live 60fps Particle Trail + Ball + Paddle + Death/Explosion Bursts!
    CosmeticRenderer::RenderTrail(context.Render, m_ballTrail, m_animTime, m_trailScratch);
    CosmeticRenderer::DrawBall(context.Render, m_ballPos, kShowroomBallRadius, ballStyle, pri, sec, m_animTime, 1.0f, false, false, false, m_ballTexId);

    const float squashX = 1.0f + m_paddleHitPulse * 0.07f;
    const float squashY = 1.0f - m_paddleHitPulse * 0.08f;
    CosmeticRenderer::DrawPaddle(
        context.Render,
        m_paddlePos,
        Vector2f{kShowroomPaddleW * squashX, kShowroomPaddleH * squashY},
        m_paddleTiltDeg,
        paddleStyle,
        pri,
        sec,
        m_animTime,
        1.0f,
        m_paddleTexId);

    m_explosions.Render(context.Render);

    // 3. Stage labels (top-left)
    if (m_fontId != 0)
    {
        context.Render.DrawText("CUSTOMIZATION GARAGE // LIVE 60FPS SHOWROOM", m_fontId, 22.0f,
                                Transform2D{Vector2f{arenaLeft + 20.0f, arenaTop + 12.0f}}, pri);

        std::string subHint = "MOVE MOUSE TO STEER PADDLE  •  CLICK ARENA OR [SPACE] TO TEST EXPLOSION";
        if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_currentItems.size()))
        {
            const auto& hovItem = m_currentItems[static_cast<size_t>(m_hoveredIndex)];
            subHint = "LIVE PREVIEW: " + hovItem.Name + (hovItem.Unlocked ? "" : " (LOCKED)");
        }
        context.Render.DrawText(subHint, m_fontId, 17.0f,
                                Transform2D{Vector2f{arenaLeft + 20.0f, arenaTop + 38.0f}},
                                (m_hoveredIndex >= 0) ? Color{255, 230, 110, 255} : Color{165, 185, 215, 220});
    }

    // 4. Floating Right Loadout Summary Card
    const float loadoutCenterX = arenaRight - kLoadoutBoxW * 0.5f - 14.0f;
    const Rect loadoutRect{Vector2f{loadoutCenterX, arena.Center.Y}, Vector2f{kLoadoutBoxW, arena.Size.Y - 24.0f}};
    context.Render.DrawRectangle(loadoutRect.Size.X, loadoutRect.Size.Y, Transform2D{loadoutRect.Center}, Color{15, 22, 36, 238});
    context.Render.DrawRectangleOutline(loadoutRect.Size.X, loadoutRect.Size.Y, Transform2D{loadoutRect.Center}, ScaleAlpha(pri, 0.5f), 1.5f);

    const float lx = loadoutRect.Center.X - loadoutRect.Size.X * 0.5f + 16.0f;
    float ly = loadoutRect.Center.Y - loadoutRect.Size.Y * 0.5f + 14.0f;

    if (m_fontId != 0)
    {
        context.Render.DrawText("EQUIPPED LOADOUT", m_fontId, 21.0f, Transform2D{Vector2f{lx, ly}}, Colors::White);
        ly += 30.0f;
        context.Render.DrawLine(Vector2f{lx, ly}, Vector2f{lx + loadoutRect.Size.X - 32.0f, ly}, ScaleAlpha(pri, 0.4f), 1.5f);
        ly += 12.0f;

        const auto& pm = ProfileManager::Get();
        auto drawLoadoutRow = [&](const char* label, CosmeticSlot slot, bool showSwatch)
        {
            const std::string eqId = pm.GetEquippedCosmetic(slot);
            const CosmeticItem* item = pm.GetCosmeticById(eqId);
            const std::string valName = item ? item->Name : eqId;

            context.Render.DrawText(label, m_fontId, 16.0f, Transform2D{Vector2f{lx, ly}}, Color{140, 165, 200, 230});
            if (showSwatch && item)
            {
                context.Render.DrawRectangle(16.0f, 16.0f, Transform2D{Vector2f{lx + 10.0f, ly + 26.0f}}, item->SwatchColor);
                context.Render.DrawRectangleOutline(16.0f, 16.0f, Transform2D{Vector2f{lx + 10.0f, ly + 26.0f}}, Colors::White, 1.0f);
                context.Render.DrawText(valName, m_fontId, 18.0f, Transform2D{Vector2f{lx + 26.0f, ly + 15.0f}}, Colors::White);
            }
            else
            {
                context.Render.DrawText(valName, m_fontId, 18.0f, Transform2D{Vector2f{lx, ly + 16.0f}}, Colors::White);
            }
            ly += 44.0f;
        };

        drawLoadoutRow("PRIMARY COLOR", CosmeticSlot::PrimaryColor, true);
        drawLoadoutRow("SECONDARY COLOR", CosmeticSlot::SecondaryColor, true);
        drawLoadoutRow("PADDLE SKIN", CosmeticSlot::PaddleSkin, false);
        drawLoadoutRow("BALL SKIN", CosmeticSlot::BallSkin, false);
        drawLoadoutRow("PARTICLE TRAIL", CosmeticSlot::BallTrail, false);
        drawLoadoutRow("DEATH EFFECT", CosmeticSlot::DeathEffect, false);
    }

    // Loadout footer action buttons: [TEST BURST] & [RANDOMIZE]
    const Rect testBurstBtn{Vector2f{loadoutCenterX - 66.0f, arenaBot - 36.0f}, Vector2f{124.0f, 38.0f}};
    const Rect randomizeBtn{Vector2f{loadoutCenterX + 66.0f, arenaBot - 36.0f}, Vector2f{124.0f, 38.0f}};
    auto drawSmallBtn = [&](const Rect& r, const char* text, Color accent)
    {
        const bool hov = r.Contains(mouse);
        context.Render.DrawRectangle(r.Size.X, r.Size.Y, Transform2D{r.Center}, hov ? ScaleAlpha(accent, 0.32f) : Color{22, 32, 50, 240});
        context.Render.DrawRectangleOutline(r.Size.X, r.Size.Y, Transform2D{r.Center}, hov ? Colors::White : accent, 1.5f);
        if (m_fontId != 0)
        {
            context.Render.DrawTextCentered(text, m_fontId, 17.0f, Transform2D{r.Center}, Colors::White);
        }
    };
    drawSmallBtn(testBurstBtn, "TEST BURST", Color{255, 130, 50, 255});
    drawSmallBtn(randomizeBtn, "RANDOMIZE", pri);

    // Outer showroom border
    context.Render.DrawRectangleOutline(arena.Size.X, arena.Size.Y, Transform2D{arena.Center}, ScaleAlpha(pri, 0.72f), 2.5f);
}

void ProfileScene::RenderCategoryTabs(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary)
{
    const auto& pm = ProfileManager::Get();

    for (int t = 0; t < kTabCount; ++t)
    {
        const Rect r = GetTabRect(t, viewSize);
        const bool active = (t == m_activeTab);
        const bool hov = r.Contains(mouse);
        const CosmeticSlot slot = TabIndexToSlot(t);
        const int unlocked = pm.GetUnlockedCosmeticCount(slot);
        const int total = pm.GetTotalCosmeticCount(slot);

        const Color bg = active ? ScaleAlpha(primary, 0.24f) : (hov ? Color{28, 40, 62, 235} : Color{16, 23, 38, 235});
        const Color border = active ? primary : (hov ? Color{190, 215, 245, 230} : Color{58, 76, 108, 210});

        context.Render.DrawRectangle(r.Size.X, r.Size.Y, Transform2D{r.Center}, bg);
        context.Render.DrawRectangleOutline(r.Size.X, r.Size.Y, Transform2D{r.Center}, border, active ? 2.5f : 1.5f);

        if (active)
        {
            context.Render.DrawRectangle(r.Size.X, 4.0f, Transform2D{Vector2f{r.Center.X, r.Center.Y + r.Size.Y * 0.5f - 2.0f}}, primary);
        }

        if (m_fontId != 0)
        {
            std::string label = std::string(TabIndexToLabel(t)) + " (" + std::to_string(unlocked) + "/" + std::to_string(total) + ")";
            context.Render.DrawTextCentered(label, m_fontId, 19.0f, Transform2D{r.Center},
                                            active ? Colors::White : Color{185, 205, 230, 240});
        }
    }
}

void ProfileScene::RenderItemGrid(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color previewPri, Color previewSec)
{
    const auto& pm = ProfileManager::Get();
    const CosmeticSlot slot = TabIndexToSlot(m_activeTab);
    const std::string equippedId = pm.GetEquippedCosmetic(slot);
    const bool isColorTab = (slot == CosmeticSlot::PrimaryColor || slot == CosmeticSlot::SecondaryColor);
    const int itemCount = static_cast<int>(m_currentItems.size());

    for (int i = 0; i < itemCount; ++i)
    {
        const CosmeticItem& item = m_currentItems[static_cast<size_t>(i)];
        const Rect r = GetGridItemRect(i, itemCount, isColorTab, viewSize);
        const bool isEquipped = (item.Id == equippedId);
        const bool isSelected = (i == m_selectedIndex);
        const bool isHovered = r.Contains(mouse);

        Color cardBg = item.Unlocked ? Color{18, 26, 42, 235} : Color{13, 18, 30, 235};
        if (isEquipped) cardBg = MixColor(cardBg, previewPri, 0.16f);
        else if (isHovered) cardBg = Color{28, 40, 62, 245};

        Color borderCol = Color{55, 72, 102, 200};
        float borderThick = 1.5f;
        if (isEquipped)
        {
            borderCol = Color{80, 255, 165, 255};
            borderThick = 3.0f;
        }
        else if (isSelected)
        {
            borderCol = previewPri;
            borderThick = 2.5f;
        }
        else if (isHovered)
        {
            borderCol = Colors::White;
            borderThick = 2.0f;
        }

        context.Render.DrawRectangle(r.Size.X, r.Size.Y, Transform2D{r.Center}, cardBg);

        if (isColorTab)
        {
            // Glossy Geometry Dash-inspired color swatch tile
            const float swW = r.Size.X - 24.0f;
            const float swH = r.Size.Y * 0.56f;
            const Vector2f swCenter{r.Center.X, r.Center.Y - r.Size.Y * 0.12f};

            Color swatchCol = item.SwatchColor;
            if (!item.Unlocked)
            {
                swatchCol = MixColor(swatchCol, Color{20, 26, 38, 255}, 0.42f);
            }
            context.Render.DrawRectangle(swW, swH, Transform2D{swCenter}, swatchCol);
            // Glossy top bevel strip
            context.Render.DrawRectangle(swW - 8.0f, swH * 0.28f,
                                         Transform2D{Vector2f{swCenter.X, swCenter.Y - swH * 0.28f}},
                                         Color{255, 255, 255, item.Unlocked ? static_cast<uint8_t>(75) : static_cast<uint8_t>(30)});
            context.Render.DrawRectangleOutline(swW, swH, Transform2D{swCenter},
                                                item.Unlocked ? Color{255, 255, 255, 185} : Color{110, 125, 150, 150}, 1.5f);

            if (!item.Unlocked)
            {
                CosmeticRenderer::DrawLockIcon(context.Render, swCenter, 18.0f, Color{240, 245, 255, 235});
            }

            if (m_fontId != 0)
            {
                context.Render.DrawTextCentered(item.Name, m_fontId, 17.0f,
                                                Transform2D{Vector2f{r.Center.X, r.Center.Y + r.Size.Y * 0.25f}},
                                                item.Unlocked ? Colors::White : Color{165, 180, 205, 220});

                std::string badge;
                Color badgeCol{130, 255, 175, 255};
                if (isEquipped)
                {
                    badge = "EQUIPPED";
                    badgeCol = Color{90, 255, 165, 255};
                }
                else if (item.Unlocked)
                {
                    badge = "OWNED";
                    badgeCol = Color{130, 200, 240, 220};
                }
                else if (item.UnlockType == CosmeticUnlockType::Shop)
                {
                    badge = std::to_string(item.Price) + " CR";
                    badgeCol = Color{255, 215, 80, 245};
                }
                else
                {
                    badge = "MILESTONE";
                    badgeCol = Color{255, 155, 70, 245};
                }
                context.Render.DrawTextCentered(badge, m_fontId, 15.0f,
                                                Transform2D{Vector2f{r.Center.X, r.Center.Y + r.Size.Y * 0.40f}},
                                                badgeCol);
            }
        }
        else
        {
            // Rich Cosmetic Card with live animated procedural preview!
            const Vector2f previewCenter{r.Center.X, r.Center.Y - r.Size.Y * 0.14f};
            const float alphaScale = item.Unlocked ? 1.0f : 0.65f;

            // Subtle preview stage pad behind icon
            context.Render.DrawRectangle(r.Size.X - 24.0f, r.Size.Y * 0.56f, Transform2D{previewCenter}, Color{10, 15, 25, 210});

            if (slot == CosmeticSlot::PaddleSkin)
            {
                CosmeticRenderer::DrawPaddle(
                    context.Render,
                    previewCenter,
                    Vector2f{std::min(195.0f, r.Size.X * 0.56f), 26.0f},
                    isHovered ? std::sin(m_animTime * 4.0f) * 3.5f : 0.0f,
                    ProfileManager::ResolvePaddleSkinStyle(item.Id),
                    previewPri,
                    previewSec,
                    m_animTime,
                    alphaScale,
                    m_paddleTexId);
            }
            else if (slot == CosmeticSlot::BallSkin)
            {
                CosmeticRenderer::DrawBall(
                    context.Render,
                    previewCenter,
                    24.0f,
                    ProfileManager::ResolveBallSkinStyle(item.Id),
                    previewPri,
                    previewSec,
                    m_animTime,
                    alphaScale,
                    false,
                    false,
                    false,
                    m_ballTexId);
            }
            else if (slot == CosmeticSlot::BallTrail)
            {
                CosmeticRenderer::DrawTrailPreviewIcon(
                    context.Render,
                    previewCenter,
                    Vector2f{r.Size.X - 56.0f, r.Size.Y * 0.42f},
                    ProfileManager::ResolveTrailStyle(item.Id),
                    previewPri,
                    previewSec,
                    m_animTime);
            }
            else if (slot == CosmeticSlot::DeathEffect)
            {
                CosmeticRenderer::DrawDeathEffectPreviewIcon(
                    context.Render,
                    previewCenter,
                    32.0f,
                    ProfileManager::ResolveDeathEffectStyle(item.Id),
                    previewPri,
                    previewSec,
                    m_animTime);
            }

            // Lock badge in top-right if locked
            if (!item.Unlocked)
            {
                const Vector2f lockPos{r.Center.X + r.Size.X * 0.5f - 26.0f, r.Center.Y - r.Size.Y * 0.5f + 24.0f};
                context.Render.DrawCircle(16.0f, Transform2D{lockPos}, Color{12, 16, 26, 225});
                CosmeticRenderer::DrawLockIcon(context.Render, lockPos, 13.0f, Color{255, 210, 95, 240});
            }

            // Bottom text bar inside card
            if (m_fontId != 0)
            {
                const float textLeft = r.Center.X - r.Size.X * 0.5f + 16.0f;
                const float textRight = r.Center.X + r.Size.X * 0.5f - 16.0f;
                const float titleY = r.Center.Y + r.Size.Y * 0.22f;
                context.Render.DrawText(item.Name, m_fontId, 20.0f, Transform2D{Vector2f{textLeft, titleY}},
                                        item.Unlocked ? Colors::White : Color{185, 198, 220, 235});

                std::string statusText;
                Color statusCol{110, 255, 170, 255};
                if (isEquipped)
                {
                    statusText = "[EQUIPPED]";
                    statusCol = Color{90, 255, 165, 255};
                }
                else if (item.Unlocked)
                {
                    statusText = "OWNED";
                    statusCol = Color{130, 210, 245, 235};
                }
                else if (item.UnlockType == CosmeticUnlockType::Shop)
                {
                    statusText = std::to_string(item.Price) + " CR";
                    statusCol = Color{255, 215, 80, 255};
                }
                else
                {
                    const auto prog = pm.GetCosmeticMilestoneProgress(item.Id);
                    statusText = "MILESTONE " + std::to_string(static_cast<int>(prog.GetRatio() * 100.0f)) + "%";
                    statusCol = Color{255, 165, 70, 255};
                }
                const Vector2f sz = context.Render.GetTextSize(statusText, m_fontId, 18.0f);
                context.Render.DrawText(statusText, m_fontId, 18.0f, Transform2D{Vector2f{textRight - sz.X, titleY + 2.0f}}, statusCol);
            }
        }

        context.Render.DrawRectangleOutline(r.Size.X, r.Size.Y, Transform2D{r.Center}, borderCol, borderThick);
    }
}

void ProfileScene::RenderInspectorBar(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary)
{
    const Rect insp = GetInspectorRect(viewSize);
    context.Render.DrawRectangle(insp.Size.X, insp.Size.Y, Transform2D{insp.Center}, Color{13, 19, 32, 245});
    context.Render.DrawRectangleOutline(insp.Size.X, insp.Size.Y, Transform2D{insp.Center}, ScaleAlpha(primary, 0.55f), 2.0f);

    const CosmeticItem* item = GetInspectedItem();
    if (!item || m_fontId == 0) return;

    const auto& pm = ProfileManager::Get();
    const CosmeticSlot slot = TabIndexToSlot(m_activeTab);
    const bool isEquipped = (pm.GetEquippedCosmetic(slot) == item->Id);

    const float leftX = insp.Center.X - insp.Size.X * 0.5f + 24.0f;
    const float topY = insp.Center.Y - insp.Size.Y * 0.5f + 14.0f;

    // Item Title + Category Tag
    std::string header = std::string("[") + SlotToShortTag(item->Slot) + "]  " + item->Name;
    context.Render.DrawText(header, m_fontId, 26.0f, Transform2D{Vector2f{leftX, topY}}, Colors::White);
    context.Render.DrawText(item->Description, m_fontId, 19.0f, Transform2D{Vector2f{leftX, topY + 34.0f}}, Color{180, 200, 228, 240});

    // Unlock requirement & progress bar when locked
    if (!item->Unlocked)
    {
        if (item->UnlockType == CosmeticUnlockType::Achievement)
        {
            const auto prog = pm.GetCosmeticMilestoneProgress(item->Id);
            std::string reqLine = "UNLOCK REQUIREMENT: " + item->UnlockRequirement +
                                  "  (" + std::to_string(prog.Current) + " / " + std::to_string(prog.Target) +
                                  (prog.UnitLabel.empty() ? "" : (" " + prog.UnitLabel)) + ")";
            context.Render.DrawText(reqLine, m_fontId, 18.0f, Transform2D{Vector2f{leftX, topY + 62.0f}}, Color{255, 195, 85, 255});

            // Progress bar
            const float barW = 360.0f;
            const float barH = 14.0f;
            const Vector2f barCenter{insp.Center.X + 185.0f, insp.Center.Y + 24.0f};
            context.Render.DrawRectangle(barW, barH, Transform2D{barCenter}, Color{24, 32, 48, 240});
            const float fillW = std::max(0.0f, barW * prog.GetRatio());
            if (fillW > 1.0f)
            {
                context.Render.DrawRectangle(fillW, barH - 2.0f,
                                             Transform2D{Vector2f{barCenter.X - barW * 0.5f + fillW * 0.5f, barCenter.Y}},
                                             Color{255, 185, 65, 245});
            }
            context.Render.DrawRectangleOutline(barW, barH, Transform2D{barCenter}, Color{130, 150, 185, 220}, 1.0f);
        }
        else if (item->UnlockType == CosmeticUnlockType::Shop)
        {
            std::string reqLine = "SHOP UNLOCK: " + std::to_string(item->Price) + " CREDITS  (YOUR BANK: " +
                                  std::to_string(PowerUpManager::Get().GetCoins()) + " CR)";
            context.Render.DrawText(reqLine, m_fontId, 18.0f, Transform2D{Vector2f{leftX, topY + 62.0f}}, Color{255, 220, 95, 255});
        }
    }
    else
    {
        context.Render.DrawText(isEquipped ? "STATUS: CURRENTLY EQUIPPED ACROSS ALL GAME MODES" : "STATUS: UNLOCKED — CLICK CARD OR EQUIP BUTTON TO APPLY",
                                m_fontId, 18.0f, Transform2D{Vector2f{leftX, topY + 62.0f}},
                                isEquipped ? Color{95, 255, 165, 255} : Color{135, 215, 255, 240});
    }

    // Primary Action Button (right side of Inspector)
    const Rect actionBtn{Vector2f{insp.Center.X + insp.Size.X * 0.5f - 165.0f, insp.Center.Y}, Vector2f{290.0f, 58.0f}};
    const bool hov = actionBtn.Contains(mouse);

    std::string btnLabel;
    Color btnFill{24, 34, 54, 240};
    Color btnBorder{110, 130, 165, 220};
    Color btnText = Colors::White;

    if (isEquipped)
    {
        btnLabel = (slot == CosmeticSlot::DeathEffect) ? "[ EQUIPPED • TEST ]" : "[ EQUIPPED ]";
        btnFill = Color{18, 65, 48, 245};
        btnBorder = Color{90, 255, 165, 255};
        btnText = Color{130, 255, 185, 255};
    }
    else if (item->Unlocked)
    {
        btnLabel = "EQUIP ITEM";
        btnFill = hov ? ScaleAlpha(primary, 0.42f) : ScaleAlpha(primary, 0.25f);
        btnBorder = primary;
    }
    else if (item->UnlockType == CosmeticUnlockType::Shop)
    {
        const bool canAfford = (PowerUpManager::Get().GetCoins() >= item->Price);
        if (canAfford)
        {
            btnLabel = "BUY & EQUIP (" + std::to_string(item->Price) + " CR)";
            btnFill = hov ? Color{95, 72, 18, 250} : Color{65, 48, 12, 245};
            btnBorder = Color{255, 215, 70, 255};
            btnText = Color{255, 232, 115, 255};
        }
        else
        {
            btnLabel = "NEED " + std::to_string(item->Price) + " CR";
            btnFill = Color{54, 20, 24, 240};
            btnBorder = Color{210, 75, 75, 220};
            btnText = Color{255, 135, 135, 240};
        }
    }
    else
    {
        const auto prog = pm.GetCosmeticMilestoneProgress(item->Id);
        btnLabel = "MILESTONE (" + std::to_string(static_cast<int>(prog.GetRatio() * 100.0f)) + "%)";
        btnFill = Color{42, 30, 18, 235};
        btnBorder = Color{230, 155, 60, 220};
        btnText = Color{255, 195, 95, 245};
    }

    context.Render.DrawRectangle(actionBtn.Size.X, actionBtn.Size.Y, Transform2D{actionBtn.Center}, btnFill);
    context.Render.DrawRectangleOutline(actionBtn.Size.X, actionBtn.Size.Y, Transform2D{actionBtn.Center}, hov ? Colors::White : btnBorder, 2.5f);
    context.Render.DrawTextCentered(btnLabel, m_fontId, 22.0f, Transform2D{actionBtn.Center}, btnText);
}

void ProfileScene::RenderCareerMode(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse, Color primary, Color secondary)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float bodyTop = -halfH + kHeaderH + 20.0f;
    const float bodyBot = halfH - 24.0f;
    const float bodyH = bodyBot - bodyTop;
    const auto& pm = ProfileManager::Get();

    // ---- LEFT COLUMN: Pilot Crest, Career Telemetry, Reset Save ----
    const float leftColW = 540.0f;
    const float leftColX = -halfW + kSideMargin + leftColW * 0.5f;
    const Rect leftPanel{Vector2f{leftColX, bodyTop + bodyH * 0.5f}, Vector2f{leftColW, bodyH}};
    context.Render.DrawRectangle(leftPanel.Size.X, leftPanel.Size.Y, Transform2D{leftPanel.Center}, Color{13, 19, 32, 242});
    context.Render.DrawRectangleOutline(leftPanel.Size.X, leftPanel.Size.Y, Transform2D{leftPanel.Center}, ScaleAlpha(primary, 0.55f), 2.0f);

    // Live Pilot Emblem (equipped paddle + ball floating in crest box)
    const Rect crestBox{Vector2f{leftColX, bodyTop + 88.0f}, Vector2f{leftColW - 40.0f, 136.0f}};
    context.Render.DrawRectangle(crestBox.Size.X, crestBox.Size.Y, Transform2D{crestBox.Center}, Color{9, 14, 24, 240});
    context.Render.DrawRectangleOutline(crestBox.Size.X, crestBox.Size.Y, Transform2D{crestBox.Center}, ScaleAlpha(primary, 0.65f), 1.5f);

    const Vector2f crestBallPos{crestBox.Center.X + std::sin(m_animTime * 2.2f) * 65.0f, crestBox.Center.Y - 22.0f + std::cos(m_animTime * 3.1f) * 8.0f};
    CosmeticRenderer::DrawBall(context.Render, crestBallPos, 16.0f, pm.GetEquippedBallSkinStyle(), primary, secondary, m_animTime, 1.0f, false, false, false, m_ballTexId);
    CosmeticRenderer::DrawPaddle(context.Render, Vector2f{crestBox.Center.X, crestBox.Center.Y + 26.0f}, Vector2f{160.0f, 24.0f}, 0.0f,
                                 pm.GetEquippedPaddleSkinStyle(), primary, secondary, m_animTime, 1.0f, m_paddleTexId);

    const Rect editNameBtn{Vector2f{leftColX, bodyTop + 188.0f}, Vector2f{220.0f, 38.0f}};
    const bool editHov = editNameBtn.Contains(mouse);
    context.Render.DrawRectangle(editNameBtn.Size.X, editNameBtn.Size.Y, Transform2D{editNameBtn.Center},
                                 editHov ? ScaleAlpha(primary, 0.3f) : Color{22, 32, 52, 240});
    context.Render.DrawRectangleOutline(editNameBtn.Size.X, editNameBtn.Size.Y, Transform2D{editNameBtn.Center},
                                        editHov ? Colors::White : primary, 1.5f);
    if (m_fontId != 0)
    {
        context.Render.DrawTextCentered("EDIT CALLSIGN", m_fontId, 19.0f, Transform2D{editNameBtn.Center}, Colors::White);
    }

    // Career Statistics Table
    if (m_fontId != 0)
    {
        const float statLeft = leftColX - leftColW * 0.5f + 26.0f;
        const float statRight = leftColX + leftColW * 0.5f - 26.0f;
        float sy = bodyTop + 230.0f;

        context.Render.DrawText("PILOT TELEMETRY // " + pm.GetUsername(), m_fontId, 23.0f, Transform2D{Vector2f{statLeft, sy}}, primary);
        sy += 34.0f;
        context.Render.DrawLine(Vector2f{statLeft, sy}, Vector2f{statRight, sy}, ScaleAlpha(primary, 0.4f), 1.5f);
        sy += 14.0f;

        int upgradedPwrCount = 0;
        const PowerUpType allPwr[] = {
            PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::Storm,
            PowerUpType::TempoBall, PowerUpType::ExtraLife, PowerUpType::BigBall,
            PowerUpType::FireBall, PowerUpType::StickyPaddle, PowerUpType::BottomBarrier,
            PowerUpType::MagnetPaddle, PowerUpType::GhostBall, PowerUpType::ItemRegen,
            PowerUpType::ShieldBarrier
        };
        for (PowerUpType t : allPwr)
        {
            if (PowerUpManager::Get().GetPersistedLevel(t) > 1) ++upgradedPwrCount;
        }

        const int highestCleared = std::max(0, PlayerPrefs::GetInt("HighestClearedLevel", 0));

        auto drawStatRow = [&](const char* label, const std::string& value, Color valCol)
        {
            context.Render.DrawText(label, m_fontId, 20.0f, Transform2D{Vector2f{statLeft, sy}}, Color{175, 195, 225, 240});
            const Vector2f sz = context.Render.GetTextSize(value, m_fontId, 20.0f);
            context.Render.DrawText(value, m_fontId, 20.0f, Transform2D{Vector2f{statRight - sz.X, sy}}, valCol);
            sy += 36.0f;
        };

        drawStatRow("LIFETIME SCORE", FormatIntWithCommas(pm.GetLifetimeScore()) + " PTS", Colors::White);
        drawStatRow("TOTAL CREDITS EARNED", FormatIntWithCommas(pm.GetLifetimeCredits()) + " CR", Color{255, 224, 95, 255});
        drawStatRow("CURRENT BANK", FormatIntWithCommas(static_cast<uint32_t>(std::max(0, PowerUpManager::Get().GetCoins()))) + " CR", Color{255, 215, 70, 255});
        drawStatRow("HIGHEST STAGE CLEARED", "STAGE " + std::to_string(highestCleared), primary);
        drawStatRow("POWER-UPS UPGRADED", std::to_string(upgradedPwrCount) + " / 13", Color{125, 235, 255, 255});
        drawStatRow("COSMETICS COLLECTED", std::to_string(pm.GetTotalUnlockedCosmetics()) + " / " + std::to_string(pm.GetTotalCosmetics()), Color{110, 255, 175, 255});

        sy += 10.0f;
        context.Render.DrawText("ENDLESS MODE RECORDS", m_fontId, 21.0f, Transform2D{Vector2f{statLeft, sy}}, Color{255, 175, 75, 255});
        sy += 30.0f;
        context.Render.DrawLine(Vector2f{statLeft, sy}, Vector2f{statRight, sy}, Color{255, 175, 75, 110}, 1.5f);
        sy += 12.0f;

        drawStatRow("BEST SURVIVAL TIME", pm.GetEndlessBestTimeFormatted(), Color{255, 215, 120, 255});
        drawStatRow("BEST WAVE REACHED", "WAVE " + std::to_string(pm.GetEndlessBestWaves()), Color{255, 215, 120, 255});
        drawStatRow("ENDLESS HIGH SCORE", FormatIntWithCommas(pm.GetEndlessHighScore()) + " PTS", Colors::White);
        drawStatRow("TOTAL ENDLESS RUNS", std::to_string(pm.GetEndlessTotalRuns()), Color{190, 205, 230, 255});
    }

    // 2-step Reset Save Data button at bottom of left column
    const Rect resetBtn{Vector2f{leftColX, bodyBot - 36.0f}, Vector2f{leftColW - 40.0f, 48.0f}};
    const bool resetHov = resetBtn.Contains(mouse);
    const Color rFill = m_resetArmed ? Color{155, 28, 32, 250} : (resetHov ? Color{95, 26, 30, 245} : Color{56, 18, 22, 235});
    const Color rBorder = m_resetArmed ? Color{255, 95, 95, 255} : Color{210, 65, 65, 220};
    context.Render.DrawRectangle(resetBtn.Size.X, resetBtn.Size.Y, Transform2D{resetBtn.Center}, rFill);
    context.Render.DrawRectangleOutline(resetBtn.Size.X, resetBtn.Size.Y, Transform2D{resetBtn.Center}, rBorder, 2.0f);
    if (m_fontId != 0)
    {
        std::string rLabel = m_resetArmed ? "CONFIRM FULL RESET? CLICK AGAIN" : "RESET SAVE DATA";
        context.Render.DrawTextCentered(rLabel, m_fontId, 20.0f, Transform2D{resetBtn.Center}, Color{255, 205, 205, 255});
    }

    // ---- RIGHT COLUMN: Achievement Milestones & Unlock Tracker ----
    const float rightLeft = -halfW + kSideMargin + leftColW + 22.0f;
    const float rightRight = halfW - kSideMargin;
    const float rightW = rightRight - rightLeft;
    const Rect rightPanel{Vector2f{(rightLeft + rightRight) * 0.5f, bodyTop + bodyH * 0.5f}, Vector2f{rightW, bodyH}};
    context.Render.DrawRectangle(rightPanel.Size.X, rightPanel.Size.Y, Transform2D{rightPanel.Center}, Color{13, 19, 32, 242});
    context.Render.DrawRectangleOutline(rightPanel.Size.X, rightPanel.Size.Y, Transform2D{rightPanel.Center}, Color{255, 200, 75, 150}, 2.0f);

    if (m_fontId != 0)
    {
        context.Render.DrawText("ACHIEVEMENT MILESTONES & COSMETIC REWARDS", m_fontId, 24.0f,
                                Transform2D{Vector2f{rightLeft + 20.0f, bodyTop + 14.0f}}, Color{255, 220, 95, 255});
        context.Render.DrawText("COMPLETE MILESTONES TO UNLOCK EXCLUSIVE COSMETICS (CLICK UNLOCKED CARDS TO EQUIP)", m_fontId, 16.0f,
                                Transform2D{Vector2f{rightLeft + 20.0f, bodyTop + 40.0f}}, Color{165, 185, 215, 220});
    }

    const auto& all = ProfileManager::Get().GetAvailableCosmetics();
    std::vector<CosmeticItem> milestones;
    for (const auto& c : all)
    {
        if (c.UnlockType == CosmeticUnlockType::Achievement)
        {
            milestones.push_back(c);
        }
    }

    const float listTop = bodyTop + 66.0f;
    const float listBot = bodyBot - 16.0f;
    const float cardW = (rightW - 56.0f) * 0.5f;
    const float cardH = 114.0f;
    const float gapY = 12.0f;
    const int totalRows = static_cast<int>((milestones.size() + 1) / 2);
    const float totalContentH = static_cast<float>(totalRows) * (cardH + gapY);
    const float maxScroll = std::max(0.0f, totalContentH - (listBot - listTop));
    m_careerScrollY = std::clamp(m_careerScrollY, 0.0f, maxScroll);

    for (size_t i = 0; i < milestones.size(); ++i)
    {
        const CosmeticItem& item = milestones[i];
        const int col = static_cast<int>(i % 2);
        const int row = static_cast<int>(i / 2);
        const float cx = rightLeft + 20.0f + cardW * 0.5f + static_cast<float>(col) * (cardW + 16.0f);
        const float cy = listTop + cardH * 0.5f + static_cast<float>(row) * (cardH + gapY) - m_careerScrollY;

        if (cy - cardH * 0.5f < listTop - 6.0f || cy + cardH * 0.5f > listBot + 6.0f)
        {
            continue;
        }

        const Rect card{Vector2f{cx, cy}, Vector2f{cardW, cardH}};
        const bool hov = card.Contains(mouse);
        const bool isEquipped = (pm.GetEquippedCosmetic(item.Slot) == item.Id);
        const auto prog = pm.GetCosmeticMilestoneProgress(item.Id);

        const Color bg = item.Unlocked
            ? (isEquipped ? Color{18, 52, 42, 245} : (hov ? Color{28, 44, 68, 245} : Color{20, 32, 50, 240}))
            : Color{16, 20, 32, 235};
        const Color border = isEquipped
            ? Color{90, 255, 165, 255}
            : (item.Unlocked ? Color{100, 220, 255, 220} : Color{75, 90, 120, 190});

        context.Render.DrawRectangle(card.Size.X, card.Size.Y, Transform2D{card.Center}, bg);
        context.Render.DrawRectangleOutline(card.Size.X, card.Size.Y, Transform2D{card.Center}, border, isEquipped ? 2.5f : 1.5f);

        // Mini preview icon on left of card
        const Vector2f iconCenter{cx - cardW * 0.5f + 54.0f, cy};
        context.Render.DrawRectangle(80.0f, 80.0f, Transform2D{iconCenter}, Color{10, 14, 24, 220});
        context.Render.DrawRectangleOutline(80.0f, 80.0f, Transform2D{iconCenter}, ScaleAlpha(border, 0.5f), 1.0f);

        if (item.Slot == CosmeticSlot::PrimaryColor || item.Slot == CosmeticSlot::SecondaryColor)
        {
            context.Render.DrawRectangle(54.0f, 54.0f, Transform2D{iconCenter}, item.SwatchColor);
            context.Render.DrawRectangleOutline(54.0f, 54.0f, Transform2D{iconCenter}, Colors::White, 1.5f);
        }
        else if (item.Slot == CosmeticSlot::PaddleSkin)
        {
            CosmeticRenderer::DrawPaddle(context.Render, iconCenter, Vector2f{68.0f, 15.0f}, 0.0f,
                                         ProfileManager::ResolvePaddleSkinStyle(item.Id), primary, secondary, m_animTime, 1.0f, m_paddleTexId);
        }
        else if (item.Slot == CosmeticSlot::BallSkin)
        {
            CosmeticRenderer::DrawBall(context.Render, iconCenter, 18.0f,
                                       ProfileManager::ResolveBallSkinStyle(item.Id), primary, secondary, m_animTime, 1.0f, false, false, false, m_ballTexId);
        }
        else if (item.Slot == CosmeticSlot::BallTrail)
        {
            CosmeticRenderer::DrawTrailPreviewIcon(context.Render, iconCenter, Vector2f{64.0f, 38.0f},
                                                   ProfileManager::ResolveTrailStyle(item.Id), primary, secondary, m_animTime);
        }
        else if (item.Slot == CosmeticSlot::DeathEffect)
        {
            CosmeticRenderer::DrawDeathEffectPreviewIcon(context.Render, iconCenter, 22.0f,
                                                         ProfileManager::ResolveDeathEffectStyle(item.Id), primary, secondary, m_animTime);
        }

        if (!item.Unlocked)
        {
            CosmeticRenderer::DrawLockIcon(context.Render, Vector2f{iconCenter.X + 24.0f, iconCenter.Y - 24.0f}, 10.0f, Color{255, 200, 80, 235});
        }

        if (m_fontId != 0)
        {
            const float tx = cx - cardW * 0.5f + 108.0f;
            const float ty = cy - cardH * 0.5f + 12.0f;

            std::string title = std::string("[") + SlotToShortTag(item.Slot) + "] " + item.Name;
            context.Render.DrawText(title, m_fontId, 19.0f, Transform2D{Vector2f{tx, ty}}, Colors::White);
            context.Render.DrawText(item.UnlockRequirement, m_fontId, 16.0f, Transform2D{Vector2f{tx, ty + 26.0f}}, Color{185, 202, 230, 235});

            // Progress bar
            const float barW = cardW - 126.0f;
            const float barH = 12.0f;
            const Vector2f barC{tx + barW * 0.5f, ty + 62.0f};
            context.Render.DrawRectangle(barW, barH, Transform2D{barC}, Color{22, 30, 46, 240});
            const float fillW = std::max(0.0f, barW * (item.Unlocked ? 1.0f : prog.GetRatio()));
            if (fillW > 1.0f)
            {
                context.Render.DrawRectangle(fillW, barH - 2.0f,
                                             Transform2D{Vector2f{tx + fillW * 0.5f, barC.Y}},
                                             item.Unlocked ? Color{85, 245, 155, 245} : Color{255, 185, 60, 245});
            }

            std::string progLabel = item.Unlocked
                ? (isEquipped ? "UNLOCKED • EQUIPPED" : "UNLOCKED • CLICK TO EQUIP")
                : (std::to_string(prog.Current) + " / " + std::to_string(prog.Target) +
                   (prog.UnitLabel.empty() ? "" : (" " + prog.UnitLabel)));
            context.Render.DrawText(progLabel, m_fontId, 15.0f, Transform2D{Vector2f{tx, ty + 74.0f}},
                                    item.Unlocked ? Color{105, 255, 175, 255} : Color{255, 205, 105, 245});
        }
    }
}

void ProfileScene::RenderToast(GameContext& context, const Vector2f& viewSize)
{
    if (m_toastTimer <= 0.0f || m_toastText.empty() || m_fontId == 0)
    {
        return;
    }

    const float alpha = std::clamp(m_toastTimer / 0.45f, 0.0f, 1.0f);
    const float halfH = viewSize.Y * 0.5f;
    const Vector2f sz = context.Render.GetTextSize(m_toastText, m_fontId, 22.0f);
    const float boxW = std::max(280.0f, sz.X + 48.0f);
    const Vector2f pos{0.0f, -halfH + kHeaderH + 34.0f};

    context.Render.DrawRectangle(boxW, 40.0f, Transform2D{pos}, ScaleAlpha(Color{10, 16, 28, 245}, alpha));
    context.Render.DrawRectangleOutline(boxW, 40.0f, Transform2D{pos}, ScaleAlpha(m_toastColor, alpha), 2.0f);
    context.Render.DrawTextCentered(m_toastText, m_fontId, 22.0f, Transform2D{pos}, ScaleAlpha(m_toastColor, alpha));
}
