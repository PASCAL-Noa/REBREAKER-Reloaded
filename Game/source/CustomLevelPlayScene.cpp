#include "Scenes/CustomLevelPlayScene.h"
#include "Scenes/RobostockScene.h"
#include "Factories/BrickFactory.h"
#include "Resources/ResourceManager.h"
#include "Core/GameContext.h"
#include "Core/GameRules.h"
#include "Core/ProfileManager.h"
#include "Core/SceneManager.h"
#include "ECS/Systems/DynamicEventSystem.h"
#include "ECS/Systems/PaddleSystem.h"
#include "ECS/Systems/BallSystem.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include "TweenEffects/TweenEffects.h"
#include "AudioMixer.h"
#include <algorithm>
#include <cmath>

CustomLevelPlayScene::CustomLevelPlayScene(std::string singleLevelId)
    : m_isPlaylist(false)
{
    if (!singleLevelId.empty())
    {
        m_levelIds.push_back(std::move(singleLevelId));
    }
}

CustomLevelPlayScene::CustomLevelPlayScene(std::vector<std::string> playlistLevelIds, std::string playlistName)
    : m_levelIds(std::move(playlistLevelIds))
    , m_playlistName(std::move(playlistName))
    , m_isPlaylist(true)
{
    if (m_levelIds.size() > CustomPlaylist::MAX_SLOTS)
    {
        m_levelIds.resize(CustomPlaylist::MAX_SLOTS);
    }
}

void CustomLevelPlayScene::OnInit(GameContext& context)
{
    GameScene::OnInit(context);
    m_playlist.Clear();

    m_bossLaserSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_laser.wav");
    m_paddleCrackSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_hit.wav");

    if (m_uiController)
    {
        m_uiController->SetOnQuitOverride([this]()
        {
            if (mp_context)
            {
                ReturnToRobostock(*mp_context);
            }
        });
    }

    if (m_levelIds.empty())
    {
        ReturnToRobostock(context);
        return;
    }

    m_currentSlot = 0;
    LoadSlotLevel(0, context, false);
    StartCountdown();
    if (m_uiController)
    {
        m_uiController->ShowState(GameUIState::Countdown);
    }
}

bool CustomLevelPlayScene::IsGameOver() const
{
    if (m_isPaddleDestroyed)
    {
        return m_paddleDestroyedTimer <= 0.0f;
    }
    return GameScene::IsGameOver();
}

bool CustomLevelPlayScene::IsLevelCleared() const
{
    if (IsCountdownActive() || m_isPaddleDestroyed)
    {
        return false;
    }
    if (m_brickCount > 0)
    {
        return false;
    }
    // Only cleared when no playable page remains on the current level
    return !FindNextPlayablePage(m_activePageIndex + 1).has_value();
}

bool CustomLevelPlayScene::HasNextLevel() const
{
    // Single levels NEVER chain; only Special Playlists chain their slots
    return m_isPlaylist && (m_currentSlot + 1 < m_levelIds.size());
}

void CustomLevelPlayScene::LoadLevel(int levelIndex, bool preserveStats)
{
    if (!mp_context || m_levelIds.empty())
    {
        return;
    }
    const size_t clamped = static_cast<size_t>(std::clamp(levelIndex, 0, static_cast<int>(m_levelIds.size()) - 1));
    m_currentSlot = clamped;
    LoadSlotLevel(clamped, *mp_context, preserveStats);
}

void CustomLevelPlayScene::AdvanceToNextLevel()
{
    if (!mp_context || !HasNextLevel())
    {
        return;
    }
    ++m_currentSlot;
    LoadSlotLevel(m_currentSlot, *mp_context, true);
}

void CustomLevelPlayScene::OnVictoryEnter()
{
    GameScene::OnVictoryEnter();
    m_victoryReturnActive = true;
    m_victoryReturnTimer = 1.65f;
}

