#include "Scenes/StoryScene.h"
#include "Core/LoreManager.h"
#include "Core/ProfileManager.h"
#include "Core/PlayerPrefs.h"
#include "Core/GameRules.h"
#include "PowerUps/PowerUpConfig.h"
#include "ECS/Systems/PaddleSystem.h"
#include "ECS/Systems/BallSystem.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/HazardZoneComponent.h"
#include "ECS/Components/EnvironmentalProjectileComponent.h"
#include "ECS/Systems/DynamicEventSystem.h"
#include "Factories/BrickFactory.h"
#include "Math/Physics.h"
#include "AudioMixer.h"
#include "TweenEffects/TweenEffects.h"
#include <cmath>
#include <algorithm>

StoryScene::StoryScene()
{
    InitChapters();
}

void StoryScene::InitChapters()
{
    m_chapters = StoryData::GetDefaultChapters();
    m_currentChapterIndex = 0;
}

void StoryScene::OnInit(GameContext& context)
{
    GameScene::OnInit(context);

    // Equip paddle with durability & fracture mechanics
    if (m_registry.IsAlive(m_paddle) && !m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
    {
        m_registry.AddComponent<PaddleDurabilityComponent>(m_paddle, PaddleDurabilityComponent{});
    }

    // Audio & Resources
    m_bossLaserSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_laser.wav");
    m_paddleCrackSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_hit.wav");
    m_shieldAbsorbSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");
    m_repairSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup.wav");

    // Load starting chapter
    LoadChapter(m_currentChapterIndex);
}

void StoryScene::OnDestroy(GameContext& context)
{
    m_bossBullets.clear();
    m_bossShieldBricks.clear();
    GameScene::OnDestroy(context);
}

bool StoryScene::IsGameOver() const
{
    if (m_isPaddleDestroyed)
    {
        return m_paddleDestroyedTimer <= 0.0f;
    }
    return false;
}

bool StoryScene::HasNextLevel() const
{
    return m_currentChapterIndex + 1 < static_cast<int>(m_chapters.size());
}

void StoryScene::AdvanceToNextLevel()
{
    if (HasNextLevel())
    {
        LoadChapter(m_currentChapterIndex + 1);
    }
}

void StoryScene::OnGameOverEnter()
{
    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.05f);
    m_scoreManager.BreakCombo();

    uint32_t finalScore = m_scoreManager.GetScore();
    uint32_t earnedCredits = ProfileManager::ConvertScoreToCredits(finalScore);
    PowerUpManager::Get().AddCoins(static_cast<int>(earnedCredits));
    ProfileManager::Get().AddLifetimeScore(finalScore);
    ProfileManager::Get().AddLifetimeCredits(earnedCredits);

    if (mp_context)
    {
        if (m_musicEnabled)
        {
            mp_context->Audio.FadeToMusic("Resources/audio/music/Game-death.ogg", 0.6f, 50.0f, false);
        }
        if (m_audioEnabled && m_gameOverSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_gameOverSfxId, 100.0f);
        }
    }

    if (m_uiController)
    {
        m_uiController->SetupGameOver(finalScore, m_currentChapterIndex + 1);
    }
}

void StoryScene::OnGameOverExit()
{
    FullReset();
}

void StoryScene::OnVictoryExit()
{
    m_currentChapterIndex = 0;
    FullReset();
}

void StoryScene::FullReset()
{
    ClearVisualMalus();
    RestoreTemporalAudio(0.1f);
    if (mp_eventSystem)
    {
        mp_eventSystem->ResetLifecycle();
    }
    if (mp_context)
    {
        m_playlist.PlayNext(*mp_context, 1.0f);
    }

    // 1. Refill player hearts/lives to 3
    m_lives = PlayerPrefs::GetInt("Lives", 3);
    if (m_lives <= 0)
    {
        m_lives = 3;
    }
    if (mp_context)
    {
        mp_context->Events.Publish(LivesChangedEvent(m_lives));
    }

    // 2. Reset score & combo
    m_scoreManager.Reset();
    if (mp_context)
    {
        mp_context->Events.Publish(ScoreChangedEvent(m_scoreManager.GetScore(), m_scoreManager.GetComboMultiplier()));
    }

    // 3. Reset paddle destruction & visual state
    m_isPaddleDestroyed = false;
    m_paddleDestroyedTimer = 0.0f;
    m_paddleDamageFlashTimer = 0.0f;
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(true);
    }

    // 4. Reset paddle durability
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
    {
        auto& dur = m_registry.GetComponent<PaddleDurabilityComponent>(m_paddle);
        dur.CurrentDurability = dur.MaxDurability;
        dur.CurrentShield = 0.0f;
        dur.IsShieldActive = false;
        dur.InvulnerabilityTimer = 0.0f;
    }

    // 5. Clean up any hostile projectiles
    m_triangleProjectiles.clear();
    for (Entity b : m_bossBullets)
    {
        if (m_registry.IsAlive(b))
        {
            m_registry.DestroyEntityDeferred(b);
        }
    }
    m_bossBullets.clear();
    m_bossLaserSweepTimer = 0.0f;

    // 6. Reload CURRENT CHAPTER (stay on current level!)
    LoadChapter(m_currentChapterIndex);
}

float StoryScene::GetBasePaddleScaleX() const
{
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
    {
        const auto& dur = const_cast<Registry&>(m_registry).GetComponent<PaddleDurabilityComponent>(m_paddle);
        return dur.GetEffectiveWidthMultiplier();
    }
    return 1.0f;
}

void StoryScene::LoadChapter(int chapterIndex)
{
    m_currentChapterIndex = std::clamp(chapterIndex, 0, static_cast<int>(m_chapters.size()) - 1);
    m_isPaddleDestroyed = false;
    m_paddleDestroyedTimer = 0.0f;
    m_paddleDamageFlashTimer = 0.0f;
    m_triangleProjectiles.clear();
    m_turretBricks.clear();
    m_isDialogueActive = false;

    // Refill lives if empty/lost
    if (m_lives <= 0)
    {
        m_lives = PlayerPrefs::GetInt("Lives", 3);
        if (m_lives <= 0)
        {
            m_lives = 3;
        }
        if (mp_context)
        {
            mp_context->Events.Publish(LivesChangedEvent(m_lives));
        }
    }

    // Reset paddle visual tint & ensure input is active
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(true);
    }

    // Gamerules Sync in Story Mode:
    // If ManualOverride is false, chapter rules define GameRules.
    // If ManualOverride is true, user settings in Settings override chapter rules.
    if (mp_context)
    {
        const bool manualOverride = mp_context->Rules.GetRule(Rule::Gameplay::ManualOverride);
        if (!manualOverride)
        {
            const auto& chapter = m_chapters[m_currentChapterIndex];
            mp_context->Rules.SetRule(Rule::Gameplay::ArenaEventsEnabled, chapter.IsBossChapter);
            mp_context->Rules.SetRule(Rule::Gameplay::HazardZonesEnabled, chapter.IsBossChapter);
            mp_context->Rules.SetRule(Rule::Gameplay::InfiniteLives, false);
            mp_context->Rules.SetRule(Rule::Gameplay::InfiniteUltimate, false);
        }
    }

    SetupChapterPlayfield(m_currentChapterIndex);
    ResetBallAndPaddle();
    StartCountdown();
}

