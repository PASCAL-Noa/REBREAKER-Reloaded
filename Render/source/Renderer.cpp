#include "Renderer.h"


static sf::Color ToSfColor(const Color& c)
{
    return {c.r, c.g, c.b, c.a};
}

Renderer::Renderer(Window& window, ResourceManager& resources)
    : m_window(window), m_resources(resources)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 32;

    m_renderTexture.resize(m_window.GetNative().getSize(), settings);

    m_renderTexture.setSmooth(true);
}

void Renderer::BeginDraw()
{
    m_renderTexture.clear();
}

void Renderer::EndDraw()
{
    m_renderTexture.display();

    m_window.Clear();
    sf::Sprite renderSprite(m_renderTexture.getTexture());
    m_window.GetNative().draw(renderSprite);
    m_window.Display();
}

void Renderer::SetCamera(const Camera2D& camera)
{
    sf::View view = m_renderTexture.getDefaultView();
    view.setCenter({camera.X, camera.Y});
    view.setRotation(sf::degrees(camera.Rotation));
    view.zoom(camera.Zoom);
    m_renderTexture.setView(view);
}

void Renderer::ResetCamera()
{
    m_renderTexture.setView(m_renderTexture.getDefaultView());
}

void Renderer::DrawSprite(uint32_t textureId, const Transform2D& transform, Color color)
{
    if (sf::Texture* texture = m_resources.Get<sf::Texture>(textureId))
    {
        sf::Sprite sprite(*texture);
        sprite.setPosition({transform.X, transform.Y});
        sprite.setRotation(sf::degrees(transform.Rotation));
        sprite.setScale({transform.ScaleX, transform.ScaleY});
        sprite.setColor(ToSfColor(color));

        m_renderTexture.draw(sprite);
    }
}

void Renderer::DrawCircle(float radius, const Transform2D& transform, Color color)
{
    sf::CircleShape circle(radius);
    circle.setPosition({transform.X, transform.Y});
    circle.setRotation(sf::degrees(transform.Rotation));
    circle.setScale({transform.ScaleX, transform.ScaleY});
    circle.setFillColor(ToSfColor(color));

    m_renderTexture.draw(circle);
}

void Renderer::DrawRectangle(float width, float height, const Transform2D& transform, Color color)
{
    sf::RectangleShape rect({width, height});
    rect.setPosition({transform.X, transform.Y});
    rect.setRotation(sf::degrees(transform.Rotation));
    rect.setScale({transform.ScaleX, transform.ScaleY});
    rect.setFillColor(ToSfColor(color));

    m_renderTexture.draw(rect);
}

void Renderer::DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color)
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        sfText.setPosition({transform.X, transform.Y});
        sfText.setRotation(sf::degrees(transform.Rotation));
        sfText.setScale({transform.ScaleX, transform.ScaleY});
        sfText.setFillColor(ToSfColor(color));

        m_renderTexture.draw(sfText);
    }
}