void CustomLevelPlayScene::FullReset()
{
    if (!mp_context || m_levelIds.empty())
    {
        return;
    }

    m_victoryReturnActive = false;
    m_victoryReturnTimer = 0.0f;
    m_currentSlot = 0;
    m_scoreManager.Reset();
    m_lives = 3;
    mp_context->Events.Publish(LivesChangedEvent(m_lives));

    LoadSlotLevel(0, *mp_context, false);
    StartCountdown();
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Countdown));
    }
    if (m_uiController)
    {
        m_uiController->ShowState(GameUIState::Countdown);
    }
}

void CustomLevelPlayScene::OnReplayRequested()
{
    FullReset();
}

void CustomLevelPlayScene::RestorePostUltimateMusic()
{
    if (!mp_context || !m_musicEnabled)
    {
        return;
    }
    mp_context->Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
}

std::optional<size_t> CustomLevelPlayScene::FindNextPlayablePage(size_t startIndex) const
{
    for (size_t i = startIndex; i < m_levelData.GetPageCount(); ++i)
    {
        if (m_levelData.GetBreakableBrickCount(i) > 0)
        {
            return i;
        }
    }
    return std::nullopt;
}

void CustomLevelPlayScene::LoadSlotLevel(size_t slotIndex, GameContext& context, bool preserveStats)
{
    if (slotIndex >= m_levelIds.size())
    {
        return;
    }

    m_isPaddleDestroyed = false;
    m_paddleDestroyedTimer = 0.0f;
    m_paddleDamageFlashTimer = 0.0f;
    m_sparkEmissionTimer = 0.0f;
    m_levelElapsed = 0.0f;
    m_nextQueuedEventIdx = 0;

    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.05f);

    if (!m_library.LoadLevelData(m_levelIds[slotIndex], m_levelData))
    {
        m_levelData.ClearGrid();
        m_levelData.ProceduralGenerate(24, 1, true, static_cast<unsigned int>(slotIndex + 1));
    }

    const bool manualOverride = context.Rules.GetRule(Rule::Gameplay::ManualOverride);
    const auto& rules = m_levelData.GetGamerules();
    if (!manualOverride)
    {
        context.Rules.SetRule(Rule::Gameplay::HazardZonesEnabled, rules.HazardZones);
        context.Rules.SetRule(Rule::Gameplay::ArenaEventsEnabled, rules.ArenaEvents);
        context.Rules.SetRule(Rule::Gameplay::InfiniteLives, rules.InfiniteLives);
    }

    ApplyCustomTheme(context);

    if (mp_eventSystem)
    {
        const bool eventsOn = manualOverride
            ? context.Rules.GetRule(Rule::Gameplay::ArenaEventsEnabled)
            : rules.ArenaEvents;
        mp_eventSystem->SetEnabled(eventsOn);
        mp_eventSystem->SetIntervalRange(18.0f, 30.0f);
        mp_eventSystem->ResetLifecycle();
    }

    m_activePageIndex = FindNextPlayablePage(0).value_or(0);
    RebuildPlayfieldFromPage(m_activePageIndex, context, true);
    m_pageBannerTimer = (m_levelData.GetPageCount() > 1 || m_isPlaylist) ? 2.0f : 0.0f;

    if (!preserveStats)
    {
        m_lives = 3;
        context.Events.Publish(LivesChangedEvent(m_lives));
    }

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(true);
    }

    ResetBallAndPaddle();

    if (m_musicEnabled)
    {
        context.Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
    }
}

void CustomLevelPlayScene::ApplyCustomTheme(GameContext& context)
{
    (void)context;
    const auto& thm = m_levelData.GetTheme();
    m_bgColor = thm.BgColor;
    m_currentTheme.BgBase1 = thm.BgColor;
    m_currentTheme.BgBase2 = thm.BgColor;
    const float mult = std::clamp(thm.AmbientIntensity * 2.0f, 0.25f, 1.0f);
    m_currentTheme.AmbientLight = Color{
        static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.r) * mult, 20.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.g) * mult, 20.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.b) * mult, 20.0f, 255.0f)),
        255
    };
    m_ambientBackground.SetTheme(m_currentTheme);

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
    {
        auto& light = m_registry.GetComponent<LightSourceComponent>(m_paddle);
        light.LightColor = thm.PaddleConeColor;
        light.ConeAngleDeg = thm.PaddleConeAngle;
    }
}

