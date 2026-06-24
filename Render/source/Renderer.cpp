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

void Renderer::BeginDraw(Color clearColor)
{
    sf::Vector2u winSize = m_window.GetNative().getSize();
    if (m_renderTexture.getSize() != winSize && winSize.x > 0 && winSize.y > 0)
    {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 32;
        (void)m_renderTexture.resize(winSize, settings);
        m_renderTexture.setSmooth(true);
    }
    m_renderTexture.clear(sf::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a));
}

void Renderer::EndDraw(uint32_t postProcessShaderId)
{
    m_renderTexture.display();

    m_window.Clear();

    sf::Vector2u winSize = m_window.GetNative().getSize();
    m_window.GetNative().setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(winSize.x), static_cast<float>(winSize.y)})));
    
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
    float targetHeight = 1600.0f;
    sf::Vector2u winSize = m_window.GetNative().getSize();
    float aspect = static_cast<float>(winSize.x) / static_cast<float>(winSize.y);
    float targetWidth = targetHeight * aspect;

    sf::View view(sf::Vector2f(0.f, 0.f), sf::Vector2f(targetWidth, targetHeight));
    view.setCenter({camera.Position.X, camera.Position.Y});
    view.setRotation(sf::degrees(camera.Rotation));
    view.zoom(camera.Zoom);
    m_renderTexture.setView(view);
}

void Renderer::ResetCamera()
{
    float targetHeight = 1600.0f;
    sf::Vector2u winSize = m_window.GetNative().getSize();
    float aspect = static_cast<float>(winSize.x) / static_cast<float>(winSize.y);
    float targetWidth = targetHeight * aspect;

    sf::View view(sf::Vector2f(targetWidth / 2.0f, targetHeight / 2.0f), sf::Vector2f(targetWidth, targetHeight));
    m_renderTexture.setView(view);
}

Vector2f Renderer::GetLogicalViewSize() const
{
    float targetHeight = 1600.0f;
    sf::Vector2u winSize = m_window.GetNative().getSize();
    float aspect = static_cast<float>(winSize.x) / static_cast<float>(winSize.y);
    return Vector2f{targetHeight * aspect, targetHeight};
}

Vector2f Renderer::MapPixelToCoords(const Vector2f& pixelPos) const
{
    sf::Vector2f mapped = m_renderTexture.mapPixelToCoords(sf::Vector2i(static_cast<int>(pixelPos.X), static_cast<int>(pixelPos.Y)));
    return Vector2f{mapped.x, mapped.y};
}

Vector2f Renderer::MapCoordsToPixel(const Vector2f& coords) const
{
    sf::Vector2i mapped = m_renderTexture.mapCoordsToPixel(sf::Vector2f(coords.X, coords.Y));
    return Vector2f{static_cast<float>(mapped.x), static_cast<float>(mapped.y)};
}

void Renderer::DrawSprite(const SpriteComponent& spriteData, const Transform2D& transform, BlendMode blendMode)
{
    sf::Texture* tex = m_resources.Get<sf::Texture>(spriteData.TextureId);
    if (!tex) return;

    sf::Sprite sprite(*tex);

    if (spriteData.TextureRect)
    {
        sprite.setTextureRect(sf::IntRect({spriteData.TextureRect->Left, spriteData.TextureRect->Top}, {spriteData.TextureRect->Width, spriteData.TextureRect->Height}));
        if (spriteData.Origin) 
            sprite.setOrigin({spriteData.Origin->X, spriteData.Origin->Y});
        else 
            sprite.setOrigin({spriteData.TextureRect->Width / 2.0f, spriteData.TextureRect->Height / 2.0f});
    }
    else
    {
        if (spriteData.Origin) 
            sprite.setOrigin({spriteData.Origin->X, spriteData.Origin->Y});
        else 
            sprite.setOrigin({tex->getSize().x / 2.0f, tex->getSize().y / 2.0f});
    }

    ApplyTransform(sprite, transform);
    sprite.setColor(ToSfColor(spriteData.Tint));

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);

    if (spriteData.Shader.ShaderId != 0)
    {
        sf::Shader* shader = m_resources.Get<sf::Shader>(spriteData.Shader.ShaderId);
        if (shader)
        {
            shader->setUniform("color", sf::Glsl::Vec4(spriteData.Tint.r / 255.f, spriteData.Tint.g / 255.f, spriteData.Tint.b / 255.f, spriteData.Tint.a / 255.f));
            shader->setUniform("crack_amount", spriteData.Shader.ShaderValue);
            
            if (spriteData.Shader.OverlayTextureId != 0)
            {
                if (sf::Texture* crackTex = m_resources.Get<sf::Texture>(spriteData.Shader.OverlayTextureId))
                {
                    shader->setUniform("crack_texture", *crackTex);
                }
            }
            states.shader = shader;
        }
    }
    
    m_renderTexture.draw(sprite, states);
}

void Renderer::DrawCircle(float radius, const Transform2D& transform, Color color, BlendMode blendMode)
{
    sf::CircleShape circle(radius);
    circle.setOrigin({radius, radius});
    ApplyTransform(circle, transform);
    circle.setFillColor(ToSfColor(color));
    RenderItem(m_renderTexture, circle, blendMode);
}

void Renderer::DrawRectangle(float width, float height, const Transform2D& transform, Color color, BlendMode blendMode)
{
    sf::RectangleShape rect({width, height});
    rect.setOrigin({width / 2.0f, height / 2.0f});
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

Vector2f Renderer::GetTextSize(const std::string& text, uint32_t fontId, float fontSize) const
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        sf::FloatRect bounds = sfText.getLocalBounds();
        return {bounds.size.x, bounds.size.y};
    }
    return {0.0f, 0.0f};
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
