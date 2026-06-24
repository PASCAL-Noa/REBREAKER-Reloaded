#pragma once
#include "Scene.h"
#include "Data/Color.h"

class SceneManager
{
public:
    SceneManager() = default;

    ~SceneManager();

    void Shutdown(GameContext& context);

    template <typename T, typename... Args>
    void LoadScene(Args&&... args);

    void Update(float dt, GameContext& context);
    void Render(GameContext& context) const;

    [[nodiscard]] uint32_t GetPostProcessShader() const;
    [[nodiscard]] Color GetClearColor() const;

private:
    Scene* m_currentScene = nullptr;
    Scene* m_nextScene = nullptr;
};

template <typename T, typename ... Args>
void SceneManager::LoadScene(Args&&... args)
{
    if (m_nextScene) delete m_nextScene;
    m_nextScene = new T(std::forward<Args>(args)...);
}