void StoryScene::SetupChapterPlayfield(int chapterIndex)
{
    // Clean up existing boss, projectiles, and turrets
    if (m_bossEntity != NULL_ENTITY && m_registry.IsAlive(m_bossEntity))
    {
        m_registry.DestroyEntity(m_bossEntity);
        m_bossEntity = NULL_ENTITY;
    }
    for (Entity b : m_bossBullets)
    {
        if (m_registry.IsAlive(b)) m_registry.DestroyEntity(b);
    }
    m_bossBullets.clear();
    m_bossShieldBricks.clear();
    m_triangleProjectiles.clear();
    m_turretBricks.clear();

    // Clear non-wall entities from previous level
    std::vector<Entity> toRemove;
    m_registry.View<BrickComponent>([&](Entity e, BrickComponent&) {
        toRemove.push_back(e);
    });
    for (Entity e : toRemove)
    {
        m_registry.DestroyEntity(e);
    }
    m_brickCount = 0;

    const auto& chapter = m_chapters[chapterIndex];

    if (chapter.IsBossChapter)
    {
        // Chapter 3: Spawn OMEGA-01 Boss Encounter
        SpawnBoss(Vector2f{0.0f, -230.0f});
        SpawnBossShieldBricks();
    }
    else
    {
        // Aesthetic, well-spaced Campaign Level Layout
        const int rows = 5;
        const int cols = 10;
        const float brickWidth = 96.0f;
        const float brickHeight = 28.0f;
        const float spacing = 12.0f;
        const float totalW = cols * brickWidth + (cols - 1) * spacing;
        const float startX = -totalW * 0.5f + brickWidth * 0.5f;
        const float startY = -310.0f;

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                Vector2f pos{startX + c * (brickWidth + spacing), startY + r * (brickHeight + spacing)};

                // Turret Bricks in row 2 (columns 2, 5, 7): shoot downward triangles towards the paddle!
                if (r == 2 && (c == 2 || c == 5 || c == 7))
                {
                    Entity turret = BrickFactory::Create(m_registry, pos.X, pos.Y, BrickType::Hard, false, m_brickTexId, 2);
                    if (m_registry.HasComponent<SpriteComponent>(turret))
                    {
                        m_registry.GetComponent<SpriteComponent>(turret).Tint = Color{255, 45, 55, 255};
                    }
                    // Start cooldowns after countdown ends with generous initial stagger (3.5s to 8s)
                    m_turretBricks.push_back(TurretBrick{turret, 3.5f + (c * 0.8f), 3.2f});
                    m_brickCount++;
                    continue;
                }

                // Standard destructible campaign bricks (no infinite regenerating special bricks)
                const bool isSpecial = false;

                BrickType bType = BrickType::Light;
                uint8_t armor = 1;
                if (r == 0) { bType = BrickType::Hard; armor = 2; }
                else if (r == 1 || r == 2) { bType = BrickType::Medium; armor = 1; }
                else { bType = BrickType::Light; armor = 1; }

                Entity brick = BrickFactory::Create(m_registry, pos.X, pos.Y, bType, isSpecial, m_brickTexId, armor);
                (void)brick;
                m_brickCount++;
            }
        }
    }
}

