#include "Scenes/ProfileScene.h"
#include "Scenes/AchievementScene.h"
#include "Scenes/MenuScene.h"
#include "UI/ProfileGarageState.h"
#include "UI/ProfileGarageInput.h"
#include "UI/ProfileGarageLayout.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/Camera2D.h"
#include "AudioMixer.h"
#include <algorithm>
#include <cmath>

ProfileScene::ProfileScene()
    : m_state(std::make_unique<ProfileGarageState>())
{
}

ProfileScene::~ProfileScene() = default;

void ProfileScene::Launch(SceneManager& scenes, SceneTransitionType type, float duration)
{
    scenes.LoadSceneWithTransition<ProfileScene>(type, duration);
}

void ProfileScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    m_bgColor = Color{11, 16, 26, 255};

    m_assets.FontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_assets.PaddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_assets.BallTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_switchSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_switch.wav");
    m_explosionSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_destroy.wav");

    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 0.8f, AudioMixer::kNominalMusicLevel, true);

    m_state->Initialize();

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    m_ballPos = ProfileGarageLayout::GetShowcaseBounds(viewSize).Center;
    m_cachedTrailStyle = m_state->GetPreviewTrailStyle();
    m_cachedTrailPri = m_state->GetPreviewPrimaryColor();
    m_cachedTrailSec = m_state->GetPreviewSecondaryColor();
    m_ballTrail.Clear();
    m_ballTrail.Config = CosmeticRenderer::BuildTrailConfig(m_cachedTrailStyle, m_cachedTrailPri, m_cachedTrailSec);
    m_explosions.Clear();
    m_autoBurstTimer = 0.7f;
    m_animTime = 0.0f;
}

void ProfileScene::OnDestroy(GameContext& context)
{
    ProfileManager::Get().SaveProfile();
    m_explosions.Clear();
    DefaultScene::OnDestroy(context);
}

void ProfileScene::OnUpdate(float dt, GameContext& context)
{
    m_animTime += dt;
    m_state->UpdateTimers(dt);

    if (m_state->IsEditingNickname())
    {
        Scene::OnUpdate(dt, context);
    }
    else
    {
        DefaultScene::OnUpdate(dt, context);
    }

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f mouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    const ProfileInputResult res = ProfileGarageInput::Process(*m_state, context.Input, viewSize, mouse);

    if (res.ReturnToMenu)
    {
        ReturnToMenu(context);
        return;
    }
    if (res.OpenAchievementShowcase)
    {
        if (res.PlayClickSfx && m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
        AchievementScene::Launch(context.Scenes);
        return;
    }
    if (res.PlayClickSfx && m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
    if (res.PlaySwitchSfx && m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
    if (res.PlayUnlockSfx && m_explosionSfxId != 0) context.Audio.PlaySfx(m_explosionSfxId, 21.0f, 1.2f);
    if (res.PlayErrorSfx && m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f, 0.72f);
    if (res.TriggerDeathBurst)
    {
        SpawnShowcaseBurst(res.BurstPos, 1.3f);
        m_autoBurstTimer = 1.65f;
    }

    UpdateShowcase(dt, viewSize);
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

    ProfileShowcaseSnapshot snapshot;
    snapshot.AnimTime = m_animTime;
    snapshot.BallPos = m_ballPos;
    snapshot.Trail = &m_ballTrail;
    snapshot.Explosions = &m_explosions;
    snapshot.TrailScratch = &m_trailScratch;

    ProfileGarageRenderer::Render(context.Render, *m_state, m_assets, snapshot, viewSize, mouse);
}

void ProfileScene::UpdateShowcase(float dt, const Vector2f& viewSize)
{
    const float clampedDt = std::min(dt, 0.05f);
    const UIRect bounds = ProfileGarageLayout::GetShowcaseBounds(viewSize);

    const float phase = m_animTime * 2.1f;
    m_ballPos = Vector2f{
        bounds.Center.X + std::sin(phase) * (bounds.Size.X * 0.34f),
        bounds.Center.Y + std::sin(phase * 2.0f) * (bounds.Size.Y * 0.16f)
    };

    const TrailStyle previewTrail = m_state->GetPreviewTrailStyle();
    const Color previewPri = m_state->GetPreviewPrimaryColor();
    const Color previewSec = m_state->GetPreviewSecondaryColor();
    if (previewTrail != m_cachedTrailStyle ||
        previewPri.r != m_cachedTrailPri.r || previewPri.g != m_cachedTrailPri.g || previewPri.b != m_cachedTrailPri.b ||
        previewSec.r != m_cachedTrailSec.r || previewSec.g != m_cachedTrailSec.g || previewSec.b != m_cachedTrailSec.b)
    {
        m_cachedTrailStyle = previewTrail;
        m_cachedTrailPri = previewPri;
        m_cachedTrailSec = previewSec;
        m_ballTrail.Config = CosmeticRenderer::BuildTrailConfig(previewTrail, previewPri, previewSec);
    }

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

    if (!m_state->IsColorModalOpen() && m_state->GetActiveSlot() == CosmeticSlot::DeathEffect)
    {
        m_autoBurstTimer -= clampedDt;
        if (m_autoBurstTimer <= 0.0f)
        {
            m_autoBurstTimer = 1.65f;
            SpawnShowcaseBurst(bounds.Center, 1.3f);
        }
    }

    m_explosions.Update(clampedDt);
}

void ProfileScene::SpawnShowcaseBurst(const Vector2f& pos, float scale)
{
    m_explosions.Spawn(
        pos,
        m_state->GetPreviewDeathStyle(),
        m_state->GetPreviewPrimaryColor(),
        m_state->GetPreviewSecondaryColor(),
        scale);
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
