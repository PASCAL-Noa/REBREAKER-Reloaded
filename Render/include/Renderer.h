#pragma once
#include "Window.h"
#include "ResourceManager.h"
#include "Color.h"
#include "Transform2D.h"
#include <string>

class Renderer
{
public:
    Renderer(Window& window, ResourceManager& resources);
    ~Renderer() = default;

    void                BeginDraw() const;
    void                EndDraw() const;

    void                DrawSprite(uint32_t textureId, const Transform2D& transform, Color color = Colors::White) const;
    void                DrawCircle(float radius, const Transform2D& transform, Color color = Colors::White) const;
    void                DrawRectangle(float width, float height, const Transform2D& transform, Color color = Colors::White) const;
    void                DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color = Colors::White) const;

private:
    Window&             m_window;
    ResourceManager&    m_resources;
};