Entity StoryScene::SpawnBoss(const Vector2f& position)
{
    m_bossEntity = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_bossEntity, Transform2D{position, 0.0f, Vector2f{1.0f, 1.0f}});
    m_registry.AddComponent<BoxCollider>(m_bossEntity, BoxCollider{Vector2f{280.0f, 100.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<RigidBody>(m_bossEntity, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});

    BossComponent boss{};
    boss.BasePosition = position;
    boss.CurrentPosition = position;
    boss.MaxHealth = 360.0f;
    boss.CurrentHealth = 360.0f;
    boss.Phase = BossPhase::Phase1;
    boss.WeakPoint.RelativeOffset = Vector2f{0.0f, 25.0f};
    boss.WeakPoint.Radius = 32.0f;
    boss.WeakPoint.DamageMultiplier = 2.5f;

    m_registry.AddComponent<BossComponent>(m_bossEntity, boss);
    return m_bossEntity;
}

void StoryScene::SpawnBossShieldBricks()
{
    m_bossShieldBricks.clear();
    const float y = -140.0f;
    const float brickW = 75.0f;
    const float spacing = 12.0f;
    const int count = 5;
    const float startX = -((count - 1) * (brickW + spacing)) * 0.5f;

    for (int i = 0; i < count; ++i)
    {
        Vector2f pos{startX + i * (brickW + spacing), y};
        Entity b = BrickFactory::Create(m_registry, pos.X, pos.Y, BrickType::Hard, false, m_brickTexId, 2);
        m_bossShieldBricks.push_back(b);
        m_brickCount++;
    }
}

void StoryScene::SpawnBossBullet(const Vector2f& pos, const Vector2f& vel, float damage)
{
    Entity bullet = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(bullet, Transform2D{pos, 0.0f, Vector2f{1.0f, 1.0f}});
    m_registry.AddComponent<CircleCollider>(bullet, CircleCollider{12.0f, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<RigidBody>(bullet, RigidBody{vel, 1.0f, 1.0f, true});

    BossProjectileComponent bpc{};
    bpc.Damage = damage;
    bpc.Radius = 12.0f;
    bpc.Velocity = vel;
    bpc.IsDeflected = false;
    bpc.Lifetime = 6.0f;
    bpc.ColorTint = Color{255, 60, 60, 255};

    m_registry.AddComponent<BossProjectileComponent>(bullet, bpc);
    m_bossBullets.push_back(bullet);
}

void StoryScene::TriggerBossLaser(float startX, float chargeDuration, float fireDuration)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    if (!m_registry.HasComponent<BossComponent>(m_bossEntity)) return;

    auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
    BossLaserBeam beam{};
    beam.StartX = startX;
    beam.Width = 28.0f;
    beam.ChargeDuration = chargeDuration;
    beam.ChargeTimer = chargeDuration;
    beam.FireDuration = fireDuration;
    beam.FireTimer = 0.0f;
    beam.IsCharging = true;
    beam.IsFiring = false;
    beam.DamagePerSecond = 45.0f;

    boss.LaserBeams.push_back(beam);
}

void StoryScene::StartChapterDialogue(bool isIntro)
{
    m_isDialogueActive = true;
    m_isIntroDialogue = isIntro;
    m_dialogueCardIndex = 0;
    m_dialogueCharTimer = 0.0f;
    m_dialogueVisibleChars = 0;

    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(false);
    }

    if (isIntro)
    {
        // Cancel/pause countdown so "READY..." is not displayed or frozen during cutscene!
        m_countdownActive = false;
        m_countdownTimer = 0.0f;
        m_countdownGoTimer = 0.0f;
    }
}

void StoryScene::AdvanceDialogue()
{
    if (!m_isDialogueActive) return;

    const auto& chapter = m_chapters[m_currentChapterIndex];
    const auto& cards = m_isIntroDialogue ? chapter.IntroDialogue : chapter.OutroDialogue;

    // If typewriter text is still printing, clicking immediately displays the full text
    if (m_dialogueCardIndex < cards.size())
    {
        const int fullLen = static_cast<int>(cards[m_dialogueCardIndex].Text.length());
        if (m_dialogueVisibleChars < fullLen)
        {
            m_dialogueVisibleChars = fullLen;
            return;
        }
    }

    m_dialogueCardIndex++;
    m_dialogueCharTimer = 0.0f;
    m_dialogueVisibleChars = 0;

    if (m_dialogueCardIndex >= cards.size())
    {
        m_isDialogueActive = false;

        // If intro dialogue finished, launch the countdown sequence!
        if (m_isIntroDialogue)
        {
            StartCountdown();
        }
        else
        {
            if (mp_paddleSystem)
            {
                mp_paddleSystem->SetInputEnabled(true);
            }
            if (HasNextLevel())
            {
                AdvanceToNextLevel();
            }
            else
            {
                if (mp_state_machine)
                {
                    mp_state_machine->SetState(static_cast<int>(SceneState::Victory));
                }
            }
        }
    }
}

void StoryScene::SkipDialogue()
{
    m_isDialogueActive = false;

    if (m_isIntroDialogue)
    {
        StartCountdown();
    }
    else
    {
        if (mp_paddleSystem)
        {
            mp_paddleSystem->SetInputEnabled(true);
        }
        if (HasNextLevel())
        {
            AdvanceToNextLevel();
        }
        else
        {
            if (mp_state_machine)
            {
                mp_state_machine->SetState(static_cast<int>(SceneState::Victory));
            }
        }
    }
}

void StoryScene::CompleteChapter()
{
    const auto& chapter = m_chapters[m_currentChapterIndex];

    // Grant Lore Unlock
    if (!chapter.UnlockedLoreId.empty())
    {
        LoreManager::Get().UnlockLore(chapter.UnlockedLoreId);
    }

    // Grant Currency Reward
    if (chapter.CurrencyReward > 0)
    {
        ProfileManager::Get().AddLifetimeCredits(static_cast<uint32_t>(chapter.CurrencyReward));
        PowerUpManager::Get().AddCoins(chapter.CurrencyReward);
    }

    // Grant Exclusive Cosmetic Skin
    if (!chapter.UnlockedCosmeticId.empty())
    {
        ProfileManager::Get().GrantCosmetic(chapter.UnlockedCosmeticId);
        LoreManager::Get().UnlockLore("lore_boss_clear");
    }

    if (HasNextLevel())
    {
        AdvanceToNextLevel();
    }
    else
    {
        if (mp_state_machine)
        {
            mp_state_machine->SetState(static_cast<int>(SceneState::Victory));
        }
    }
}

void StoryScene::OnUpdate(float dt, GameContext& context)
{
    // Update paddle damage flash timer
    if (m_paddleDamageFlashTimer > 0.0f)
    {
        m_paddleDamageFlashTimer -= dt;
        if (m_paddleDamageFlashTimer <= 0.0f)
        {
            m_paddleDamageFlashTimer = 0.0f;
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle) && !m_isPaddleDestroyed)
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
            }
        }
    }

    // Base Gameplay Update
    GameScene::OnUpdate(dt, context);

    if (m_lives <= 0 && !m_isPaddleDestroyed)
    {
        TriggerPaddleExplosion(context);
        return;
    }

    // If paddle was destroyed, countdown the death timer and spawn smoldering smoke/sparks
    if (m_isPaddleDestroyed)
    {
        if (m_paddleDestroyedTimer > 0.0f)
        {
            m_paddleDestroyedTimer -= dt;

            // Smoldering embers and smoke during the death delay
            m_sparkEmissionTimer += dt;
            if (m_sparkEmissionTimer >= 0.07f)
            {
                m_sparkEmissionTimer = 0.0f;
                Vector2f paddlePos{0.0f, 350.0f};
                if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
                {
                    paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
                }
                const float offsetRnd = (static_cast<float>(rand() % 100) - 50.0f) * 1.4f;
                Color sparkCol{200, 235, 255, 200};
                if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
                {
                    sparkCol = m_registry.GetComponent<LightSourceComponent>(m_paddle).LightColor;
                    sparkCol.a = 200;
                }
                SpawnExplosionParticles(paddlePos + Vector2f{offsetRnd, 0.0f}, sparkCol, 8);
            }
        }
        return;
    }

    // Turret Bricks & Downward Triangle Projectiles
    UpdateTurretBricks(dt, context);
    UpdateTriangleProjectiles(dt, context);

    // Boss Encounter Loop or Standard Chapter Clear
    if (m_chapters[m_currentChapterIndex].IsBossChapter)
    {
        UpdateBoss(dt, context);
        UpdateBossAttacks(dt, context);
        UpdateBossProjectiles(dt, context);
        CheckBossCollisions(context);
    }
    else
    {
        // Standard Chapter Clear Condition: all destroyable bricks cleared
        if (m_brickCount <= 0 && m_brickCount != -1)
        {
            CompleteChapter();
        }
    }
}

void StoryScene::UpdateTurretBricks(float dt, GameContext& context)
{
    // Do NOT advance turret timers or fire missiles while the countdown is active!
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

        it->ShootTimer -= dt;

        // Visual warning charge in the last 0.45s before firing
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

        // Fire downward triangle missile!
        if (it->ShootTimer <= 0.0f)
        {
            it->ShootTimer = it->Cooldown;

            if (m_registry.HasComponent<Transform2D>(it->EntityId))
            {
                const auto& bTrans = m_registry.GetComponent<Transform2D>(it->EntityId);
                Vector2f spawnPos{bTrans.Position.X, bTrans.Position.Y + 18.0f};

                TriangleProjectile proj;
                proj.Position = spawnPos;
                proj.Velocity = Vector2f{0.0f, 210.0f};
                proj.Size = 16.0f;
                m_triangleProjectiles.push_back(proj);

                // Muzzle flash & audio
                SpawnExplosionParticles(spawnPos, Color{255, 80, 40, 255}, 12);
                if (m_audioEnabled && m_bossLaserSfxId != 0)
                {
                    context.Audio.PlaySfx(m_bossLaserSfxId, 55.0f, 1.45f);
                }
            }

            if (m_registry.HasComponent<SpriteComponent>(it->EntityId))
            {
                m_registry.GetComponent<SpriteComponent>(it->EntityId).Tint = Color{255, 45, 55, 255};
            }
        }

        ++it;
    }
}

