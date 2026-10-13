#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "LeaderboardManager.h"
#include "LevelManager.h"

struct StarParticle
{
    float X = 0.0f;
    float Y = 0.0f;
    float Speed = 20.0f;
    float Size = 2.0f;
    float Alpha = 180.0f;
    float TwinkleFreq = 2.0f;
};

class MenuScene : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t GetPostProcessShader() const override;

private:
    void    InitRetroBackground(const GameContext& context);
    void    UpdateRetroBackground(float dt, const GameContext& context);
    void    RenderRetroBackground(const GameContext& context) const;

    void    CreateMainMenuUI(const GameContext& context);
    void    CreateSettingsLayout(const GameContext& context);
    void    CreateAudioTab(const GameContext& context);
    void    CreateRenderTab(const GameContext& context);
    void    CreateInputsTab(const GameContext& context);
    void    CreateGamerulesTab(const GameContext& context);
    void    CreateCheatsTab(const GameContext& context);
    void    OpenSettingsTab(Entity targetCanvas);

    void    CreateLevelSelectModal(const GameContext& context);
    void    CreateLeaderboardModal(const GameContext& context);
    void    CreateDebugModal(const GameContext& context);

    void    OpenModal(Entity modalCanvas);
    void    CloseModals();

    void    UpdateVolumeBars(const std::vector<Entity>& bars, float volume);

    Entity  m_mainCanvas = NULL_ENTITY;
    Entity  m_settingsCanvas = NULL_ENTITY;
    Entity  m_audioCanvas = NULL_ENTITY;
    Entity  m_renderCanvas = NULL_ENTITY;
    Entity  m_inputsCanvas = NULL_ENTITY;
    Entity  m_gamerulesCanvas = NULL_ENTITY;
    Entity  m_cheatsCanvas = NULL_ENTITY;
    Entity  m_activeTabCanvas = NULL_ENTITY;
    Entity  m_cheatsTabBtn = NULL_ENTITY;
    Entity  m_levelSelectCanvas = NULL_ENTITY;
    Entity  m_leaderboardCanvas = NULL_ENTITY;
    Entity  m_debugCanvas = NULL_ENTITY;
    Entity  m_activeModal = NULL_ENTITY;

    // Settings widgets
    Entity  m_fsBtn = NULL_ENTITY;
    Entity  m_vsyncBtn = NULL_ENTITY;
    Entity  m_shaderBtn = NULL_ENTITY;
    Entity  m_particlesBtn = NULL_ENTITY;
    std::vector<Entity> m_sfxVolumeBars;
    std::vector<Entity> m_musicVolumeBars;

    LeaderboardManager  m_leaderboard;
    LevelManager        m_levelManager;

    uint32_t            m_clickSfxId = 0;
    uint32_t            m_shaderId = 0;

    std::vector<StarParticle> m_stars;
    float               m_gridAnimOffset = 0.0f;
    float               m_animTime = 0.0f;
};