void CustomLevelPlayScene::RebuildPlayfieldFromPage(size_t pageIdx, GameContext& context, bool withDescent)
{
    (void)context;
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&)
    {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    m_turretBricks.clear();
    m_turretBullets.clear();
    m_brickCount = 0;

    for (int r = 0; r < LevelData::GRID_ROWS; ++r)
    {
        for (int c = 0; c < LevelData::GRID_COLS; ++c)
        {
            const EditorCellData cell = m_levelData.GetCell(c, r, pageIdx);
            if (cell.Type == EditorTileType::Empty)
            {
                continue;
            }

            const Vector2f pos = m_levelData.GetCellCenter(c, r);
            BrickType bType = BrickType::Light;
            bool isSpecial = false;

            switch (cell.Type)
            {
            case EditorTileType::Light:   bType = BrickType::Light; break;
            case EditorTileType::Medium:  bType = BrickType::Medium; break;
            case EditorTileType::Hard:    bType = BrickType::Hard; break;
            case EditorTileType::Special: bType = BrickType::Special; isSpecial = true; break;
            case EditorTileType::Metal:   bType = BrickType::Metal; break;
            case EditorTileType::Bomb:    bType = BrickType::Bomb; break;
            case EditorTileType::Turret:  bType = BrickType::Hard; break;
            case EditorTileType::Lantern: bType = BrickType::Lantern; break;
            default: break;
            }

            Entity bEntity = BrickFactory::Create(m_registry, pos.X, pos.Y, bType, isSpecial, m_brickTexId, 0, cell.HitPoints);

            if (cell.Type == EditorTileType::Special && !cell.RandomPowerUp && m_registry.HasComponent<BrickComponent>(bEntity))
            {
                auto& bComp = m_registry.GetComponent<BrickComponent>(bEntity);
                bComp.HasForcedPowerUp = true;
                bComp.ForcedPowerUp = cell.SpecificPowerUp;
            }

            if (bType != BrickType::Metal && bType != BrickType::Special)
            {
                ++m_brickCount;
            }

            if (cell.Type == EditorTileType::Turret)
            {
                if (m_registry.HasComponent<SpriteComponent>(bEntity))
                {
                    m_registry.GetComponent<SpriteComponent>(bEntity).Tint =
                        (cell.HitPoints > 3) ? BrickFactory::GetColorForHitPoints(cell.HitPoints) : Color{255, 45, 55, 255};
                }
                m_turretBricks.push_back(TurretBrick{
                    .EntityId = bEntity,
                    .ShootTimer = cell.TurretInitialDelay + static_cast<float>(m_turretBricks.size()) * 0.35f,
                    .Cooldown = cell.TurretCooldown,
                    .ProjectileSpeed = cell.TurretProjectileSpeed,
                    .BaseHP = cell.HitPoints
                });
            }

            if (withDescent)
            {
                BeginBrickDescent(bEntity, pos.Y, 340.0f, 360.0f);
            }
        }
    }
}