void StoryScene::UpdateTriangleProjectiles(float dt, GameContext& context)
{
    if (m_triangleProjectiles.empty()) return;

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

    for (auto it = m_triangleProjectiles.begin(); it != m_triangleProjectiles.end(); )
    {
        it->Position.Y += it->Velocity.Y * dt;

        // Off-screen cleanup
        if (it->Position.Y > 450.0f)
        {
            it = m_triangleProjectiles.erase(it);
            continue;
        }

        // Collision with paddle
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
                Vector2f impactPos = it->Position;
                it = m_triangleProjectiles.erase(it);

                // 1. Impact feedback
                SpawnExplosionParticles(impactPos, Color{255, 60, 40, 255}, 28);
                TweenEffects::Shake(m_registry, m_camera, 0.35f, 16.0f);
                if (m_audioEnabled && m_paddleCrackSfxId != 0)
                {
                    context.Audio.PlaySfx(m_paddleCrackSfxId, 85.0f, 0.85f);
                }

                // 2. Paddle damage flash
                m_paddleDamageFlashTimer = 0.28f;
                if (m_registry.HasComponent<SpriteComponent>(m_paddle))
                {
                    m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Color{255, 50, 50, 255};
                }

                // 3. Player loses a heart/life
                m_lives--;
                context.Events.Publish(LivesChangedEvent(m_lives));

                // 4. When paddle reaches 0 lives -> Explodes & triggers Game Over!
                if (m_lives <= 0)
                {
                    m_lives = 0;
                    TriggerPaddleExplosion(context);
                    return;
                }

                continue;
            }
        }

        ++it;
    }
}

void StoryScene::TriggerPaddleExplosion(GameContext& context)
{
    m_isPaddleDestroyed = true;
    m_paddleDestroyedTimer = 0.85f;
    m_lives = 0;

    Vector2f paddlePos{0.0f, 350.0f};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }

    // Determine paddle's theme color (from LightSourceComponent, equipped cosmetic skin, or default cyan)
    Color paddleThemeColor{200, 235, 255, 255};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
    {
        paddleThemeColor = m_registry.GetComponent<LightSourceComponent>(m_paddle).LightColor;
    }
    else
    {
        std::string skin = ProfileManager::Get().GetEquippedCosmetic(CosmeticSlot::PaddleSkin);
        if (skin == "paddle_gold_plating")
        {
            paddleThemeColor = Color{255, 215, 0, 255};
        }
        else if (skin == "paddle_obsidian")
        {
            paddleThemeColor = Color{140, 60, 220, 255};
        }
    }

    Color secondaryColor{
        static_cast<uint8_t>(std::min(255, paddleThemeColor.r + 40)),
        static_cast<uint8_t>(std::min(255, paddleThemeColor.g + 40)),
        static_cast<uint8_t>(std::min(255, paddleThemeColor.b + 40)),
        255
    };

    // Paddle explosion themed to paddle color
    SpawnExplosionParticles(paddlePos, paddleThemeColor, 75);
    SpawnExplosionParticles(paddlePos, secondaryColor, 55);
    SpawnLightFlash(paddlePos, 420.0f, paddleThemeColor, 3.0f, 0.5f);
    TweenEffects::Shake(m_registry, m_camera, 0.55f, 22.0f);

    if (m_audioEnabled && m_explosionSfxId != 0)
    {
        context.Audio.PlaySfx(m_explosionSfxId, 100.0f, 0.85f);
    }

    // Hide paddle sprite & detach ball
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

void StoryScene::UpdatePaddleDurability(float dt, GameContext& context)
{
    if (!m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
        return;

    auto& dur = m_registry.GetComponent<PaddleDurabilityComponent>(m_paddle);
    dur.Update(dt);

    // Dynamic width scaling based on fracture state
    if (m_registry.HasComponent<Transform2D>(m_paddle))
    {
        auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        paddleTrans.Scale.X = dur.GetEffectiveWidthMultiplier();
    }

    // Sparking VFX for fractured/critical states
    m_sparkEmissionTimer += dt;
    const auto state = dur.GetFractureState();
    if (state == PaddleFractureState::Fractured || state == PaddleFractureState::Critical)
    {
        const float interval = (state == PaddleFractureState::Critical) ? 0.08f : 0.22f;
        if (m_sparkEmissionTimer >= interval)
        {
            m_sparkEmissionTimer = 0.0f;
            if (m_registry.HasComponent<Transform2D>(m_paddle))
            {
                const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
                const float offsetX = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 100.0f;
                SpawnExplosionParticles(Vector2f{pTrans.Position.X + offsetX, pTrans.Position.Y}, Color{255, 200, 50, 255}, 5);
            }
        }
    }

    // Paddle Destruction Defeat Condition
    if (dur.IsDestroyed() && !m_isPaddleDestroyed)
    {
        TriggerPaddleDestruction(context);
    }
}

void StoryScene::TriggerPaddleDestruction(GameContext& context)
{
    m_isPaddleDestroyed = true;

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        // Massive shatter explosion
        SpawnExplosionParticles(pTrans.Position, Color{255, 60, 60, 255}, 50);
        SpawnExplosionParticles(pTrans.Position, Color{255, 220, 50, 255}, 40);
        SpawnLightFlash(pTrans.Position, 650.0f, Color{255, 100, 50, 255}, 2.5f, 0.6f);
    }

    if (m_audioEnabled && m_explosionSfxId != 0)
    {
        context.Audio.PlaySfx(m_explosionSfxId, 100.0f, 0.8f);
    }

    // Second immediate defeat condition alongside ball loss
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::GameOver));
    }
}

void StoryScene::CheckHazardPaddleCollisions(GameContext& context)
{
    if (m_isPaddleDestroyed || !m_registry.IsAlive(m_paddle)) return;
    if (!m_registry.HasComponent<Transform2D>(m_paddle) || !m_registry.HasComponent<BoxCollider>(m_paddle) ||
        !m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
        return;

    const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddle);
    auto& dur = m_registry.GetComponent<PaddleDurabilityComponent>(m_paddle);

    // 1. Debuff Hazards damaging paddle durability
    m_registry.View<Transform2D, HazardZoneComponent>([&](Entity, const Transform2D& hTrans, HazardZoneComponent& hComp)
    {
        if (!hComp.IsActive) return;
        const float dx = std::abs(pTrans.Position.X - hTrans.Position.X);
        const float dy = std::abs(pTrans.Position.Y - hTrans.Position.Y);
        if (dx < (hComp.Size.X * 0.5f + pCol.GetEffectiveSize(pTrans.Scale).X * 0.5f) &&
            dy < (hComp.Size.Y * 0.5f + pCol.GetEffectiveSize(pTrans.Scale).Y * 0.5f))
        {
            if (dur.TakeDamage(12.0f))
            {
                if (m_audioEnabled && m_paddleCrackSfxId != 0)
                {
                    context.Audio.PlaySfx(m_paddleCrackSfxId, 60.0f, 1.1f);
                }
            }
        }
    });

    // 2. Meteors / Hostile Projectiles from DynamicEventSystem
    if (mp_eventSystem)
    {
        for (Entity proj : mp_eventSystem->GetActiveProjectiles())
        {
            if (!m_registry.IsAlive(proj) || !m_registry.HasComponent<Transform2D>(proj) ||
                !m_registry.HasComponent<CircleCollider>(proj) || !m_registry.HasComponent<EnvironmentalProjectileComponent>(proj))
                continue;

            const auto& epc = m_registry.GetComponent<EnvironmentalProjectileComponent>(proj);
            if (epc.IsDeflected) continue;

            const auto& mTrans = m_registry.GetComponent<Transform2D>(proj);
            const auto& mCol = m_registry.GetComponent<CircleCollider>(proj);

            if (Physics::IntersectCircleAABB(mCol, mTrans, pCol, pTrans).IsColliding)
            {
                if (dur.TakeDamage(20.0f))
                {
                    SpawnExplosionParticles(mTrans.Position, Color{255, 120, 40, 255}, 15);
                    if (m_audioEnabled && m_paddleCrackSfxId != 0)
                    {
                        context.Audio.PlaySfx(m_paddleCrackSfxId, 80.0f, 0.9f);
                    }
                }
            }
        }
    }
}

