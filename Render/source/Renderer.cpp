#include "Renderer.h"


static sf::Color ToSfColor(const Color& c)
{
    return {c.r, c.g, c.b, c.a};
}

Renderer::Renderer(Window& window, ResourceManager& resources)
    : m_window(window), m_resources(resources)
{
}

void Renderer::BeginDraw() const
{
    m_window.Clear();
}

void Renderer::EndDraw() const
{
    m_window.Display();
}

void Renderer::SetCamera(const Camera2D& camera) const
{
    sf::View view = m_window.GetNative().getDefaultView();

    view.setCenter({camera.X, camera.Y});
    view.setRotation(sf::degrees(camera.Rotation));
    view.zoom(camera.Zoom);

    m_window.GetNative().setView(view);
}

void Renderer::ResetCamera() const
{
    m_window.GetNative().setView(m_window.GetNative().getDefaultView());
}

void Renderer::DrawSprite(uint32_t textureId, const Transform2D& transform, Color color) const
{
    if (sf::Texture* tex = m_resources.Get<sf::Texture>(textureId))
    {
        sf::Sprite sprite(*tex);
        sprite.setPosition({transform.X, transform.Y});
        sprite.setRotation(sf::degrees(transform.Rotation));
        sprite.setScale({transform.ScaleX, transform.ScaleY});
        sprite.setColor(ToSfColor(color));

        m_window.GetNative().draw(sprite);
    }
}

void Renderer::DrawCircle(float radius, const Transform2D& transform, Color color) const
{
    sf::CircleShape circle(radius);
    circle.setPosition({transform.X, transform.Y});
    circle.setRotation(sf::degrees(transform.Rotation));
    circle.setScale({transform.ScaleX, transform.ScaleY});
    circle.setFillColor(ToSfColor(color));

    m_window.GetNative().draw(circle);
}

void Renderer::DrawRectangle(float width, float height, const Transform2D& transform, Color color) const
{
    sf::RectangleShape rect({width, height});
    rect.setPosition({transform.X, transform.Y});
    rect.setRotation(sf::degrees(transform.Rotation));
    rect.setScale({transform.ScaleX, transform.ScaleY});
    rect.setFillColor(ToSfColor(color));

    m_window.GetNative().draw(rect);
}

void Renderer::DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color) const
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        sfText.setPosition({transform.X, transform.Y});
        sfText.setRotation(sf::degrees(transform.Rotation));
        sfText.setScale({transform.ScaleX, transform.ScaleY});
        sfText.setFillColor(ToSfColor(color));

        m_window.GetNative().draw(sfText);
    }
}