void CustomLevelPlayScene::OnUpdate(float dt, GameContext& context)
{
    GameScene::OnUpdate(dt, context);

    // Auto-return to Robostock once single level or Special Playlist completes
    if (m_victoryReturnActive)
    {
        m_victoryReturnTimer -= dt;
        const bool skipNow = context.Input.IsKeyPress(KeyCode::Space) ||
                             context.Input.IsKeyPress(KeyCode::Enter) ||
                             context.Input.IsKeyPress(KeyCode::Escape);
        if (m_victoryReturnTimer <= 0.0f || skipNow)
        {
            m_victoryReturnActive = false;
            ReturnToRobostock(context);
            return;
        }
    }

    if (m_pageBannerTimer > 0.0f)
    {
        m_pageBannerTimer = std::max(0.0f, m_pageBannerTimer - dt);
    }

    const int curState = mp_state_machine ? mp_state_machine->GetCurrentState() : -1;
    if (curState != static_cast<int>(SceneState::Playing))
    {
        // Strictly freeze turrets, projectiles, scheduled events, and timers while paused / in transition
        return;
    }

    // Multi-page progression within the current custom level
    if (!IsCountdownActive() && !m_isPaddleDestroyed && m_brickCount <= 0)
    {
        if (auto nextPage = FindNextPlayablePage(m_activePageIndex + 1); nextPage.has_value())
        {
            m_activePageIndex = *nextPage;
            RebuildPlayfieldFromPage(m_activePageIndex, context, true);
            m_pageBannerTimer = 2.0f;
            if (m_audioEnabled && m_levelClearSfxId != 0)
            {
                context.Audio.PlaySfx(m_levelClearSfxId, 65.0f, 1.1f);
            }
        }
    }

    // Scheduled EventQueue execution (with alarm!)
    const auto& queue = m_levelData.GetEventQueue();
    if (mp_eventSystem && m_nextQueuedEventIdx < queue.size() && !IsCountdownActive() && !m_isPaddleDestroyed)
    {
        m_levelElapsed += dt;
        const auto& nextEv = queue[m_nextQueuedEventIdx];
        if (m_levelElapsed >= nextEv.TriggerDelay && !mp_eventSystem->IsEventActive())
        {
            mp_eventSystem->TriggerEvent(LevelData::MapEditorEventToArenaEvent(nextEv.Type), false);
            m_levelElapsed = 0.0f;
            ++m_nextQueuedEventIdx;
        }
    }

    // Paddle destruction death sequence
    if (m_isPaddleDestroyed)
    {
        m_paddleDestroyedTimer -= dt;
        return;
    }

    // Paddle red hit flash recovery
    if (m_paddleDamageFlashTimer > 0.0f)
    {
        m_paddleDamageFlashTimer = std::max(0.0f, m_paddleDamageFlashTimer - dt);
        if (m_paddleDamageFlashTimer <= 0.0f && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
        {
            m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
        }
    }

    // Critical 1-HP paddle sparks
    if (m_lives == 1 && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        m_sparkEmissionTimer -= dt;
        if (m_sparkEmissionTimer <= 0.0f)
        {
            m_sparkEmissionTimer = 0.14f;
            const Vector2f pPos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
            SpawnExplosionParticles(pPos, Color{255, 190, 60, 255}, 4);
        }
    }

    UpdateTurrets(dt, context);
}

void CustomLevelPlayScene::OnRender(GameContext& context)
{
    GameScene::OnRender(context);
}

void CustomLevelPlayScene::RenderSceneSpecificHUD(GameContext& context)
{
    // World-space overlays (turrets, triangle missiles, paddle cracks)
    if (m_registry.IsAlive(m_camera) && m_registry.HasComponent<Camera2D>(m_camera))
    {
        context.Render.SetCamera(m_registry.GetComponent<Camera2D>(m_camera));
    }
    RenderTurrets(context);
    RenderPaddleDurabilityOverlay(context);

    // Screen-space concise level / playlist badge
    Camera2D uiCam{};
    uiCam.Position = Vector2f{0.0f, 0.0f};
    uiCam.Zoom = 1.0f;
    uiCam.Rotation = 0.0f;
    context.Render.SetCamera(uiCam);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float topY = -viewSize.Y * 0.5f + 26.0f;

    std::string header;
    if (m_isPlaylist)
    {
        header = std::to_string(m_currentSlot + 1) + "/" + std::to_string(m_levelIds.size()) + "  " + m_levelData.GetLevelName();
    }
    else
    {
        const std::string idStr = !m_levelIds.empty() ? m_levelIds[0] : m_levelData.GetLevelId();
        header = idStr + "  " + m_levelData.GetLevelName();
    }

    if (m_levelData.GetPageCount() > 1)
    {
        header += "  [P" + std::to_string(m_activePageIndex + 1) + "/" + std::to_string(m_levelData.GetPageCount()) + "]";
    }

    const Vector2f badgePos{0.0f, topY};
    context.Render.DrawRectangle(340.0f, 30.0f, Transform2D{badgePos}, Color{14, 18, 28, 195});
    context.Render.DrawRectangleOutline(340.0f, 30.0f, Transform2D{badgePos}, Color{80, 220, 255, 170}, 1.5f);
    context.Render.DrawTextCentered(header, m_fontId, 19.0f, Transform2D{badgePos}, Color{225, 245, 255, 245});
}

void CustomLevelPlayScene::UpdateTurrets(float dt, GameContext& context)
{
    if (IsCountdownActive())
    {
        return;
    }

    for (auto it = m_turretBricks.begin(); it != m_turretBricks.end(); )
    {
        if (!m_registry.IsAlive(it->EntityId) || !m_registry.HasComponent<BrickComponent>(it->EntityId) ||
            m_registry.GetComponent<BrickComponent>(it->EntityId).HitPoints == 0)
        {
            it = m_turretBricks.erase(it);
            continue;
        }

        const auto& brick = m_registry.GetComponent<BrickComponent>(it->EntityId);
        if (brick.IsDescending)
        {
            ++it;
            continue;
        }

        it->ShootTimer -= dt;

        if (it->ShootTimer <= 0.45f && m_registry.HasComponent<Transform2D>(it->EntityId))
        {
            const float pulse = 0.5f + 0.5f * std::sin(it->ShootTimer * 30.0f);
            if (m_registry.HasComponent<SpriteComponent>(it->EntityId))
            {
                m_registry.GetComponent<SpriteComponent>(it->EntityId).Tint = Color{
                    255,
                    static_cast<uint8_t>(45 + 140 * pulse),
                    static_cast<uint8_t>(55 + 140 * pulse),
                    255
                };
            }
        }

        if (it->ShootTimer <= 0.0f)
        {
            it->ShootTimer = it->Cooldown;

            if (m_registry.HasComponent<Transform2D>(it->EntityId))
            {
                const auto& bTrans = m_registry.GetComponent<Transform2D>(it->EntityId);
                const Vector2f spawnPos{bTrans.Position.X, bTrans.Position.Y + 18.0f};

                TriangleProjectile proj;
                proj.Position = spawnPos;
                proj.Velocity = Vector2f{0.0f, it->ProjectileSpeed};
                proj.Size = 16.0f;
                m_turretBullets.push_back(proj);

                SpawnExplosionParticles(spawnPos, Color{255, 80, 40, 255}, 12);
                if (m_audioEnabled && m_bossLaserSfxId != 0)
                {
                    context.Audio.PlaySfx(m_bossLaserSfxId, 55.0f, 1.45f);
                }
            }

            if (m_registry.HasComponent<SpriteComponent>(it->EntityId))
            {
                const Color baseCol = (it->BaseHP > 3)
                    ? BrickFactory::GetColorForHitPoints(it->BaseHP)
                    : Color{255, 45, 55, 255};
                m_registry.GetComponent<SpriteComponent>(it->EntityId).Tint = baseCol;
            }
        }
        ++it;
    }

    if (m_turretBullets.empty())
    {
        return;
    }

    const bool hasPaddle = m_registry.IsAlive(m_paddle) && !m_isPaddleDestroyed &&
                           m_registry.HasComponent<Transform2D>(m_paddle) &&
                           m_registry.HasComponent<BoxCollider>(m_paddle);

    Vector2f paddlePos{0.0f, 350.0f};
    Vector2f halfSize{60.0f, 10.0f};
    if (hasPaddle)
    {
        const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddle);
        paddlePos = pTrans.Position;
        halfSize = pCol.GetEffectiveSize(pTrans.Scale) * 0.5f;
    }

    for (auto it = m_turretBullets.begin(); it != m_turretBullets.end(); )
    {
        it->Position.Y += it->Velocity.Y * dt;

        if (it->Position.Y > 450.0f)
        {
            it = m_turretBullets.erase(it);
            continue;
        }

        if (hasPaddle)
        {
            const float projW = it->Size * 0.75f;
            const float projH = it->Size * 0.85f;
            const bool hitX = (it->Position.X + projW >= paddlePos.X - halfSize.X) &&
                              (it->Position.X - projW <= paddlePos.X + halfSize.X);
            const bool hitY = (it->Position.Y + projH >= paddlePos.Y - halfSize.Y) &&
                              (it->Position.Y - projH <= paddlePos.Y + halfSize.Y + 6.0f);

            if (hitX && hitY)
            {
                const Vector2f impactPos = it->Position;
                it = m_turretBullets.erase(it);

                SpawnExplosionParticles(impactPos, Color{255, 60, 40, 255}, 28);
                TweenEffects::Shake(m_registry, m_camera, 0.35f, 16.0f);
                if (m_audioEnabled && m_paddleCrackSfxId != 0)
                {
                    context.Audio.PlaySfx(m_paddleCrackSfxId, 85.0f, 0.85f);
                }

                m_paddleDamageFlashTimer = 0.28f;
                if (m_registry.HasComponent<SpriteComponent>(m_paddle))
                {
                    m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Color{255, 50, 50, 255};
                }

                if (!context.Rules.GetRule(Rule::Gameplay::InfiniteLives))
                {
                    --m_lives;
                    context.Events.Publish(LivesChangedEvent(m_lives));
                    if (m_lives <= 0)
                    {
                        m_lives = 0;
                        TriggerPaddleExplosion(context);
                        return;
                    }
                }
                continue;
            }
        }
        ++it;
    }
}