void StoryScene::UpdateBoss(float dt, GameContext& context)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    if (!m_registry.HasComponent<BossComponent>(m_bossEntity) || !m_registry.HasComponent<Transform2D>(m_bossEntity))
        return;

    auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
    auto& bTrans = m_registry.GetComponent<Transform2D>(m_bossEntity);

    // Defeat sequence handling
    if (boss.IsDefeated())
    {
        boss.DefeatTimer += dt;
        m_bossDefeatCinematicTimer += dt;

        // Cascading explosions across boss hull
        if (m_bossDefeatCinematicTimer >= 0.25f && m_bossExplosionIndex < 12)
        {
            m_bossDefeatCinematicTimer = 0.0f;
            m_bossExplosionIndex++;
            const float offsetX = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 260.0f;
            const float offsetY = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 90.0f;
            Vector2f expPos = bTrans.Position + Vector2f{offsetX, offsetY};
            SpawnExplosionParticles(expPos, Color{255, 210, 60, 255}, 25);
            SpawnLightFlash(expPos, 350.0f, Color{255, 140, 40, 255}, 2.0f, 0.35f);

            if (m_audioEnabled && m_explosionSfxId != 0)
            {
                context.Audio.PlaySfx(m_explosionSfxId, 75.0f, 0.8f + static_cast<float>(m_bossExplosionIndex) * 0.05f);
            }
        }

        if (boss.DefeatTimer >= boss.DefeatDuration && !boss.DefeatCinematicComplete)
        {
            boss.DefeatCinematicComplete = true;
            TriggerBossDefeat(context);
        }
        return;
    }

    // Swaying movement pattern
    boss.SwayTimer += dt * boss.SwayFrequency;
    bTrans.Position.X = boss.BasePosition.X + std::sin(boss.SwayTimer) * boss.SwayAmplitude;
    bTrans.Position.Y = boss.BasePosition.Y + std::cos(boss.SwayTimer * 0.5f) * 25.0f;
    boss.CurrentPosition = bTrans.Position;

    if (boss.DamageFlashTimer > 0.0f)
    {
        boss.DamageFlashTimer = std::max(0.0f, boss.DamageFlashTimer - dt);
    }
    boss.WeakPoint.PulseTimer += dt * 4.0f;
}

void StoryScene::UpdateBossAttacks(float dt, GameContext& context)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
    if (boss.IsDefeated()) return;

    boss.AttackCooldownTimer -= dt;
    if (boss.AttackCooldownTimer <= 0.0f)
    {
        boss.AttackCooldownTimer = boss.AttackInterval;

        // Alternating attack patterns by Phase
        if (boss.Phase == BossPhase::Phase1)
        {
            // 3-way Bullet Hell Volley
            const Vector2f corePos = boss.CurrentPosition + boss.WeakPoint.RelativeOffset;
            SpawnBossBullet(corePos, Vector2f{-100.0f, 220.0f});
            SpawnBossBullet(corePos, Vector2f{0.0f, 240.0f});
            SpawnBossBullet(corePos, Vector2f{100.0f, 220.0f});
        }
        else if (boss.Phase == BossPhase::Phase2)
        {
            // Dual Sweeping Laser Beams + 5-way volley
            const float paddleX = (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle)) ?
                                   m_registry.GetComponent<Transform2D>(m_paddle).Position.X : 0.0f;
            TriggerBossLaser(paddleX - 120.0f, 0.9f, 1.2f);
            TriggerBossLaser(paddleX + 120.0f, 0.9f, 1.2f);

            const Vector2f corePos = boss.CurrentPosition + boss.WeakPoint.RelativeOffset;
            for (int i = -2; i <= 2; ++i)
            {
                SpawnBossBullet(corePos, Vector2f{static_cast<float>(i) * 65.0f, 230.0f});
            }
        }
        else if (boss.Phase == BossPhase::Phase3)
        {
            // Enrage: Rapid Central Laser + 7-way bullet storm
            const float paddleX = (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle)) ?
                                   m_registry.GetComponent<Transform2D>(m_paddle).Position.X : 0.0f;
            TriggerBossLaser(paddleX, 0.65f, 1.4f);

            const Vector2f corePos = boss.CurrentPosition + boss.WeakPoint.RelativeOffset;
            for (int i = -3; i <= 3; ++i)
            {
                SpawnBossBullet(corePos, Vector2f{static_cast<float>(i) * 75.0f, 260.0f}, 22.0f);
            }
        }
    }

    // Laser Beams lifecycle & paddle damage check
    for (auto it = boss.LaserBeams.begin(); it != boss.LaserBeams.end(); )
    {
        if (it->IsCharging)
        {
            it->ChargeTimer -= dt;
            if (it->ChargeTimer <= 0.0f)
            {
                it->IsCharging = false;
                it->IsFiring = true;
                it->FireTimer = it->FireDuration;
                if (m_audioEnabled && m_bossLaserSfxId != 0)
                {
                    context.Audio.PlaySfx(m_bossLaserSfxId, 85.0f, 0.85f);
                }
            }
            ++it;
        }
        else if (it->IsFiring)
        {
            it->FireTimer -= dt;

            // Damage paddle if paddle intersects laser beam X
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle) &&
                m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
            {
                const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
                auto& dur = m_registry.GetComponent<PaddleDurabilityComponent>(m_paddle);
                const float halfBeam = it->Width * 0.5f;

                if (pTrans.Position.X >= (it->StartX - halfBeam - 40.0f) &&
                    pTrans.Position.X <= (it->StartX + halfBeam + 40.0f))
                {
                    dur.TakeDamage(it->DamagePerSecond * dt, true);
                }
            }

            if (it->FireTimer <= 0.0f)
            {
                it = boss.LaserBeams.erase(it);
            }
            else
            {
                ++it;
            }
        }
        else
        {
            it = boss.LaserBeams.erase(it);
        }
    }
}

