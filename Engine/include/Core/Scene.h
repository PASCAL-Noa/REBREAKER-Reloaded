#pragma once
#include "ECS/Registry.hpp"
#include "ECS/SystemManager.hpp"

struct GameContext;

class Scene
{
public:
    Scene() = default;
    virtual ~Scene() = default;

    virtual void                    OnInit(GameContext& context) {}
    virtual void                    OnUpdate(float dt, GameContext& context) {}
    virtual void                    OnRender(GameContext& context) {}
    virtual void                    OnDestroy(GameContext& context) {}
    [[nodiscard]] virtual uint32_t  GetPostProcessShader() const { return 0; }

protected:
    Registry        m_registry;
    SystemManager   m_systemManager;
};