void CustomLevelPlayScene::RenderTurrets(GameContext& context) const
{
    auto& registry = const_cast<Registry&>(m_registry);
    for (const auto& tb : m_turretBricks)
    {
        if (!registry.IsAlive(tb.EntityId) || !registry.HasComponent<Transform2D>(tb.EntityId))
        {
            continue;
        }
        if (registry.HasComponent<BrickComponent>(tb.EntityId) && registry.GetComponent<BrickComponent>(tb.EntityId).IsDescending)
        {
            continue;
        }

        const Vector2f pos = registry.GetComponent<Transform2D>(tb.EntityId).Position;
        const float triW = 14.0f;
        const float triH = 12.0f;
        std::vector<Vertex> iconVerts = {
            Vertex{pos.X - triW * 0.5f, pos.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{pos.X + triW * 0.5f, pos.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{pos.X, pos.Y + triH * 0.7f, Color{255, 255, 255, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(iconVerts, PrimitiveType::Triangles);

        if (tb.ShootTimer <= 0.45f)
        {
            const float chargeRatio = 1.0f - (std::max(0.0f, tb.ShootTimer) / 0.45f);
            const float muzzleRadius = 8.0f + 6.0f * chargeRatio;
            context.Render.DrawCircle(muzzleRadius, Transform2D{Vector2f{pos.X, pos.Y + 14.0f}},
                                      Color{255, 60, 40, static_cast<uint8_t>(200 * chargeRatio)});
        }
    }

    for (const auto& proj : m_turretBullets)
    {
        const Vector2f pos = proj.Position;
        const float w = proj.Size * 0.75f;
        const float h = proj.Size * 0.85f;

        std::vector<Vertex> verts = {
            Vertex{pos.X - w, pos.Y - h * 0.6f, Color{255, 50, 40, 245}, 0.0f, 0.0f},
            Vertex{pos.X + w, pos.Y - h * 0.6f, Color{255, 50, 40, 245}, 0.0f, 0.0f},
            Vertex{pos.X, pos.Y + h * 0.8f, Color{255, 220, 80, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(verts, PrimitiveType::Triangles);

        const Color outlineCol{255, 240, 160, 255};
        context.Render.DrawLine(Vector2f{pos.X - w, pos.Y - h * 0.6f}, Vector2f{pos.X + w, pos.Y - h * 0.6f}, outlineCol, 2.0f);
        context.Render.DrawLine(Vector2f{pos.X + w, pos.Y - h * 0.6f}, Vector2f{pos.X, pos.Y + h * 0.8f}, outlineCol, 2.0f);
        context.Render.DrawLine(Vector2f{pos.X, pos.Y + h * 0.8f}, Vector2f{pos.X - w, pos.Y - h * 0.6f}, outlineCol, 2.0f);
        context.Render.DrawPointLight(pos, 45.0f, Color{255, 60, 40, 200}, 0.8f, BlendMode::Add);
    }
}

void CustomLevelPlayScene::TriggerPaddleExplosion(GameContext& context)
{
    m_isPaddleDestroyed = true;
    m_paddleDestroyedTimer = 0.85f;
    m_lives = 0;

    Vector2f paddlePos{0.0f, 350.0f};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }

    Color paddleThemeColor{200, 235, 255, 255};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
    {
        paddleThemeColor = m_registry.GetComponent<LightSourceComponent>(m_paddle).LightColor;
    }
    else
    {
        const std::string skin = ProfileManager::Get().GetEquippedCosmetic(CosmeticSlot::PaddleSkin);
        if (skin == "paddle_gold_plating")
        {
            paddleThemeColor = Color{255, 215, 0, 255};
        }
        else if (skin == "paddle_obsidian")
        {
            paddleThemeColor = Color{140, 60, 220, 255};
        }
    }

    const Color secondaryColor{
        static_cast<uint8_t>(std::min(255, paddleThemeColor.r + 40)),
        static_cast<uint8_t>(std::min(255, paddleThemeColor.g + 40)),
        static_cast<uint8_t>(std::min(255, paddleThemeColor.b + 40)),
        255
    };

    SpawnExplosionParticles(paddlePos, paddleThemeColor, 75);
    SpawnExplosionParticles(paddlePos, secondaryColor, 55);
    SpawnLightFlash(paddlePos, 420.0f, paddleThemeColor, 3.0f, 0.5f);
    TweenEffects::Shake(m_registry, m_camera, 0.55f, 22.0f);

    if (m_audioEnabled && m_explosionSfxId != 0)
    {
        context.Audio.PlaySfx(m_explosionSfxId, 100.0f, 0.85f);
    }

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint.a = 0;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(false);
    }
    if (mp_ballSystem)
    {
        mp_ballSystem->ClearBalls(m_ball);
    }
}

void CustomLevelPlayScene::RenderPaddleDurabilityOverlay(GameContext& context) const
{
    if (!m_registry.IsAlive(m_paddle) || m_isPaddleDestroyed ||
        !m_registry.HasComponent<Transform2D>(m_paddle) || !m_registry.HasComponent<BoxCollider>(m_paddle))
    {
        return;
    }

    auto& registry = const_cast<Registry&>(m_registry);
    const auto& pTrans = registry.GetComponent<Transform2D>(m_paddle);
    const auto& pCol = registry.GetComponent<BoxCollider>(m_paddle);
    const Vector2f effSize = pCol.GetEffectiveSize(pTrans.Scale);

    if (m_lives < 3 && m_lives > 0)
    {
        const Color crackCol = (m_lives == 1) ? Color{255, 50, 50, 220} : Color{220, 220, 220, 170};
        const float halfW = effSize.X * 0.5f;
        const float halfH = effSize.Y * 0.5f;

        context.Render.DrawLine(Vector2f{pTrans.Position.X - halfW * 0.35f, pTrans.Position.Y - halfH},
                                Vector2f{pTrans.Position.X - halfW * 0.15f, pTrans.Position.Y + halfH}, crackCol, 2.0f);
        if (m_lives == 1)
        {
            context.Render.DrawLine(Vector2f{pTrans.Position.X + halfW * 0.25f, pTrans.Position.Y - halfH},
                                    Vector2f{pTrans.Position.X + halfW * 0.40f, pTrans.Position.Y + halfH}, crackCol, 2.0f);
        }
    }
}

void CustomLevelPlayScene::ReturnToRobostock(GameContext& context)
{
    const std::string focusId = (!m_levelIds.empty() && m_currentSlot < m_levelIds.size())
        ? m_levelIds[m_currentSlot]
        : std::string{};
    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 0.9f, 60.0f, true);
    context.Scenes.LoadSceneWithTransition<RobostockScene>(
        SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f},
        focusId);
}
