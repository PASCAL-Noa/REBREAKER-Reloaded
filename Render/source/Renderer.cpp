#include "Graphics/Renderer.h"

static sf::Color ToSfColor(const Color& c)
{
    return {c.r, c.g, c.b, c.a};
}

static sf::BlendMode ToSfBlendMode(const BlendMode& mode)
{
    return (mode == BlendMode::Add) ? sf::BlendAdd : sf::BlendAlpha;
}

static sf::PrimitiveType ToSfPrimitiveType(const PrimitiveType type)
{
    switch (type)
    {
        case PrimitiveType::Points:         return sf::PrimitiveType::Points;
        case PrimitiveType::Lines:          return sf::PrimitiveType::Lines;
        case PrimitiveType::LineStrip:      return sf::PrimitiveType::LineStrip;
        case PrimitiveType::Triangles:      return sf::PrimitiveType::Triangles;
        case PrimitiveType::TriangleStrip:  return sf::PrimitiveType::TriangleStrip;
        default:                            return sf::PrimitiveType::Triangles;
    }
}

static void ApplyTransform(sf::Transformable& target, const Transform2D& transform)
{
    target.setPosition({transform.Position.X, transform.Position.Y});
    target.setRotation(sf::degrees(transform.Rotation));
    target.setScale({transform.Scale.X, transform.Scale.Y});
}

static void RenderItem(sf::RenderTexture& texture, const sf::Drawable& drawable, BlendMode mode)
{
    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(mode);
    texture.draw(drawable, states);
}

Renderer::Renderer(Window& window, ResourceManager& resources) : m_window(window), m_resources(resources)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 32;

    (void)m_renderTexture.resize(m_window.GetNative().getSize(), settings);

    m_renderTexture.setSmooth(true);
}

void Renderer::BeginDraw()
{
    m_renderTexture.clear();
}

void Renderer::EndDraw(uint32_t postProcessShaderId)
{
    m_renderTexture.display();

    m_window.Clear();
    sf::Sprite renderSprite(m_renderTexture.getTexture());

    if (postProcessShaderId != 0 && m_resources.Get<sf::Shader>(postProcessShaderId) != nullptr)
    {
        sf::RenderStates states;
        states.shader = m_resources.Get<sf::Shader>(postProcessShaderId);
        m_window.GetNative().draw(renderSprite, states);
    }
    else
    {
        m_window.GetNative().draw(renderSprite);
    }
    m_window.Display();
}

void Renderer::SetCamera(const Camera2D& camera)
{
    sf::View view = m_renderTexture.getDefaultView();
    view.setCenter({camera.Position.X, camera.Position.Y});
    view.setRotation(sf::degrees(camera.Rotation));
    view.zoom(camera.Zoom);
    m_renderTexture.setView(view);
}

void Renderer::ResetCamera()
{
    m_renderTexture.setView(m_renderTexture.getDefaultView());
}

void Renderer::DrawSprite(uint32_t textureId, const Transform2D& transform, Color color, BlendMode blendMode, uint32_t shaderId, uint32_t overlayTexId, float shaderValue)
{
    if (sf::Texture* tex = m_resources.Get<sf::Texture>(textureId))
    {
        sf::Sprite sprite(*tex);

        sprite.setOrigin({tex->getSize().x / 2.0f, tex->getSize().y / 2.0f});

        ApplyTransform(sprite, transform);
        sprite.setColor(ToSfColor(color));

        sf::RenderStates states;
        states.blendMode = ToSfBlendMode(blendMode);
        if (shaderId != 0)
        {
            sf::Shader* shader = m_resources.Get<sf::Shader>(shaderId);
            if (shader)
            {
                shader->setUniform("color", sf::Glsl::Vec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
                shader->setUniform("crack_amount", shaderValue);
                if (overlayTexId != 0)
                {
                    sf::Texture* crackTex = m_resources.Get<sf::Texture>(overlayTexId);
                    if (crackTex)
                    {
                        shader->setUniform("crack_texture", *crackTex);
                    }
                }
                states.shader = shader;
            }
        }
        m_renderTexture.draw(sprite, states);
    }
}

void Renderer::DrawCircle(float radius, const Transform2D& transform, Color color, BlendMode blendMode)
{
    sf::CircleShape circle(radius);
    ApplyTransform(circle, transform);
    circle.setFillColor(ToSfColor(color));
    RenderItem(m_renderTexture, circle, blendMode);
}

void Renderer::DrawRectangle(float width, float height, const Transform2D& transform, Color color, BlendMode blendMode)
{
    sf::RectangleShape rect({width, height});
    ApplyTransform(rect, transform);
    rect.setFillColor(ToSfColor(color));
    RenderItem(m_renderTexture, rect, blendMode);
}

void Renderer::DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color, BlendMode blendMode)
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        ApplyTransform(sfText, transform);
        sfText.setFillColor(ToSfColor(color));
        RenderItem(m_renderTexture, sfText, blendMode);
    }
}

void Renderer::DrawVertices(const std::vector<Vertex> &vertices, PrimitiveType type, uint32_t textureId, BlendMode blendMode)
{
    if (vertices.empty()) return;

    std::vector<sf::Vertex> sfVertices;
    sfVertices.reserve(vertices.size());

    for (const auto& v : vertices)
    {
        sfVertices.emplace_back(sf::Vector2f{v.X, v.Y}, ToSfColor(v.Color), sf::Vector2f{v.U, v.V});
    }

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);

    if (textureId != 0)
    {
        states.texture = m_resources.Get<sf::Texture>(textureId);
    }

    m_renderTexture.draw(sfVertices.data(), sfVertices.size(), ToSfPrimitiveType(type), states);
}

void Renderer::DrawRectangleOutline(float width, float height, const Transform2D& transform, Color color, float thickness)
{
    sf::RectangleShape rect({width, height});

    rect.setOrigin({width / 2.0f, height / 2.0f});

    ApplyTransform(rect, transform);
    rect.setFillColor(sf::Color::Transparent);
    rect.setOutlineColor(ToSfColor(color));
    rect.setOutlineThickness(thickness);
    RenderItem(m_renderTexture, rect, BlendMode::Alpha);
}

void Renderer::DrawCircleOutline(float radius, const Transform2D& transform, Color color, float thickness)
{
    sf::CircleShape circle(radius);
    
    circle.setOrigin({radius, radius});

    ApplyTransform(circle, transform);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineColor(ToSfColor(color));
    circle.setOutlineThickness(thickness);
    RenderItem(m_renderTexture, circle, BlendMode::Alpha);
}

void Renderer::DrawLine(const Vector2f& p1, const Vector2f& p2, Color color, float thickness)
{
    float length = std::sqrt(std::pow(p2.X - p1.X, 2) + std::pow(p2.Y - p1.Y, 2));
    float angle = std::atan2(p2.Y - p1.Y, p2.X - p1.X) * 180.0f / 3.14159265f;

    sf::RectangleShape line({length, thickness});
    line.setOrigin({0.0f, thickness / 2.0f});
    line.setPosition({p1.X, p1.Y});
    line.setRotation(sf::degrees(angle));
    line.setFillColor(ToSfColor(color));

    m_renderTexture.draw(line);
}

Vector2f Renderer::GetTextureSize(uint32_t textureId) const
{
    if (sf::Texture* tex = m_resources.Get<sf::Texture>(textureId))
    {
        return Vector2f{static_cast<float>(tex->getSize().x), static_cast<float>(tex->getSize().y)};
    }
    return Vector2f{0.0f, 0.0f};
}
