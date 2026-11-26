#include "Scenes/RobostockScene.h"
#include "Scenes/MenuScene.h"
#include "Scenes/EditorScene.h"
#include "Scenes/CustomLevelPlayScene.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/Transform2D.h"
#include "AudioMixer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr int kCardsPerPage = 9; // 3x3 vault grid
    constexpr float kTopBarH = 68.0f;
    constexpr float kRightPanelW = 520.0f;

    std::string FormatTwoDigits(int v)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d", std::max(0, v));
        return std::string(buf);
    }
}

RobostockScene::RobostockScene() = default;

RobostockScene::RobostockScene(std::string initialSelectedLevelId)
    : m_initialSelectedId(std::move(initialSelectedLevelId))
{
}

void RobostockScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    mp_context = &context;
    m_bgColor = Color{11, 15, 22, 255};

    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_switchSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_switch.wav");

    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 0.8f, AudioMixer::kNominalMusicLevel, true);

    m_library.Load();

    // Ensure at least one default Special Playlist exists for immediate drag & drop convenience
    if (m_library.GetPlaylists().empty())
    {
        m_library.CreatePlaylist("LIST 01");
    }

    if (!m_initialSelectedId.empty())
    {
        if (auto idx = m_library.FindLevelIndex(m_initialSelectedId); idx.has_value())
        {
            m_selectedLevelIdx = static_cast<int>(*idx);
            m_vaultPage = m_selectedLevelIdx / kCardsPerPage;
        }
    }

    const int lvlCount = static_cast<int>(m_library.GetLevels().size());
    m_selectedLevelIdx = (lvlCount > 0) ? std::clamp(m_selectedLevelIdx, 0, lvlCount - 1) : -1;
    m_activePlaylistIdx = m_library.GetPlaylists().empty()
        ? -1
        : std::clamp(m_activePlaylistIdx, 0, static_cast<int>(m_library.GetPlaylists().size()) - 1);
}

void RobostockScene::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}

void RobostockScene::OnUpdate(float dt, GameContext& context)
{
    m_animTimer += dt;
    mp_context = &context;

    if (m_modalMode != ModalMode::None)
    {
        // Bypass DefaultScene::OnUpdate while typing so Num0 doesn't exit to MenuScene
        Scene::OnUpdate(dt, context);
        HandleModalInput(context);
        return;
    }

    DefaultScene::OnUpdate(dt, context);
    HandleInput(context);
}

RobostockScene::CardRect RobostockScene::GetVaultCardRect(int visibleSlot, const Vector2f& viewSize) const
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;

    const float vaultMinX = -halfW + 28.0f;
    const float vaultMaxX = halfW - kRightPanelW - 44.0f;
    const float vaultW = std::max(480.0f, vaultMaxX - vaultMinX);

    const float gridTopY = -halfH + kTopBarH + 22.0f;
    const float gridBotY = halfH - 96.0f;
    const float availH = std::max(320.0f, gridBotY - gridTopY);

    const float gapX = 18.0f;
    const float gapY = 16.0f;
    const float cardW = (vaultW - gapX * 2.0f) / 3.0f;
    const float cardH = (availH - gapY * 2.0f) / 3.0f;

    const int col = visibleSlot % 3;
    const int row = visibleSlot / 3;

    CardRect r;
    r.Size = Vector2f{cardW, cardH};
    r.Center = Vector2f{
        vaultMinX + cardW * 0.5f + static_cast<float>(col) * (cardW + gapX),
        gridTopY + cardH * 0.5f + static_cast<float>(row) * (cardH + gapY)
    };
    return r;
}

RobostockScene::CardRect RobostockScene::GetPlaylistSlotRect(int slotIdx, const Vector2f& viewSize) const
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float panelCenterX = halfW - 22.0f - kRightPanelW * 0.5f;

    const float slotsTopY = -halfH + kTopBarH + 126.0f;
    const float slotsBotY = halfH - 96.0f;
    const float totalH = std::max(340.0f, slotsBotY - slotsTopY);
    const float stepY = totalH / static_cast<float>(CustomPlaylist::MAX_SLOTS);
    const float slotH = std::max(32.0f, stepY - 6.0f);

    CardRect r;
    r.Size = Vector2f{kRightPanelW - 36.0f, slotH};
    r.Center = Vector2f{panelCenterX, slotsTopY + stepY * (static_cast<float>(slotIdx) + 0.5f)};
    return r;
}

void RobostockScene::OpenModal(ModalMode mode, const std::string& initialText)
{
    m_modalMode = mode;
    m_modalTextBuffer = initialText;
}

void RobostockScene::ConfirmModal(GameContext& context)
{
    const auto& levels = m_library.GetLevels();
    const auto& playlists = m_library.GetPlaylists();

    if (m_modalMode == ModalMode::RenameLevel)
    {
        if (m_selectedLevelIdx >= 0 && m_selectedLevelIdx < static_cast<int>(levels.size()))
        {
            m_library.RenameLevel(levels[static_cast<size_t>(m_selectedLevelIdx)].Id, m_modalTextBuffer);
        }
    }
    else if (m_modalMode == ModalMode::CreatePlaylist)
    {
        const std::string newId = m_library.CreatePlaylist(m_modalTextBuffer);
        const auto& updated = m_library.GetPlaylists();
        for (size_t i = 0; i < updated.size(); ++i)
        {
            if (updated[i].Id == newId)
            {
                m_activePlaylistIdx = static_cast<int>(i);
                break;
            }
        }
    }
    else if (m_modalMode == ModalMode::RenamePlaylist)
    {
        if (m_activePlaylistIdx >= 0 && m_activePlaylistIdx < static_cast<int>(playlists.size()))
        {
            m_library.RenamePlaylist(playlists[static_cast<size_t>(m_activePlaylistIdx)].Id, m_modalTextBuffer);
        }
    }

    m_modalMode = ModalMode::None;
    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
}

void RobostockScene::HandleModalInput(GameContext& context)
{
    const std::string& entered = context.Input.GetEnteredText();
    for (char ch : entered)
    {
        if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t') continue;
        if (static_cast<unsigned char>(ch) >= 32 && m_modalTextBuffer.size() < 20)
        {
            m_modalTextBuffer.push_back(ch);
        }
    }

    if (context.Input.IsKeyPress(KeyCode::Backspace) && !m_modalTextBuffer.empty())
    {
        m_modalTextBuffer.pop_back();
    }

    if (context.Input.IsKeyPress(KeyCode::Enter))
    {
        ConfirmModal(context);
        return;
    }

    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        m_modalMode = ModalMode::None;
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        return;
    }

    if (context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        auto [mx, my] = context.Input.GetMousePosition();
        const Vector2f mouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

        const CardRect okBtn{Vector2f{-105.0f, 76.0f}, Vector2f{180.0f, 46.0f}};
        const CardRect cancelBtn{Vector2f{105.0f, 76.0f}, Vector2f{180.0f, 46.0f}};
        if (okBtn.Contains(mouse))
        {
            ConfirmModal(context);
            return;
        }
        if (cancelBtn.Contains(mouse))
        {
            m_modalMode = ModalMode::None;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }
    }
}

