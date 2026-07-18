#include "Core/SceneManager.h"

SceneManager::~SceneManager()
{

}

void SceneManager::Shutdown(GameContext& context)
{
    if (m_currentScene)
    {
        m_currentScene->OnDestroy(context);
        m_currentScene.reset();
    }
    if (m_nextScene)
    {
        m_nextScene.reset();
    }
}

void SceneManager::Update(float dt, GameContext& context)
{
    if (m_nextScene)
    {
        if (m_currentScene)
        {
            m_currentScene->OnDestroy(context);
        }
        m_currentScene = std::move(m_nextScene);

        m_currentScene->OnInit(context);
    }

    if (m_currentScene)
    {
        m_currentScene->OnUpdate(dt, context);
    }
}

void SceneManager::Render(GameContext& context) const
{
    if (m_currentScene)
    {
        m_currentScene->OnRender(context);
    }
}

uint32_t SceneManager::GetPostProcessShader() const
{
    return m_currentScene ? m_currentScene->GetPostProcessShader() : 0;
}

Color SceneManager::GetClearColor() const
{
    return m_currentScene ? m_currentScene->GetClearColor() : Colors::Black;
}


