#include "Core/SceneManager.h"

SceneManager::~SceneManager()
{
    delete m_currentScene;
    delete m_nextScene;
}

void SceneManager::Shutdown(GameContext& context)
{
    if (m_currentScene)
    {
        m_currentScene->OnDestroy(context);
        delete m_currentScene;
        m_currentScene = nullptr;
    }
    if (m_nextScene)
    {
        delete m_nextScene;
        m_nextScene = nullptr;
    }
}

void SceneManager::Update(float dt, GameContext& context)
{
    if (m_nextScene)
    {
        if (m_currentScene)
        {
            m_currentScene->OnDestroy(context);
            delete m_currentScene;
        }
        m_currentScene = m_nextScene;
        m_nextScene = nullptr;

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

