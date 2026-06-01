#pragma once
#include "System/Window.h"
#include "Resources/ResourceManager.h"
#include "Data/Color.h"
#include "ECS/Components/Transform2D.h"
#include <string>

#include "Data/BlendMode.h"
#include "ECS/Components/Camera2D.h"
#include "Data/PrimitiveType.h"
#include "Data/Vertex.h"

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
    void                DrawVertices(const std::vector<Vertex>& vertices, PrimitiveType type, uint32_t textureId = 0, BlendMode blendMode = BlendMode::Alpha);
    void                DrawRectangleOutline(float width, float height, const Transform2D& transform, Color color, float thickness = 2.0f);
    void                DrawCircleOutline(float radius, const Transform2D& transform, Color color, float thickness = 2.0f);

private:
    Window&             m_window;
    ResourceManager&    m_resources;
    sf::RenderTexture   m_renderTexture;
};