void StoryScene::UpdateBossProjectiles(float dt, GameContext& context)
{
    std::erase_if(m_bossBullets, [this](Entity e) { return !m_registry.IsAlive(e); });

    for (Entity b : m_bossBullets)
    {
        if (!m_registry.IsAlive(b)) continue;
        if (!m_registry.HasComponent<Transform2D>(b) || !m_registry.HasComponent<BossProjectileComponent>(b)) continue;

        auto& bTrans = m_registry.GetComponent<Transform2D>(b);
        auto& bpc = m_registry.GetComponent<BossProjectileComponent>(b);

        bTrans.Position += bpc.Velocity * dt;
        bpc.Lifetime -= dt;

        // Despawn expired or off-screen bullets
        if (bTrans.Position.Y > 500.0f || bTrans.Position.Y < -500.0f || bpc.Lifetime <= 0.0f)
        {
            m_registry.DestroyEntityDeferred(b);
            continue;
        }

        // Bullet vs Paddle Collision
        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle) &&
            m_registry.HasComponent<BoxCollider>(m_paddle) && m_registry.HasComponent<PaddleDurabilityComponent>(m_paddle))
        {
            const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddle);
            auto& dur = m_registry.GetComponent<PaddleDurabilityComponent>(m_paddle);

            CircleCollider bCol{bpc.Radius, Vector2f{0.0f, 0.0f}, false, true};
            if (!bpc.IsDeflected && bpc.Velocity.Y > 0.0f && Physics::IntersectCircleAABB(bCol, bTrans, pCol, pTrans).IsColliding)
            {
                // Test deflection vs direct damage
                const float hitOffset = (bTrans.Position.X - pTrans.Position.X) / 60.0f;
                if (std::abs(hitOffset) <= 0.95f)
                {
                    // Deflected back upwards toward boss!
                    bpc.IsDeflected = true;
                    bpc.Velocity.Y = -std::abs(bpc.Velocity.Y) * 1.25f;
                    bpc.Velocity.X += hitOffset * 150.0f;
                    bpc.ColorTint = Color{80, 220, 255, 255}; // Turn friendly cyan
                    SpawnExplosionParticles(bTrans.Position, Color{100, 240, 255, 255}, 12);
                    if (m_audioEnabled && m_bounceSfxId != 0)
                    {
                        context.Audio.PlaySfx(m_bounceSfxId, 65.0f, 1.3f);
                    }
                }
                else
                {
                    // Direct hit on paddle edge damages durability
                    dur.TakeDamage(bpc.Damage);
                    SpawnExplosionParticles(bTrans.Position, Color{255, 60, 60, 255}, 15);
                    m_registry.DestroyEntityDeferred(b);
                }
            }
        }

        // Deflected bullet vs Boss Collision
        if (bpc.IsDeflected && m_bossEntity != NULL_ENTITY && m_registry.IsAlive(m_bossEntity) &&
            m_registry.HasComponent<BossComponent>(m_bossEntity) && m_registry.HasComponent<Transform2D>(m_bossEntity))
        {
            auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
            const auto& bEntityTrans = m_registry.GetComponent<Transform2D>(m_bossEntity);
            const Vector2f corePos = bEntityTrans.Position + boss.WeakPoint.RelativeOffset;

            // Check weak point hit
            if ((bTrans.Position - corePos).Length() <= (boss.WeakPoint.Radius + bpc.Radius))
            {
                boss.TakeDamage(bpc.Damage, true);
                SpawnExplosionParticles(bTrans.Position, Color{255, 220, 50, 255}, 20);
                m_scoreManager.AddScore(250);
                m_registry.DestroyEntityDeferred(b);
            }
        }
    }
}

void StoryScene::CheckBossCollisions(GameContext& context)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    if (!m_registry.HasComponent<BossComponent>(m_bossEntity) || !m_registry.HasComponent<Transform2D>(m_bossEntity))
        return;

    auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
    const auto& bossTrans = m_registry.GetComponent<Transform2D>(m_bossEntity);
    if (boss.IsDefeated()) return;

    const Vector2f weakPointWorld = bossTrans.Position + boss.WeakPoint.RelativeOffset;

    // Check deflected balls against Boss Weak Point & Armored Hull
    const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
    for (Entity b : balls)
    {
        if (!m_registry.IsAlive(b)) continue;
        if (!m_registry.HasComponent<Transform2D>(b) || !m_registry.HasComponent<RigidBody>(b) ||
            !m_registry.HasComponent<CircleCollider>(b))
            continue;

        auto& bTrans = m_registry.GetComponent<Transform2D>(b);
        auto& bRb = m_registry.GetComponent<RigidBody>(b);
        const auto& bCol = m_registry.GetComponent<CircleCollider>(b);
        const float ballRadius = bCol.GetEffectiveRadius(bTrans.Scale);

        // 1. Weak Point Strike Check (Circular Core)
        const float distToCore = (bTrans.Position - weakPointWorld).Length();
        if (distToCore <= (boss.WeakPoint.Radius + ballRadius))
        {
            // CRITICAL HIT! Deflected ball penetrated weak point!
            boss.TakeDamage(20.0f, true);
            bRb.Velocity.Y = std::abs(bRb.Velocity.Y); // Reflect downwards

            SpawnExplosionParticles(bTrans.Position, Color{255, 230, 60, 255}, 30);
            SpawnLightFlash(weakPointWorld, 420.0f, Color{255, 200, 50, 255}, 2.2f, 0.35f);
            TriggerCameraZoomPulse(0.35f, 4.0f, 0.06f);
            m_scoreManager.AddScore(500);

            if (m_audioEnabled && m_explosionSfxId != 0)
            {
                context.Audio.PlaySfx(m_explosionSfxId, 85.0f, 1.2f);
            }
            continue;
        }

        // 2. Armored Hull Check (AABB Box Collider)
        if (m_registry.HasComponent<BoxCollider>(m_bossEntity))
        {
            const auto& bossCol = m_registry.GetComponent<BoxCollider>(m_bossEntity);
            if (Physics::IntersectCircleAABB(bCol, bTrans, bossCol, bossTrans).IsColliding)
            {
                // Ball bounces off armored hull with reduced damage
                boss.TakeDamage(4.0f, false);
                bRb.Velocity.Y = std::abs(bRb.Velocity.Y);

                SpawnExplosionParticles(bTrans.Position, Color{150, 160, 180, 255}, 12);
                if (m_audioEnabled && m_bounceSfxId != 0)
                {
                    context.Audio.PlaySfx(m_bounceSfxId, 70.0f, 0.85f);
                }
            }
        }
    }
}

void StoryScene::TriggerBossDefeat(GameContext& /*context*/)
{
    CompleteChapter();
}

void StoryScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    GameScene::HandleBrickCollision(entity, ballEntity, isLaser, isExplosion, damage);
}