void RobostockScene::HandleInput(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f mouse{mx - halfW, my - halfH};

    const bool leftPress = context.Input.IsMouseButtonPress(MouseButton::Left);
    const bool leftDown = context.Input.IsMouseButtonDown(MouseButton::Left);

    // 1. Escape -> Return to TV Channel Grid on CH 06 ROBOSTOCK
    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        MenuScene::SetReturnChannelType(ChannelType::Robostock);
        MenuScene::SetReturnWithDezoom(true);
        context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
        return;
    }

    // 2. Active Drag-and-Drop tracking & release
    if (m_dragKind != DragKind::None)
    {
        m_dragCurrentPos = mouse;
        const float dx = m_dragCurrentPos.X - m_dragStartPos.X;
        const float dy = m_dragCurrentPos.Y - m_dragStartPos.Y;
        if (!m_dragActive && (dx * dx + dy * dy) > 64.0f)
        {
            m_dragActive = true;
        }

        if (!leftDown)
        {
            if (m_dragActive)
            {
                const float panelCenterX = halfW - 22.0f - kRightPanelW * 0.5f;
                const CardRect rightPanelRect{
                    Vector2f{panelCenterX, (kTopBarH * 0.5f)},
                    Vector2f{kRightPanelW, viewSize.Y - kTopBarH - 28.0f}
                };

                if (m_dragKind == DragKind::LevelCard)
                {
                    const auto& levels = m_library.GetLevels();
                    if (m_dragSourceIdx >= 0 && m_dragSourceIdx < static_cast<int>(levels.size()))
                    {
                        if (rightPanelRect.Contains(mouse))
                        {
                            // Drop level into Special Playlist (auto-create if none)
                            if (m_library.GetPlaylists().empty())
                            {
                                m_library.CreatePlaylist("LIST 01");
                                m_activePlaylistIdx = 0;
                            }
                            const auto& playlists = m_library.GetPlaylists();
                            m_activePlaylistIdx = std::clamp(m_activePlaylistIdx, 0, static_cast<int>(playlists.size()) - 1);
                            const std::string plId = playlists[static_cast<size_t>(m_activePlaylistIdx)].Id;
                            const std::string lvlId = levels[static_cast<size_t>(m_dragSourceIdx)].Id;

                            std::optional<size_t> targetSlot;
                            for (int s = 0; s < static_cast<int>(CustomPlaylist::MAX_SLOTS); ++s)
                            {
                                if (GetPlaylistSlotRect(s, viewSize).Contains(mouse))
                                {
                                    targetSlot = static_cast<size_t>(s);
                                    break;
                                }
                            }

                            if (m_library.AddLevelToPlaylist(plId, lvlId, targetSlot))
                            {
                                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
                            }
                        }
                        else
                        {
                            // Check drop on another vault card to reorder
                            const int pageStart = m_vaultPage * kCardsPerPage;
                            for (int vis = 0; vis < kCardsPerPage; ++vis)
                            {
                                const int targetIdx = pageStart + vis;
                                if (targetIdx >= static_cast<int>(levels.size())) break;
                                if (GetVaultCardRect(vis, viewSize).Contains(mouse) && targetIdx != m_dragSourceIdx)
                                {
                                    if (m_library.ReorderLevel(static_cast<size_t>(m_dragSourceIdx), static_cast<size_t>(targetIdx)))
                                    {
                                        m_selectedLevelIdx = targetIdx;
                                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                else if (m_dragKind == DragKind::PlaylistSlot)
                {
                    const auto& playlists = m_library.GetPlaylists();
                    if (m_activePlaylistIdx >= 0 && m_activePlaylistIdx < static_cast<int>(playlists.size()))
                    {
                        const auto& pl = playlists[static_cast<size_t>(m_activePlaylistIdx)];
                        if (!rightPanelRect.Contains(mouse))
                        {
                            // Dragged outside playlist panel -> remove slot
                            if (m_library.RemoveSlotFromPlaylist(pl.Id, static_cast<size_t>(m_dragSourceIdx)))
                            {
                                if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
                            }
                        }
                        else
                        {
                            for (int s = 0; s < static_cast<int>(pl.LevelIds.size()); ++s)
                            {
                                if (GetPlaylistSlotRect(s, viewSize).Contains(mouse) && s != m_dragSourceIdx)
                                {
                                    if (m_library.ReorderPlaylistSlot(pl.Id, static_cast<size_t>(m_dragSourceIdx), static_cast<size_t>(s)))
                                    {
                                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            m_dragKind = DragKind::None;
            m_dragSourceIdx = -1;
            m_dragActive = false;
            return;
        }

        if (m_dragActive)
        {
            return;
        }
    }

    const auto& levels = m_library.GetLevels();
    const int totalPages = std::max(1, (static_cast<int>(levels.size()) + kCardsPerPage - 1) / kCardsPerPage);
    m_vaultPage = std::clamp(m_vaultPage, 0, totalPages - 1);

    // Mouse wheel pages vault when cursor is on vault side
    const float wheel = context.Input.GetMouseWheelDelta();
    if (std::abs(wheel) > 0.01f && mouse.X < halfW - kRightPanelW - 30.0f)
    {
        m_vaultPage = std::clamp(m_vaultPage - (wheel > 0.0f ? 1 : -1), 0, totalPages - 1);
    }

    // Keyboard Enter -> Play selected level
    if (context.Input.IsKeyPress(KeyCode::Enter) && m_selectedLevelIdx >= 0 && m_selectedLevelIdx < static_cast<int>(levels.size()))
    {
        const std::string id = levels[static_cast<size_t>(m_selectedLevelIdx)].Id;
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
        context.Scenes.LoadSceneWithTransition<CustomLevelPlayScene>(SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f}, id);
        return;
    }

    if (!leftPress)
    {
        return;
    }

    // 3. Vault Cartridge Cards (Select + arm Drag)
    const float vaultMinX = -halfW + 28.0f;
    const float vaultMaxX = halfW - kRightPanelW - 44.0f;
    const float vaultCenterX = (vaultMinX + vaultMaxX) * 0.5f;

    const int pageStart = m_vaultPage * kCardsPerPage;
    for (int vis = 0; vis < kCardsPerPage; ++vis)
    {
        const int idx = pageStart + vis;
        if (idx >= static_cast<int>(levels.size())) break;
        if (GetVaultCardRect(vis, viewSize).Contains(mouse))
        {
            m_selectedLevelIdx = idx;
            m_dragKind = DragKind::LevelCard;
            m_dragSourceIdx = idx;
            m_dragStartPos = mouse;
            m_dragCurrentPos = mouse;
            m_dragActive = false;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return;
        }
    }

    // Empty vault center CTA
    if (levels.empty())
    {
        const CardRect emptyNewBtn{Vector2f{vaultCenterX, 24.0f}, Vector2f{210.0f, 54.0f}};
        if (emptyNewBtn.Contains(mouse))
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            context.Scenes.LoadSceneWithTransition<EditorScene>(SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f}, std::string{}, true);
            return;
        }
    }

    // 4. Centralized Bottom Action Bar: [< TV] [+ CREATE] [PLAY] [EDIT] [RENAME] [DELETE] (+ pagination if > 1 page)
    const float actY = halfH - 46.0f;
    const float btnW = 148.0f;
    const float gap = 12.0f;
    const int btnCount = 6;
    const float totalRowW = btnW * static_cast<float>(btnCount) + gap * static_cast<float>(btnCount - 1);
    const float startX = vaultCenterX - totalRowW * 0.5f + btnW * 0.5f;

    const CardRect backBtn{Vector2f{startX + 0.0f * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};
    const CardRect newBtn{Vector2f{startX + 1.0f * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};
    const CardRect playBtn{Vector2f{startX + 2.0f * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};
    const CardRect editBtn{Vector2f{startX + 3.0f * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};
    const CardRect renBtn{Vector2f{startX + 4.0f * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};
    const CardRect delBtn{Vector2f{startX + 5.0f * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};

    if (backBtn.Contains(mouse))
    {
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        MenuScene::SetReturnChannelType(ChannelType::Robostock);
        MenuScene::SetReturnWithDezoom(true);
        context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
        return;
    }

    if (newBtn.Contains(mouse))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        context.Scenes.LoadSceneWithTransition<EditorScene>(SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f}, std::string{}, true);
        return;
    }

    if (m_selectedLevelIdx >= 0 && m_selectedLevelIdx < static_cast<int>(levels.size()))
    {
        const auto& sel = levels[static_cast<size_t>(m_selectedLevelIdx)];

        if (playBtn.Contains(mouse))
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
            context.Scenes.LoadSceneWithTransition<CustomLevelPlayScene>(SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f}, sel.Id);
            return;
        }
        if (editBtn.Contains(mouse))
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
            context.Scenes.LoadSceneWithTransition<EditorScene>(SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f}, sel.Id, true);
            return;
        }
        if (renBtn.Contains(mouse))
        {
            OpenModal(ModalMode::RenameLevel, sel.Name);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return;
        }
        if (delBtn.Contains(mouse))
        {
            m_library.DeleteLevel(sel.Id);
            const int remaining = static_cast<int>(m_library.GetLevels().size());
            m_selectedLevelIdx = (remaining > 0) ? std::min(m_selectedLevelIdx, remaining - 1) : -1;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
            return;
        }
    }

    // Bottom pagination buttons (when > 1 page)
    if (totalPages > 1)
    {
        const float pageClusterX = vaultCenterX + totalRowW * 0.5f + 56.0f;
        const CardRect prevPageBtn{Vector2f{pageClusterX - 38.0f, actY}, Vector2f{36.0f, 36.0f}};
        const CardRect nextPageBtn{Vector2f{pageClusterX + 38.0f, actY}, Vector2f{36.0f, 36.0f}};
        if (prevPageBtn.Contains(mouse))
        {
            m_vaultPage = (m_vaultPage + totalPages - 1) % totalPages;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return;
        }
        if (nextPageBtn.Contains(mouse))
        {
            m_vaultPage = (m_vaultPage + 1) % totalPages;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return;
        }
    }

    // 6. Right Panel — Special Playlists (Create, Edit/Rename, Delete, Switch, Slots, Launch)
    const float panelCenterX = halfW - 22.0f - kRightPanelW * 0.5f;
    const float headerY = -halfH + kTopBarH + 40.0f;
    const float subBarY = -halfH + kTopBarH + 86.0f;
    const auto& playlists = m_library.GetPlaylists();

    if (!playlists.empty())
    {
        m_activePlaylistIdx = std::clamp(m_activePlaylistIdx, 0, static_cast<int>(playlists.size()) - 1);
        const CardRect prevPlBtn{Vector2f{panelCenterX - kRightPanelW * 0.5f + 36.0f, headerY}, Vector2f{36.0f, 34.0f}};
        const CardRect nextPlBtn{Vector2f{panelCenterX + kRightPanelW * 0.5f - 36.0f, headerY}, Vector2f{36.0f, 34.0f}};

        if (prevPlBtn.Contains(mouse))
        {
            m_activePlaylistIdx = (m_activePlaylistIdx + static_cast<int>(playlists.size()) - 1) % static_cast<int>(playlists.size());
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return;
        }
        if (nextPlBtn.Contains(mouse))
        {
            m_activePlaylistIdx = (m_activePlaylistIdx + 1) % static_cast<int>(playlists.size());
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return;
        }
    }

    // Playlist CRUD mini toolbar: [+ LISTE] [NOM] [SUPPR]
    const float pBtnW = (kRightPanelW - 52.0f) / 3.0f;
    const CardRect createPlBtn{Vector2f{panelCenterX - pBtnW - 8.0f, subBarY}, Vector2f{pBtnW, 34.0f}};
    const CardRect renamePlBtn{Vector2f{panelCenterX, subBarY}, Vector2f{pBtnW, 34.0f}};
    const CardRect deletePlBtn{Vector2f{panelCenterX + pBtnW + 8.0f, subBarY}, Vector2f{pBtnW, 34.0f}};

    if (createPlBtn.Contains(mouse))
    {
        OpenModal(ModalMode::CreatePlaylist, "LIST " + FormatTwoDigits(static_cast<int>(playlists.size()) + 1));
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return;
    }

    if (!playlists.empty() && m_activePlaylistIdx >= 0 && m_activePlaylistIdx < static_cast<int>(playlists.size()))
    {
        const auto& activePl = playlists[static_cast<size_t>(m_activePlaylistIdx)];

        if (renamePlBtn.Contains(mouse))
        {
            OpenModal(ModalMode::RenamePlaylist, activePl.Name);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return;
        }
        if (deletePlBtn.Contains(mouse))
        {
            m_library.DeletePlaylist(activePl.Id);
            const int remPl = static_cast<int>(m_library.GetPlaylists().size());
            m_activePlaylistIdx = (remPl > 0) ? std::min(m_activePlaylistIdx, remPl - 1) : -1;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
            return;
        }

        // 10 Slots interactions
        for (int s = 0; s < static_cast<int>(CustomPlaylist::MAX_SLOTS); ++s)
        {
            const CardRect slotRect = GetPlaylistSlotRect(s, viewSize);
            if (!slotRect.Contains(mouse)) continue;

            if (s < static_cast<int>(activePl.LevelIds.size()))
            {
                // Remove [X] button on right side of occupied slot
                const CardRect remSlotBtn{Vector2f{slotRect.Center.X + slotRect.Size.X * 0.5f - 22.0f, slotRect.Center.Y}, Vector2f{30.0f, slotRect.Size.Y - 4.0f}};
                if (remSlotBtn.Contains(mouse))
                {
                    m_library.RemoveSlotFromPlaylist(activePl.Id, static_cast<size_t>(s));
                    if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
                    return;
                }

                // Arm slot drag for reordering
                m_dragKind = DragKind::PlaylistSlot;
                m_dragSourceIdx = s;
                m_dragStartPos = mouse;
                m_dragCurrentPos = mouse;
                m_dragActive = false;
                return;
            }
            else if (m_selectedLevelIdx >= 0 && m_selectedLevelIdx < static_cast<int>(levels.size()))
            {
                // Quick-insert selected level when clicking an empty slot
                const std::string lvlId = levels[static_cast<size_t>(m_selectedLevelIdx)].Id;
                if (m_library.AddLevelToPlaylist(activePl.Id, lvlId))
                {
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                }
                return;
            }
        }

        // Bottom [JOUER LISTE] button
        const CardRect launchPlBtn{Vector2f{panelCenterX, halfH - 46.0f}, Vector2f{kRightPanelW - 36.0f, 50.0f}};
        if (launchPlBtn.Contains(mouse) && !activePl.LevelIds.empty())
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
            context.Scenes.LoadSceneWithTransition<CustomLevelPlayScene>(
                SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f},
                activePl.LevelIds, activePl.Name);
            return;
        }
    }
}

void RobostockScene::OnRender(GameContext& context)
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

    // Subtle industrial blueprint grid background
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    for (float gx = -halfW; gx <= halfW; gx += 64.0f)
    {
        context.Render.DrawLine(Vector2f{gx, -halfH}, Vector2f{gx, halfH}, Color{22, 30, 42, 110}, 1.0f);
    }
    for (float gy = -halfH; gy <= halfH; gy += 64.0f)
    {
        context.Render.DrawLine(Vector2f{-halfW, gy}, Vector2f{halfW, gy}, Color{22, 30, 42, 110}, 1.0f);
    }

    RenderTopBar(context, viewSize, mouse);
    RenderVaultGrid(context, viewSize, mouse);
    RenderPlaylistPanel(context, viewSize, mouse);

    if (m_dragActive)
    {
        RenderDragGhost(context);
    }

    if (m_modalMode != ModalMode::None)
    {
        RenderModal(context, viewSize, mouse);
    }
}

void RobostockScene::RenderTopBar(GameContext& context, const Vector2f& viewSize, const Vector2f& /*mouse*/)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float barY = -halfH + kTopBarH * 0.5f;

    context.Render.DrawRectangle(viewSize.X, kTopBarH, Transform2D{Vector2f{0.0f, barY}}, Color{18, 24, 34, 250});
    context.Render.DrawLine(Vector2f{-halfW, -halfH + kTopBarH}, Vector2f{halfW, -halfH + kTopBarH}, Color{65, 235, 195, 190}, 2.0f);

    // Centered Animated Strix Emblem + STRIX VAULT Title + Count Pill (no buttons in top bar)
    const float bobY = std::sin(m_animTimer * 2.4f) * 2.0f;
    DrawBotBadge(context, Vector2f{-134.0f, barY + 2.0f + bobY}, 38.0f);
    context.Render.DrawTextCentered("STRIX VAULT", m_fontId, 32.0f, Transform2D{Vector2f{0.0f, barY}}, Color{65, 235, 195, 255});

    const Vector2f countPill{Vector2f{128.0f, barY}};
    context.Render.DrawRectangle(58.0f, 30.0f, Transform2D{countPill}, Color{12, 18, 26, 255});
    context.Render.DrawRectangleOutline(58.0f, 30.0f, Transform2D{countPill}, Color{65, 235, 195, 180}, 1.4f);
    context.Render.DrawTextCentered(FormatTwoDigits(static_cast<int>(m_library.GetLevels().size())),
                                    m_fontId, 20.0f, Transform2D{countPill}, Color{225, 245, 255, 255});
}

void RobostockScene::RenderVaultGrid(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float vaultMinX = -halfW + 28.0f;
    const float vaultMaxX = halfW - kRightPanelW - 44.0f;
    const float vaultCenterX = (vaultMinX + vaultMaxX) * 0.5f;

    const auto& levels = m_library.GetLevels();
    if (levels.empty())
    {
        const Vector2f emptyBox{vaultCenterX, -20.0f};
        context.Render.DrawRectangle(360.0f, 190.0f, Transform2D{emptyBox}, Color{18, 24, 34, 230});
        context.Render.DrawRectangleOutline(360.0f, 190.0f, Transform2D{emptyBox}, Color{65, 235, 195, 120}, 1.8f);
        DrawBotBadge(context, Vector2f{vaultCenterX, -62.0f}, 48.0f);

        const CardRect emptyNewBtn{Vector2f{vaultCenterX, 24.0f}, Vector2f{210.0f, 54.0f}};
        const bool hov = emptyNewBtn.Contains(mouse) && (m_modalMode == ModalMode::None);
        context.Render.DrawRectangle(emptyNewBtn.Size.X, emptyNewBtn.Size.Y, Transform2D{emptyNewBtn.Center},
                                     hov ? Color{55, 175, 130, 255} : Color{38, 135, 100, 255});
        context.Render.DrawRectangleOutline(emptyNewBtn.Size.X, emptyNewBtn.Size.Y, Transform2D{emptyNewBtn.Center},
                                            hov ? Colors::White : Color{95, 240, 190, 230}, 1.8f);
        DrawIconPlus(context, Vector2f{emptyNewBtn.Center.X - 58.0f, emptyNewBtn.Center.Y}, 16.0f, Colors::White);
        context.Render.DrawTextCentered("NEW LEVEL", m_fontId, 23.0f, Transform2D{Vector2f{emptyNewBtn.Center.X + 14.0f, emptyNewBtn.Center.Y}}, Colors::White);
    }
    else
    {
        const int pageStart = m_vaultPage * kCardsPerPage;
        for (int vis = 0; vis < kCardsPerPage; ++vis)
        {
            const int idx = pageStart + vis;
            const CardRect card = GetVaultCardRect(vis, viewSize);

            if (idx >= static_cast<int>(levels.size()))
            {
                // Recessed empty cartridge bay
                context.Render.DrawRectangle(card.Size.X, card.Size.Y, Transform2D{card.Center}, Color{15, 20, 28, 155});
                context.Render.DrawRectangleOutline(card.Size.X, card.Size.Y, Transform2D{card.Center}, Color{36, 46, 62, 130}, 1.2f);
                continue;
            }

            const auto& entry = levels[static_cast<size_t>(idx)];
            const bool isSelected = (idx == m_selectedLevelIdx);
            const bool isHovered = card.Contains(mouse) && (m_modalMode == ModalMode::None);
            const bool isDropTarget = m_dragActive && m_dragKind == DragKind::LevelCard && isHovered && idx != m_dragSourceIdx;

            // Drop shadow
            context.Render.DrawRectangle(card.Size.X, card.Size.Y, Transform2D{card.Center + Vector2f{4.0f, 5.0f}}, Color{0, 0, 0, 135});

            // Cartridge shell
            const Color shellCol = isSelected ? Color{28, 42, 54, 255} : (isHovered ? Color{24, 34, 46, 255} : Color{19, 26, 36, 250});
            context.Render.DrawRectangle(card.Size.X, card.Size.Y, Transform2D{card.Center}, shellCol);

            Color borderCol = Color{52, 68, 88, 200};
            float borderThick = 1.5f;
            if (isDropTarget)
            {
                borderCol = Colors::Gold;
                borderThick = 3.2f;
            }
            else if (isSelected)
            {
                const uint8_t pulse = static_cast<uint8_t>(185 + 70 * (0.5f + 0.5f * std::sin(m_animTimer * 6.0f)));
                borderCol = Color{65, 235, 195, pulse};
                borderThick = 3.0f;
            }
            else if (isHovered)
            {
                borderCol = Color{110, 195, 235, 235};
                borderThick = 2.0f;
            }
            context.Render.DrawRectangleOutline(card.Size.X, card.Size.Y, Transform2D{card.Center}, borderCol, borderThick);

            // Top Cartridge ID Bar
            const float topStripH = 28.0f;
            const Vector2f topStripPos{card.Center.X, card.Center.Y - card.Size.Y * 0.5f + topStripH * 0.5f + 2.0f};
            context.Render.DrawRectangle(card.Size.X - 6.0f, topStripH, Transform2D{topStripPos}, Color{13, 18, 26, 255});
            context.Render.DrawText(entry.Id, m_fontId, 18.0f,
                                    Transform2D{Vector2f{card.Center.X - card.Size.X * 0.5f + 12.0f, topStripPos.Y - 10.0f}},
                                    Color{65, 235, 195, 255});

            if (entry.PageCount > 1)
            {
                const std::string pgBadge = std::to_string(entry.PageCount) + "P";
                const Vector2f pgPos{card.Center.X + card.Size.X * 0.5f - 26.0f, topStripPos.Y};
                context.Render.DrawRectangle(34.0f, 20.0f, Transform2D{pgPos}, Color{46, 36, 14, 255});
                context.Render.DrawRectangleOutline(34.0f, 20.0f, Transform2D{pgPos}, Colors::Gold, 1.2f);
                context.Render.DrawTextCentered(pgBadge, m_fontId, 15.0f, Transform2D{pgPos}, Colors::Gold);
            }

            // Center Miniature Brick Preview
            const float botStripH = 34.0f;
            const Vector2f previewSize{card.Size.X - 22.0f, std::max(54.0f, card.Size.Y - topStripH - botStripH - 18.0f)};
            const Vector2f previewCenter{card.Center.X, card.Center.Y + (topStripH - botStripH) * 0.5f};
            RenderMiniBrickPreview(context, entry, previewCenter, previewSize);

            // Bottom Cartridge Name Strip
            const Vector2f botStripPos{card.Center.X, card.Center.Y + card.Size.Y * 0.5f - botStripH * 0.5f - 2.0f};
            context.Render.DrawRectangle(card.Size.X - 6.0f, botStripH, Transform2D{botStripPos}, Color{12, 16, 24, 255});
            context.Render.DrawTextCentered(entry.Name, m_fontId, 21.0f, Transform2D{botStripPos},
                                            isSelected ? Colors::White : Color{215, 228, 242, 255});
        }
    }

    // Centralized Bottom Bar: [< TV] [+ CREATE] [PLAY] [EDIT] [RENAME] [DELETE]
    const float actY = halfH - 46.0f;
    const float btnW = 148.0f;
    const float gap = 12.0f;
    const int btnCount = 6;
    const float totalRowW = btnW * static_cast<float>(btnCount) + gap * static_cast<float>(btnCount - 1);
    const float startX = vaultCenterX - totalRowW * 0.5f + btnW * 0.5f;
    const bool hasSelection = (m_selectedLevelIdx >= 0 && m_selectedLevelIdx < static_cast<int>(levels.size()));

    struct ActBtnSpec
    {
        const char* Label;
        Color BaseBg;
        Color HoverBg;
        Color Rim;
        int IconType; // -1 = none, 0 = play, 1 = edit, 2 = rename, 3 = trash, 4 = plus
        bool Enabled;
    };
    const ActBtnSpec specs[6] = {
        {"< TV",   Color{28, 36, 50, 255},   Color{44, 62, 86, 255},   Color{80, 140, 190, 210},  -1, true},
        {"CREATE", Color{185, 130, 28, 255}, Color{235, 175, 45, 255}, Color{255, 225, 110, 230},  4, true},
        {"PLAY",   Color{34, 130, 74, 255},  Color{48, 172, 98, 255},  Color{95, 235, 145, 255},   0, hasSelection},
        {"EDIT",   Color{165, 115, 24, 255}, Color{215, 152, 34, 255}, Color{255, 215, 85, 255},   1, hasSelection},
        {"RENAME", Color{36, 78, 116, 255},  Color{50, 108, 158, 255}, Color{105, 195, 255, 255},  2, hasSelection},
        {"DELETE", Color{128, 34, 46, 255},  Color{176, 46, 62, 255},  Color{255, 105, 120, 255},  3, hasSelection}
    };

    for (int b = 0; b < btnCount; ++b)
    {
        const CardRect rect{Vector2f{startX + static_cast<float>(b) * (btnW + gap), actY}, Vector2f{btnW, 50.0f}};
        const bool enabled = specs[b].Enabled;
        const bool hov = enabled && rect.Contains(mouse) && (m_modalMode == ModalMode::None);
        const Color bg = enabled ? (hov ? specs[b].HoverBg : specs[b].BaseBg) : Color{22, 28, 38, 180};
        const Color rim = enabled ? (hov ? Colors::White : specs[b].Rim) : Color{48, 56, 70, 150};
        const Color fg = enabled ? Colors::White : Color{105, 115, 130, 190};

        context.Render.DrawRectangle(rect.Size.X, rect.Size.Y, Transform2D{rect.Center}, bg);
        context.Render.DrawRectangleOutline(rect.Size.X, rect.Size.Y, Transform2D{rect.Center}, rim, hov ? 2.2f : 1.5f);

        if (specs[b].IconType >= 0)
        {
            const Vector2f iconPos{rect.Center.X - 42.0f, rect.Center.Y};
            if (specs[b].IconType == 0) DrawIconPlay(context, iconPos, 15.0f, fg);
            else if (specs[b].IconType == 1) DrawIconEdit(context, iconPos, 15.0f, fg);
            else if (specs[b].IconType == 2) DrawIconRename(context, iconPos, 15.0f, fg);
            else if (specs[b].IconType == 3) DrawIconTrash(context, iconPos, 16.0f, fg);
            else if (specs[b].IconType == 4) DrawIconPlus(context, iconPos, 14.0f, fg);

            context.Render.DrawTextCentered(specs[b].Label, m_fontId, 20.0f,
                                            Transform2D{Vector2f{rect.Center.X + 12.0f, rect.Center.Y}}, fg);
        }
        else
        {
            context.Render.DrawTextCentered(specs[b].Label, m_fontId, 21.0f,
                                            Transform2D{rect.Center}, fg);
        }
    }

    const int totalPages = std::max(1, (static_cast<int>(levels.size()) + kCardsPerPage - 1) / kCardsPerPage);
    if (totalPages > 1)
    {
        const float pageClusterX = vaultCenterX + totalRowW * 0.5f + 56.0f;
        const CardRect prevPageBtn{Vector2f{pageClusterX - 38.0f, actY}, Vector2f{36.0f, 36.0f}};
        const CardRect nextPageBtn{Vector2f{pageClusterX + 38.0f, actY}, Vector2f{36.0f, 36.0f}};

        context.Render.DrawRectangle(36.0f, 36.0f, Transform2D{prevPageBtn.Center}, prevPageBtn.Contains(mouse) ? Color{45, 65, 88, 255} : Color{26, 34, 48, 255});
        context.Render.DrawRectangleOutline(36.0f, 36.0f, Transform2D{prevPageBtn.Center}, Color{65, 235, 195, 190}, 1.4f);
        context.Render.DrawTextCentered("<", m_fontId, 22.0f, Transform2D{prevPageBtn.Center}, Colors::White);

        const std::string pStr = std::to_string(m_vaultPage + 1) + "/" + std::to_string(totalPages);
        context.Render.DrawTextCentered(pStr, m_fontId, 18.0f, Transform2D{Vector2f{pageClusterX, actY}}, Color{200, 225, 240, 255});

        context.Render.DrawRectangle(36.0f, 36.0f, Transform2D{nextPageBtn.Center}, nextPageBtn.Contains(mouse) ? Color{45, 65, 88, 255} : Color{26, 34, 48, 255});
        context.Render.DrawRectangleOutline(36.0f, 36.0f, Transform2D{nextPageBtn.Center}, Color{65, 235, 195, 190}, 1.4f);
        context.Render.DrawTextCentered(">", m_fontId, 22.0f, Transform2D{nextPageBtn.Center}, Colors::White);
    }
}

void RobostockScene::RenderPlaylistPanel(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float panelCenterX = halfW - 22.0f - kRightPanelW * 0.5f;
    const float panelTop = -halfH + kTopBarH + 14.0f;
    const float panelH = viewSize.Y - kTopBarH - 28.0f;
    const Vector2f panelCenter{panelCenterX, panelTop + panelH * 0.5f};

    const bool draggingLevelOverPanel = m_dragActive && m_dragKind == DragKind::LevelCard &&
                                        std::abs(mouse.X - panelCenter.X) <= kRightPanelW * 0.5f &&
                                        std::abs(mouse.Y - panelCenter.Y) <= panelH * 0.5f;

    context.Render.DrawRectangle(kRightPanelW, panelH, Transform2D{panelCenter}, Color{17, 22, 32, 250});
    context.Render.DrawRectangleOutline(kRightPanelW, panelH, Transform2D{panelCenter},
                                        draggingLevelOverPanel ? Colors::Gold : Color{65, 235, 195, 185},
                                        draggingLevelOverPanel ? 2.8f : 1.8f);

    // 1. Header Selector: [<] PLAYLIST NAME (N/10) [>]
    const float headerY = -halfH + kTopBarH + 40.0f;
    context.Render.DrawRectangle(kRightPanelW - 12.0f, 42.0f, Transform2D{Vector2f{panelCenterX, headerY}}, Color{24, 32, 46, 255});

    const auto& playlists = m_library.GetPlaylists();
    const bool hasPl = !playlists.empty() && m_activePlaylistIdx >= 0 && m_activePlaylistIdx < static_cast<int>(playlists.size());

    if (hasPl)
    {
        const auto& pl = playlists[static_cast<size_t>(m_activePlaylistIdx)];
        const CardRect prevPlBtn{Vector2f{panelCenterX - kRightPanelW * 0.5f + 36.0f, headerY}, Vector2f{36.0f, 34.0f}};
        const CardRect nextPlBtn{Vector2f{panelCenterX + kRightPanelW * 0.5f - 36.0f, headerY}, Vector2f{36.0f, 34.0f}};

        context.Render.DrawRectangle(36.0f, 34.0f, Transform2D{prevPlBtn.Center}, prevPlBtn.Contains(mouse) ? Color{45, 65, 90, 255} : Color{18, 24, 36, 255});
        context.Render.DrawRectangleOutline(36.0f, 34.0f, Transform2D{prevPlBtn.Center}, Color{65, 235, 195, 180}, 1.2f);
        context.Render.DrawTextCentered("<", m_fontId, 22.0f, Transform2D{prevPlBtn.Center}, Colors::White);

        const std::string title = pl.Name + "  (" + std::to_string(pl.LevelIds.size()) + "/10)";
        context.Render.DrawTextCentered(title, m_fontId, 22.0f, Transform2D{Vector2f{panelCenterX, headerY}}, Color{255, 220, 85, 255});

        context.Render.DrawRectangle(36.0f, 34.0f, Transform2D{nextPlBtn.Center}, nextPlBtn.Contains(mouse) ? Color{45, 65, 90, 255} : Color{18, 24, 36, 255});
        context.Render.DrawRectangleOutline(36.0f, 34.0f, Transform2D{nextPlBtn.Center}, Color{65, 235, 195, 180}, 1.2f);
        context.Render.DrawTextCentered(">", m_fontId, 22.0f, Transform2D{nextPlBtn.Center}, Colors::White);
    }
    else
    {
        context.Render.DrawTextCentered("PLAYLIST (0/10)", m_fontId, 21.0f, Transform2D{Vector2f{panelCenterX, headerY}}, Color{165, 180, 200, 255});
    }

    // 2. Compact CRUD Row: [+ LIST] [NAME] [DEL]
    const float subBarY = -halfH + kTopBarH + 86.0f;
    const float pBtnW = (kRightPanelW - 52.0f) / 3.0f;
    const CardRect createPlBtn{Vector2f{panelCenterX - pBtnW - 8.0f, subBarY}, Vector2f{pBtnW, 34.0f}};
    const CardRect renamePlBtn{Vector2f{panelCenterX, subBarY}, Vector2f{pBtnW, 34.0f}};
    const CardRect deletePlBtn{Vector2f{panelCenterX + pBtnW + 8.0f, subBarY}, Vector2f{pBtnW, 34.0f}};

    const bool cHov = createPlBtn.Contains(mouse) && (m_modalMode == ModalMode::None);
    context.Render.DrawRectangle(createPlBtn.Size.X, createPlBtn.Size.Y, Transform2D{createPlBtn.Center}, cHov ? Color{42, 138, 96, 255} : Color{28, 96, 68, 255});
    context.Render.DrawRectangleOutline(createPlBtn.Size.X, createPlBtn.Size.Y, Transform2D{createPlBtn.Center}, cHov ? Colors::White : Color{85, 225, 165, 210}, 1.3f);
    DrawIconPlus(context, Vector2f{createPlBtn.Center.X - 38.0f, createPlBtn.Center.Y}, 12.0f, Colors::White);
    context.Render.DrawTextCentered("LIST", m_fontId, 18.0f, Transform2D{Vector2f{createPlBtn.Center.X + 10.0f, createPlBtn.Center.Y}}, Colors::White);

    const bool rHov = hasPl && renamePlBtn.Contains(mouse) && (m_modalMode == ModalMode::None);
    context.Render.DrawRectangle(renamePlBtn.Size.X, renamePlBtn.Size.Y, Transform2D{renamePlBtn.Center},
                                 hasPl ? (rHov ? Color{50, 98, 145, 255} : Color{32, 62, 94, 255}) : Color{24, 28, 36, 180});
    context.Render.DrawRectangleOutline(renamePlBtn.Size.X, renamePlBtn.Size.Y, Transform2D{renamePlBtn.Center},
                                        hasPl ? (rHov ? Colors::White : Color{95, 180, 245, 200}) : Color{50, 56, 68, 140}, 1.3f);
    DrawIconRename(context, Vector2f{renamePlBtn.Center.X - 32.0f, renamePlBtn.Center.Y}, 12.0f, hasPl ? Colors::White : Color{110, 120, 135, 200});
    context.Render.DrawTextCentered("NAME", m_fontId, 18.0f, Transform2D{Vector2f{renamePlBtn.Center.X + 10.0f, renamePlBtn.Center.Y}},
                                    hasPl ? Colors::White : Color{110, 120, 135, 200});

    const bool dHov = hasPl && deletePlBtn.Contains(mouse) && (m_modalMode == ModalMode::None);
    context.Render.DrawRectangle(deletePlBtn.Size.X, deletePlBtn.Size.Y, Transform2D{deletePlBtn.Center},
                                 hasPl ? (dHov ? Color{168, 44, 58, 255} : Color{112, 30, 42, 255}) : Color{24, 28, 36, 180});
    context.Render.DrawRectangleOutline(deletePlBtn.Size.X, deletePlBtn.Size.Y, Transform2D{deletePlBtn.Center},
                                        hasPl ? (dHov ? Colors::White : Color{245, 95, 110, 200}) : Color{50, 56, 68, 140}, 1.3f);
    DrawIconTrash(context, Vector2f{deletePlBtn.Center.X - 36.0f, deletePlBtn.Center.Y}, 13.0f, hasPl ? Colors::White : Color{110, 120, 135, 200});
    context.Render.DrawTextCentered("DEL", m_fontId, 18.0f, Transform2D{Vector2f{deletePlBtn.Center.X + 10.0f, deletePlBtn.Center.Y}},
                                    hasPl ? Colors::White : Color{110, 120, 135, 200});

    // 3. 10 Slots (01 .. 10)
    for (int s = 0; s < static_cast<int>(CustomPlaylist::MAX_SLOTS); ++s)
    {
        const CardRect slotRect = GetPlaylistSlotRect(s, viewSize);
        const bool slotHover = slotRect.Contains(mouse) && (m_modalMode == ModalMode::None);
        const bool occupied = hasPl && (s < static_cast<int>(playlists[static_cast<size_t>(m_activePlaylistIdx)].LevelIds.size()));

        if (occupied)
        {
            const std::string& lvlId = playlists[static_cast<size_t>(m_activePlaylistIdx)].LevelIds[static_cast<size_t>(s)];
            const CustomLevelEntry* entry = m_library.FindLevel(lvlId);

            const bool isReorderDrop = m_dragActive && m_dragKind == DragKind::PlaylistSlot && slotHover && s != m_dragSourceIdx;
            const Color bg = isReorderDrop ? Color{48, 66, 88, 255} : (slotHover ? Color{30, 42, 58, 255} : Color{23, 31, 44, 255});
            const Color rim = isReorderDrop ? Colors::Gold : Color{65, 235, 195, 190};

            context.Render.DrawRectangle(slotRect.Size.X, slotRect.Size.Y, Transform2D{slotRect.Center}, bg);
            context.Render.DrawRectangleOutline(slotRect.Size.X, slotRect.Size.Y, Transform2D{slotRect.Center}, rim, isReorderDrop ? 2.4f : 1.3f);

            // Slot index badge
            const Vector2f numPos{slotRect.Center.X - slotRect.Size.X * 0.5f + 24.0f, slotRect.Center.Y};
            context.Render.DrawRectangle(36.0f, slotRect.Size.Y - 6.0f, Transform2D{numPos}, Color{13, 18, 26, 255});
            context.Render.DrawTextCentered(FormatTwoDigits(s + 1), m_fontId, 18.0f, Transform2D{numPos}, Color{65, 235, 195, 255});

            // Level Name + ID
            const std::string label = entry ? (entry->Name + "   [" + entry->Id + "]") : lvlId;
            context.Render.DrawText(label, m_fontId, 19.0f,
                                    Transform2D{Vector2f{slotRect.Center.X - slotRect.Size.X * 0.5f + 52.0f, slotRect.Center.Y - 10.0f}},
                                    Colors::White);

            // Remove [X] button
            const CardRect remBtn{Vector2f{slotRect.Center.X + slotRect.Size.X * 0.5f - 22.0f, slotRect.Center.Y}, Vector2f{30.0f, slotRect.Size.Y - 6.0f}};
            const bool remHov = remBtn.Contains(mouse) && !m_dragActive;
            context.Render.DrawRectangle(remBtn.Size.X, remBtn.Size.Y, Transform2D{remBtn.Center}, remHov ? Color{185, 45, 60, 255} : Color{48, 26, 34, 255});
            context.Render.DrawTextCentered("X", m_fontId, 18.0f, Transform2D{remBtn.Center}, remHov ? Colors::White : Color{255, 120, 135, 240});
        }
        else
        {
            const bool dropHighlight = draggingLevelOverPanel && (!hasPl || playlists[static_cast<size_t>(m_activePlaylistIdx)].LevelIds.size() < CustomPlaylist::MAX_SLOTS);
            context.Render.DrawRectangle(slotRect.Size.X, slotRect.Size.Y, Transform2D{slotRect.Center},
                                         dropHighlight ? Color{28, 38, 34, 235} : Color{13, 17, 24, 195});
            context.Render.DrawRectangleOutline(slotRect.Size.X, slotRect.Size.Y, Transform2D{slotRect.Center},
                                                dropHighlight ? Color{255, 215, 70, 210} : (slotHover ? Color{70, 95, 125, 190} : Color{38, 48, 64, 150}), 1.2f);

            const Vector2f numPos{slotRect.Center.X - slotRect.Size.X * 0.5f + 24.0f, slotRect.Center.Y};
            context.Render.DrawTextCentered(FormatTwoDigits(s + 1), m_fontId, 17.0f, Transform2D{numPos}, Color{85, 100, 122, 210});
            context.Render.DrawTextCentered(slotHover && m_selectedLevelIdx >= 0 ? "+" : "- - -", m_fontId, 18.0f,
                                            Transform2D{slotRect.Center},
                                            slotHover && m_selectedLevelIdx >= 0 ? Color{65, 235, 195, 230} : Color{68, 80, 98, 180});
        }
    }

    // 4. Bottom [PLAY LIST] Button
    const bool canLaunch = hasPl && !playlists[static_cast<size_t>(m_activePlaylistIdx)].LevelIds.empty();
    const CardRect launchBtn{Vector2f{panelCenterX, halfH - 46.0f}, Vector2f{kRightPanelW - 36.0f, 50.0f}};
    const bool lHov = canLaunch && launchBtn.Contains(mouse) && (m_modalMode == ModalMode::None);

    context.Render.DrawRectangle(launchBtn.Size.X, launchBtn.Size.Y, Transform2D{launchBtn.Center},
                                 canLaunch ? (lHov ? Color{45, 185, 140, 255} : Color{32, 142, 108, 255}) : Color{26, 32, 42, 200});
    context.Render.DrawRectangleOutline(launchBtn.Size.X, launchBtn.Size.Y, Transform2D{launchBtn.Center},
                                        canLaunch ? (lHov ? Colors::White : Color{95, 250, 205, 240}) : Color{52, 62, 78, 160},
                                        lHov ? 2.4f : 1.6f);
    DrawIconPlay(context, Vector2f{launchBtn.Center.X - 78.0f, launchBtn.Center.Y}, 16.0f, canLaunch ? Colors::White : Color{105, 118, 136, 200});
    context.Render.DrawTextCentered("PLAY LIST", m_fontId, 23.0f, Transform2D{Vector2f{launchBtn.Center.X + 14.0f, launchBtn.Center.Y}},
                                    canLaunch ? Colors::White : Color{105, 118, 136, 200});
}

void RobostockScene::RenderMiniBrickPreview(GameContext& context, const CustomLevelEntry& entry, const Vector2f& center, const Vector2f& size) const
{
    context.Render.DrawRectangle(size.X, size.Y, Transform2D{center}, entry.BgColor);
    context.Render.DrawRectangleOutline(size.X, size.Y, Transform2D{center}, Color{46, 62, 84, 210}, 1.2f);

    const float padX = 8.0f;
    const float padY = 6.0f;
    const float innerW = std::max(24.0f, size.X - padX * 2.0f);
    const float innerH = std::max(20.0f, size.Y - padY * 2.0f);

    const float cellW = innerW / static_cast<float>(LevelData::GRID_COLS);
    const float cellH = innerH / static_cast<float>(LevelData::GRID_ROWS);
    const float bw = std::max(2.0f, cellW - 2.0f);
    const float bh = std::max(2.0f, cellH - 1.8f);
    const float minX = center.X - innerW * 0.5f;
    const float minY = center.Y - innerH * 0.5f;

    for (const auto& pb : entry.PreviewBricks)
    {
        if (pb.Col < 0 || pb.Col >= LevelData::GRID_COLS || pb.Row < 0 || pb.Row >= LevelData::GRID_ROWS)
        {
            continue;
        }
        const Vector2f bPos{
            minX + (static_cast<float>(pb.Col) + 0.5f) * cellW,
            minY + (static_cast<float>(pb.Row) + 0.5f) * cellH
        };
        context.Render.DrawRectangle(bw, bh, Transform2D{bPos}, pb.Tint);
    }
}

void RobostockScene::RenderDragGhost(GameContext& context)
{
    std::string label;
    if (m_dragKind == DragKind::LevelCard)
    {
        const auto& levels = m_library.GetLevels();
        if (m_dragSourceIdx >= 0 && m_dragSourceIdx < static_cast<int>(levels.size()))
        {
            const auto& e = levels[static_cast<size_t>(m_dragSourceIdx)];
            label = e.Id + "  " + e.Name;
        }
    }
    else if (m_dragKind == DragKind::PlaylistSlot)
    {
        const auto& playlists = m_library.GetPlaylists();
        if (m_activePlaylistIdx >= 0 && m_activePlaylistIdx < static_cast<int>(playlists.size()))
        {
            const auto& pl = playlists[static_cast<size_t>(m_activePlaylistIdx)];
            if (m_dragSourceIdx >= 0 && m_dragSourceIdx < static_cast<int>(pl.LevelIds.size()))
            {
                const std::string& id = pl.LevelIds[static_cast<size_t>(m_dragSourceIdx)];
                const auto* e = m_library.FindLevel(id);
                label = FormatTwoDigits(m_dragSourceIdx + 1) + "  " + (e ? e->Name : id);
            }
        }
    }

    if (label.empty()) return;

    const Vector2f ghostSize{240.0f, 44.0f};
    context.Render.DrawRectangle(ghostSize.X, ghostSize.Y, Transform2D{m_dragCurrentPos + Vector2f{4.0f, 5.0f}}, Color{0, 0, 0, 150});
    context.Render.DrawRectangle(ghostSize.X, ghostSize.Y, Transform2D{m_dragCurrentPos}, Color{24, 38, 48, 235});
    context.Render.DrawRectangleOutline(ghostSize.X, ghostSize.Y, Transform2D{m_dragCurrentPos}, Colors::Gold, 2.2f);
    context.Render.DrawTextCentered(label, m_fontId, 20.0f, Transform2D{m_dragCurrentPos}, Colors::White);
}

void RobostockScene::RenderModal(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse)
{
    context.Render.DrawRectangle(viewSize.X * 1.5f, viewSize.Y * 1.5f, Transform2D{Vector2f{0.0f, 0.0f}}, Color{6, 9, 14, 215});

    const Vector2f boxSz{460.0f, 230.0f};
    context.Render.DrawRectangle(boxSz.X, boxSz.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{22, 28, 40, 255});
    context.Render.DrawRectangleOutline(boxSz.X, boxSz.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{65, 235, 195, 255}, 2.0f);

    const char* title = "NAME";
    if (m_modalMode == ModalMode::RenameLevel) title = "LEVEL NAME";
    else if (m_modalMode == ModalMode::CreatePlaylist) title = "NEW PLAYLIST";
    else if (m_modalMode == ModalMode::RenamePlaylist) title = "PLAYLIST NAME";

    context.Render.DrawRectangle(boxSz.X - 4.0f, 42.0f, Transform2D{Vector2f{0.0f, -92.0f}}, Color{30, 40, 56, 255});
    context.Render.DrawTextCentered(title, m_fontId, 22.0f, Transform2D{Vector2f{0.0f, -92.0f}}, Color{255, 220, 85, 255});

    // Input field
    context.Render.DrawRectangle(400.0f, 52.0f, Transform2D{Vector2f{0.0f, -8.0f}}, Color{12, 16, 24, 255});
    context.Render.DrawRectangleOutline(400.0f, 52.0f, Transform2D{Vector2f{0.0f, -8.0f}}, Color{65, 235, 195, 230}, 1.8f);

    const bool blink = (std::fmod(m_animTimer * 2.2f, 2.0f) < 1.0f);
    const std::string shown = m_modalTextBuffer + (blink ? "_" : " ");
    context.Render.DrawTextCentered(shown, m_fontId, 25.0f, Transform2D{Vector2f{0.0f, -8.0f}}, Colors::White);

    const CardRect okBtn{Vector2f{-105.0f, 76.0f}, Vector2f{180.0f, 46.0f}};
    const CardRect cancelBtn{Vector2f{105.0f, 76.0f}, Vector2f{180.0f, 46.0f}};
    const bool okHov = okBtn.Contains(mouse);
    const bool caHov = cancelBtn.Contains(mouse);

    context.Render.DrawRectangle(okBtn.Size.X, okBtn.Size.Y, Transform2D{okBtn.Center}, okHov ? Color{52, 172, 102, 255} : Color{38, 132, 78, 255});
    context.Render.DrawRectangleOutline(okBtn.Size.X, okBtn.Size.Y, Transform2D{okBtn.Center}, okHov ? Colors::White : Color{95, 230, 145, 255}, 1.5f);
    context.Render.DrawTextCentered("OK", m_fontId, 21.0f, Transform2D{okBtn.Center}, Colors::White);

    context.Render.DrawRectangle(cancelBtn.Size.X, cancelBtn.Size.Y, Transform2D{cancelBtn.Center}, caHov ? Color{82, 48, 58, 255} : Color{46, 52, 66, 255});
    context.Render.DrawRectangleOutline(cancelBtn.Size.X, cancelBtn.Size.Y, Transform2D{cancelBtn.Center}, caHov ? Colors::White : Color{95, 105, 125, 255}, 1.5f);
    context.Render.DrawTextCentered("CANCEL", m_fontId, 21.0f, Transform2D{cancelBtn.Center}, Color{225, 232, 242, 255});
}

void RobostockScene::DrawIconPlay(GameContext& context, const Vector2f& pos, float size, Color color) const
{
    const float r = size * 0.55f;
    std::vector<Vertex> tri = {
        Vertex{pos.X - r * 0.7f, pos.Y - r, color, 0.0f, 0.0f},
        Vertex{pos.X - r * 0.7f, pos.Y + r, color, 0.0f, 0.0f},
        Vertex{pos.X + r, pos.Y, color, 0.0f, 0.0f}
    };
    context.Render.DrawVertices(tri, PrimitiveType::Triangles);
}

void RobostockScene::DrawIconEdit(GameContext& context, const Vector2f& pos, float size, Color color) const
{
    context.Render.DrawRectangle(size * 0.95f, size * 0.34f, Transform2D{pos, -0.7854f}, color);
    context.Render.DrawCircle(size * 0.16f, Transform2D{pos + Vector2f{-size * 0.36f, size * 0.36f}}, Colors::Gold);
}

void RobostockScene::DrawIconTrash(GameContext& context, const Vector2f& pos, float size, Color color) const
{
    const float w = size * 0.72f;
    const float h = size * 0.78f;
    context.Render.DrawRectangle(w * 1.18f, size * 0.16f, Transform2D{pos + Vector2f{0.0f, -h * 0.52f}}, color);
    context.Render.DrawRectangleOutline(w, h, Transform2D{pos + Vector2f{0.0f, size * 0.08f}}, color, 1.6f);
    context.Render.DrawLine(pos + Vector2f{-w * 0.18f, -h * 0.2f}, pos + Vector2f{-w * 0.18f, h * 0.36f}, color, 1.4f);
    context.Render.DrawLine(pos + Vector2f{w * 0.18f, -h * 0.2f}, pos + Vector2f{w * 0.18f, h * 0.36f}, color, 1.4f);
}

void RobostockScene::DrawIconPlus(GameContext& context, const Vector2f& pos, float size, Color color) const
{
    context.Render.DrawRectangle(size, size * 0.24f, Transform2D{pos}, color);
    context.Render.DrawRectangle(size * 0.24f, size, Transform2D{pos}, color);
}

void RobostockScene::DrawIconRename(GameContext& context, const Vector2f& pos, float size, Color color) const
{
    context.Render.DrawRectangleOutline(size * 1.15f, size * 0.72f, Transform2D{pos}, color, 1.5f);
    context.Render.DrawLine(pos + Vector2f{-size * 0.28f, size * 0.14f}, pos + Vector2f{size * 0.28f, size * 0.14f}, color, 1.5f);
}

void RobostockScene::DrawBotBadge(GameContext& context, const Vector2f& pos, float size) const
{
    // Strix — the Neon Archivist Owl emblem (matches the CH 06 TV preview)
    const float sc = size / 36.0f;
    const Color teal{65, 235, 195, 255};
    const Color gold{255, 210, 70, 255};

    // Crescent ear tufts
    for (int side = -1; side <= 1; side += 2)
    {
        const float d = static_cast<float>(side);
        std::vector<Vertex> tuft = {
            Vertex{pos.X + d * 4.0f * sc, pos.Y - 9.0f * sc, Color{24, 54, 66, 255}, 0.0f, 0.0f},
            Vertex{pos.X + d * 13.0f * sc, pos.Y - 19.0f * sc, teal, 0.0f, 0.0f},
            Vertex{pos.X + d * 11.0f * sc, pos.Y - 4.0f * sc, Color{30, 72, 84, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(tuft, PrimitiveType::Triangles);
    }

    // Head disc
    context.Render.DrawCircle(13.0f * sc, Transform2D{pos}, Color{22, 48, 60, 255});
    context.Render.DrawCircleOutline(13.0f * sc, Transform2D{pos}, teal, 1.6f * sc);

    // Blinking amber eyes with cyan pupils
    const bool blink = std::fmod(m_animTimer, 4.2f) > 3.98f;
    for (int side = -1; side <= 1; side += 2)
    {
        const Vector2f eye = pos + Vector2f{static_cast<float>(side) * 5.2f * sc, -1.0f * sc};
        if (blink)
        {
            context.Render.DrawLine(eye - Vector2f{3.2f * sc, 0.0f}, eye + Vector2f{3.2f * sc, 0.0f}, gold, 1.4f * sc);
        }
        else
        {
            context.Render.DrawCircle(3.6f * sc, Transform2D{eye}, gold);
            context.Render.DrawCircle(1.6f * sc, Transform2D{eye}, Color{75, 250, 220, 255});
        }
    }

    // Golden beak
    std::vector<Vertex> beak = {
        Vertex{pos.X - 2.4f * sc, pos.Y + 3.0f * sc, Color{255, 225, 110, 255}, 0.0f, 0.0f},
        Vertex{pos.X + 2.4f * sc, pos.Y + 3.0f * sc, Color{255, 225, 110, 255}, 0.0f, 0.0f},
        Vertex{pos.X, pos.Y + 8.0f * sc, Color{235, 155, 30, 255}, 0.0f, 0.0f}
    };
    context.Render.DrawVertices(beak, PrimitiveType::Triangles);
}
