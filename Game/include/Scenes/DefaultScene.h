#pragma once
#include "Core/Scene.h"
#include "ECS/Components/Camera2D.h"
#include <string>
#include <cstdint>

class DefaultScene : public Scene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;

protected:
    void    DrawDefaultUI(const GameContext& context, const std::string& sceneName, const std::string& instructions) const;
    void    PlaySfx(uint32_t id, float volume = 100.0f, float pitch = 1.0f) const;
    void    PlayMusic(const std::string& filepath, float volume = 100.0f, bool loop = true) const;
    void    StopMusic() const;
    void    StopAllAudio() const;

    GameContext*    mp_context = nullptr;
    Entity     m_camera{};
    uint32_t     m_fontId = 0;
};