void StoryScene::SpawnPowerUpCapsule(const Vector2f& position, PowerUpType type)
{
    Entity capsule = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(capsule, Transform2D{position, 0.0f, Vector2f{1.0f, 1.0f}});
    m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{36.0f, 36.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<RigidBody>(capsule, RigidBody{Vector2f{0.0f, 220.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 220.0f, false});

    uint32_t orbTex = GetPowerUpOrbTexture(type);
    Color color = (orbTex != m_brickTexId) ? Colors::White : GetPowerUpColor(type);
    m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{orbTex, color, 3});
}

void StoryScene::OnRender(GameContext& context)
{
    GameScene::OnRender(context);
}

void StoryScene::RenderSceneSpecificHUD(GameContext& context)
{
    // 1. In-world rendering under game world camera
    if (m_registry.IsAlive(m_camera) && m_registry.HasComponent<Camera2D>(m_camera))
    {
        context.Render.SetCamera(m_registry.GetComponent<Camera2D>(m_camera));
    }

    RenderTurretBricks(context);
    RenderTriangleProjectiles(context);
    RenderPaddleDurabilityOverlay(context);

    if (m_chapters[m_currentChapterIndex].IsBossChapter)
    {
        RenderBossLasers(context);
        RenderBossWeakPoint(context);
    }

    // 2. Screen-space UI rendering under default UI camera
    context.Render.ResetCamera();

    if (m_chapters[m_currentChapterIndex].IsBossChapter)
    {
        RenderBossHUD(context);
    }
}

void StoryScene::RenderTurretBricks(GameContext& context)
{
    for (const auto& tb : m_turretBricks)
    {
        if (!m_registry.IsAlive(tb.EntityId) || !m_registry.HasComponent<Transform2D>(tb.EntityId))
            continue;

        const auto& bTrans = m_registry.GetComponent<Transform2D>(tb.EntityId);
        const Vector2f pos = bTrans.Position;

        // Cannon barrel indicator pointing downwards on turret brick face
        const float triW = 14.0f;
        const float triH = 12.0f;
        std::vector<Vertex> iconVerts = {
            Vertex{pos.X - triW * 0.5f, pos.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{pos.X + triW * 0.5f, pos.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{pos.X, pos.Y + triH * 0.7f, Color{255, 255, 255, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(iconVerts, PrimitiveType::Triangles);

        // Warning muzzle pulse when charging
        if (tb.ShootTimer <= 0.45f)
        {
            const float chargeRatio = 1.0f - (tb.ShootTimer / 0.45f);
            const float muzzleRadius = 8.0f + 6.0f * chargeRatio;
            context.Render.DrawCircle(muzzleRadius, Transform2D{Vector2f{pos.X, pos.Y + 14.0f}}, Color{255, 60, 40, static_cast<uint8_t>(200 * chargeRatio)});
        }
    }
}

void StoryScene::RenderTriangleProjectiles(GameContext& context)
{
    for (const auto& proj : m_triangleProjectiles)
    {
        const Vector2f pos = proj.Position;
        const float w = proj.Size * 0.75f;
        const float h = proj.Size * 0.85f;

        // Filled glowing triangle missile pointing DOWNWARDS
        std::vector<Vertex> verts = {
            Vertex{pos.X - w, pos.Y - h * 0.6f, Color{255, 50, 40, 245}, 0.0f, 0.0f},
            Vertex{pos.X + w, pos.Y - h * 0.6f, Color{255, 50, 40, 245}, 0.0f, 0.0f},
            Vertex{pos.X, pos.Y + h * 0.8f, Color{255, 220, 80, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(verts, PrimitiveType::Triangles);

        // High contrast glowing outline
        Color outlineCol{255, 240, 160, 255};
        context.Render.DrawLine(Vector2f{pos.X - w, pos.Y - h * 0.6f}, Vector2f{pos.X + w, pos.Y - h * 0.6f}, outlineCol, 2.0f);
        context.Render.DrawLine(Vector2f{pos.X + w, pos.Y - h * 0.6f}, Vector2f{pos.X, pos.Y + h * 0.8f}, outlineCol, 2.0f);
        context.Render.DrawLine(Vector2f{pos.X, pos.Y + h * 0.8f}, Vector2f{pos.X - w, pos.Y - h * 0.6f}, outlineCol, 2.0f);

        // Point light glow
        context.Render.DrawPointLight(pos, 45.0f, Color{255, 60, 40, 200}, 0.8f, BlendMode::Add);
    }
}

void StoryScene::RenderPaddleDurabilityOverlay(GameContext& context)
{
    if (!m_registry.IsAlive(m_paddle) || m_isPaddleDestroyed ||
        !m_registry.HasComponent<Transform2D>(m_paddle) || !m_registry.HasComponent<BoxCollider>(m_paddle))
        return;

    const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddle);
    const Vector2f effSize = pCol.GetEffectiveSize(pTrans.Scale);

    // Paddle Visual Fracture Decals when damaged (lives < 3)
    if (m_lives < 3 && m_lives > 0)
    {
        Color crackCol = (m_lives == 1) ? Color{255, 50, 50, 220} : Color{220, 220, 220, 170};
        const float halfW = effSize.X * 0.5f;
        const float halfH = effSize.Y * 0.5f;

        // Render jagged crack fracture lines across paddle
        context.Render.DrawLine(Vector2f{pTrans.Position.X - halfW * 0.35f, pTrans.Position.Y - halfH},
                                Vector2f{pTrans.Position.X - halfW * 0.15f, pTrans.Position.Y + halfH}, crackCol, 2.0f);
        if (m_lives == 1)
        {
            context.Render.DrawLine(Vector2f{pTrans.Position.X + halfW * 0.25f, pTrans.Position.Y - halfH},
                                    Vector2f{pTrans.Position.X + halfW * 0.40f, pTrans.Position.Y + halfH}, crackCol, 2.0f);
        }
    }
}

void StoryScene::RenderBossHUD(GameContext& context)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    if (!m_registry.HasComponent<BossComponent>(m_bossEntity)) return;

    const auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
    const Vector2f viewSize = context.Render.GetLogicalViewSize();

    // Boss Name & Phase Title at top center
    const std::string phaseStr = (boss.Phase == BossPhase::Phase1) ? " [PHASE 1 // SHIELD MATRIX]" :
                                 (boss.Phase == BossPhase::Phase2) ? " [PHASE 2 // BEAM SWEEP]" :
                                 (boss.Phase == BossPhase::Phase3) ? " [PHASE 3 // ENRAGE STORM]" : " [DEFEATED]";
    const std::string title = boss.Name + phaseStr;
    context.Render.DrawTextCentered(title, m_fontId, 16.0f,
                                    Transform2D{Vector2f{viewSize.X * 0.5f, 22.0f}},
                                    Color{255, 80, 80, 255});

    // Multi-segment Boss Health Bar at top center
    const float barW = std::min(600.0f, viewSize.X - 200.0f);
    const float barH = 14.0f;
    const Vector2f barPos{viewSize.X * 0.5f, 44.0f};

    // Dark background container
    context.Render.DrawRectangle(barW, barH, Transform2D{barPos}, Color{20, 24, 35, 230});

    // Boss HP Fill
    const float hpRatio = boss.GetHealthRatio();
    Color fillCol = (boss.Phase == BossPhase::Phase3) ? Color{255, 30, 60, 255} :
                    (boss.Phase == BossPhase::Phase2) ? Color{255, 140, 30, 255} : Color{80, 200, 255, 255};
    if (hpRatio > 0.001f)
    {
        const float fillW = barW * hpRatio;
        const Vector2f fillPos{barPos.X - barW * 0.5f + fillW * 0.5f, barPos.Y};
        context.Render.DrawRectangle(fillW, barH, Transform2D{fillPos}, fillCol);
    }

    // Border
    const float left = barPos.X - barW * 0.5f;
    const float right = barPos.X + barW * 0.5f;
    const float top = barPos.Y - barH * 0.5f;
    const float bottom = barPos.Y + barH * 0.5f;
    context.Render.DrawLine(Vector2f{left, top}, Vector2f{right, top}, Color{60, 75, 105, 220}, 1.5f);
    context.Render.DrawLine(Vector2f{left, bottom}, Vector2f{right, bottom}, Color{60, 75, 105, 220}, 1.5f);
    context.Render.DrawLine(Vector2f{left, top}, Vector2f{left, bottom}, Color{60, 75, 105, 220}, 1.5f);
    context.Render.DrawLine(Vector2f{right, top}, Vector2f{right, bottom}, Color{60, 75, 105, 220}, 1.5f);

    // Phase threshold tick marks at 33% and 66%
    context.Render.DrawLine(Vector2f{left + barW * 0.33f, top},
                            Vector2f{left + barW * 0.33f, bottom}, Color{255, 255, 255, 180});
    context.Render.DrawLine(Vector2f{left + barW * 0.66f, top},
                            Vector2f{left + barW * 0.66f, bottom}, Color{255, 255, 255, 180});
}

void StoryScene::RenderBossWeakPoint(GameContext& context)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    if (!m_registry.HasComponent<BossComponent>(m_bossEntity) || !m_registry.HasComponent<Transform2D>(m_bossEntity))
        return;

    const auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);
    const auto& bTrans = m_registry.GetComponent<Transform2D>(m_bossEntity);
    if (boss.IsDefeated()) return;

    const Vector2f corePos = bTrans.Position + boss.WeakPoint.RelativeOffset;
    const float pulse = 1.0f + 0.15f * std::sin(boss.WeakPoint.PulseTimer);
    const float coreRadius = boss.WeakPoint.Radius * pulse;

    // Glowing core reactor
    Color coreCol = (boss.Phase == BossPhase::Phase3) ? Color{255, 40, 80, 230} : Color{255, 200, 40, 230};
    context.Render.DrawCircle(coreRadius, Transform2D{corePos}, coreCol);
    context.Render.DrawCircle(coreRadius * 0.5f, Transform2D{corePos}, Color{255, 255, 255, 255});
}

void StoryScene::RenderBossLasers(GameContext& context)
{
    if (m_bossEntity == NULL_ENTITY || !m_registry.IsAlive(m_bossEntity)) return;
    if (!m_registry.HasComponent<BossComponent>(m_bossEntity)) return;

    const auto& boss = m_registry.GetComponent<BossComponent>(m_bossEntity);

    for (const auto& beam : boss.LaserBeams)
    {
        if (beam.IsCharging)
        {
            // Translucent glowing telegraph guide line
            const float alphaRatio = 1.0f - (beam.ChargeTimer / beam.ChargeDuration);
            const uint8_t alpha = static_cast<uint8_t>(alphaRatio * 180.0f);
            context.Render.DrawLine(Vector2f{beam.StartX, -200.0f}, Vector2f{beam.StartX, 450.0f}, Color{255, 50, 50, alpha});
        }
        else if (beam.IsFiring)
        {
            // Blazing vertical laser beam
            const Vector2f beamCenter{beam.StartX, 125.0f};
            context.Render.DrawRectangle(beam.Width, 650.0f, Transform2D{beamCenter}, Color{255, 80, 80, 220});
            context.Render.DrawRectangle(beam.Width * 0.4f, 650.0f, Transform2D{beamCenter}, Color{255, 255, 255, 250});
        }
    }
}

void StoryScene::RenderDialogueModal(GameContext& context)
{
    const auto& chapter = m_chapters[m_currentChapterIndex];
    const auto& cards = m_isIntroDialogue ? chapter.IntroDialogue : chapter.OutroDialogue;
    if (m_dialogueCardIndex >= cards.size()) return;

    const auto& card = cards[m_dialogueCardIndex];

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float modalWidth = std::min(920.0f, viewSize.X - 120.0f);
    const float modalHeight = 145.0f;
    const float modalCenterX = viewSize.X * 0.5f;
    const float modalCenterY = viewSize.Y - 105.0f;
    const Vector2f modalPos{modalCenterX, modalCenterY};

    // Dark cinematic translucent backdrop
    context.Render.DrawRectangle(modalWidth, modalHeight, Transform2D{modalPos}, Color{12, 16, 26, 245});

    // Glowing accent border
    const float left = modalCenterX - modalWidth * 0.5f;
    const float right = modalCenterX + modalWidth * 0.5f;
    const float top = modalCenterY - modalHeight * 0.5f;
    const float bottom = modalCenterY + modalHeight * 0.5f;

    context.Render.DrawLine(Vector2f{left, top}, Vector2f{right, top}, card.SpeakerColor, 3.0f);
    context.Render.DrawLine(Vector2f{left, bottom}, Vector2f{right, bottom}, Color{40, 50, 70, 200}, 1.5f);
    context.Render.DrawLine(Vector2f{left, top}, Vector2f{left, bottom}, Color{40, 50, 70, 200}, 1.5f);
    context.Render.DrawLine(Vector2f{right, top}, Vector2f{right, bottom}, Color{40, 50, 70, 200}, 1.5f);

    // Speaker Name & Role Badge
    const std::string speakerHeader = card.SpeakerName + "  //  " + card.SpeakerRole;
    context.Render.DrawText(speakerHeader,
                            m_fontId, 16.0f,
                            Transform2D{Vector2f{left + 28.0f, top + 16.0f}},
                            card.SpeakerColor);

    // Typewriter narrative dialogue text
    std::string visibleText = card.Text.substr(0, m_dialogueVisibleChars);
    context.Render.DrawText(visibleText,
                            m_fontId, 15.0f,
                            Transform2D{Vector2f{left + 28.0f, top + 48.0f}},
                            Color{230, 240, 255, 255});

    // Advance / Skip prompt
    const std::string prompt = "[SPACE / ENTER / CLICK] Continue    [ESC] Skip";
    context.Render.DrawText(prompt,
                            m_fontId, 12.0f,
                            Transform2D{Vector2f{right - 300.0f, bottom - 24.0f}},
                            Color{150, 175, 210, 190});
}
