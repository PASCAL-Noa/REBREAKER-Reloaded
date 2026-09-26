#pragma once
#include "Scene.h"
#include "Data/Color.h"

#include <memory>

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
    std::unique_ptr<Scene> m_currentScene = nullptr;
    std::unique_ptr<Scene> m_nextScene = nullptr;
};

template <typename T, typename ... Args>
void SceneManager::LoadScene(Args&&... args)
{
    m_nextScene = std::make_unique<T>(std::forward<Args>(args)...);
}
