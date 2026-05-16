#pragma once
#include "Window.h"
#include "ResourceManager.h"
#include "Color.h"
#include "Transform2D.h"
#include <string>

#include "BlendMode.h"
#include "Camera2D.h"

class Renderer
{
public:
    Renderer(Window& window, ResourceManager& resources);
    ~Renderer() = default;

    void                BeginDraw();
    void                EndDraw(uint32_t postProcessShaderId = 0);

    void                SetCamera(const Camera2D& camera);
    void                ResetCamera();

    void                DrawSprite(uint32_t textureId, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawCircle(float radius, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawRectangle(float width, float height, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);

private:
    Window&             m_window;
    ResourceManager&    m_resources;
    sf::RenderTexture   m_renderTexture;
};
