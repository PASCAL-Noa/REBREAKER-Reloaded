#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Camera2D.h"
#include "ECS/Components/SpriteComponent.h"

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

static constexpr float kLogicalHeight = 1600.0f;
static constexpr float kMinAspect     = 16.0f / 10.0f; // 1.6f (16:10)
static constexpr float kMaxAspect     = 16.0f / 9.0f;  // ~1.7777778f (16:9)

Renderer::Renderer(Window& window, ResourceManager& resources) : m_window(window), m_resources(resources)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 4;

    (void)m_renderTexture.resize(m_window.GetNative().getSize(), settings);
    m_renderTexture.setSmooth(true);
    ResetCamera();
}

void Renderer::BeginDraw(Color clearColor)
{
    sf::Vector2u winSize = m_window.GetNative().getSize();
    if (m_renderTexture.getSize() != winSize && winSize.x > 0 && winSize.y > 0)
    {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        (void)m_renderTexture.resize(winSize, settings);
        m_renderTexture.setSmooth(true);
        ResetCamera();
    }
    m_renderTexture.clear(sf::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a));
}

void Renderer::EndDraw(uint32_t postProcessShaderId)
{
    m_renderTexture.display();

    m_window.Clear();

    const sf::Vector2u winSize = m_window.GetNative().getSize();
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

    DrawLetterboxBars(m_window.GetNative(), winSize);

    m_window.Display();
}

void Renderer::DrawLetterboxBars(sf::RenderTarget& target, const sf::Vector2u& winSize) const
{
    if (winSize.x == 0 || winSize.y == 0) return;

    const sf::FloatRect vp = GetLetterboxViewport();
    if (vp.position.x <= 0.0001f && vp.position.y <= 0.0001f && vp.size.x >= 0.9999f && vp.size.y >= 0.9999f)
        return;

    const sf::View barView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(winSize.x), static_cast<float>(winSize.y)}));
    target.setView(barView);

    const float w = static_cast<float>(winSize.x);
    const float h = static_cast<float>(winSize.y);

    if (vp.position.y > 0.0f)
    {
        // Top and bottom bars (Letterbox)
        const float topBarHeight = vp.position.y * h;
        sf::RectangleShape topBar(sf::Vector2f(w, topBarHeight));
        topBar.setPosition({0.0f, 0.0f});
        topBar.setFillColor(sf::Color::Black);
        target.draw(topBar);

        const float bottomBarTop = (vp.position.y + vp.size.y) * h;
        const float bottomBarHeight = h - bottomBarTop;
        if (bottomBarHeight > 0.0f)
        {
            sf::RectangleShape bottomBar(sf::Vector2f(w, bottomBarHeight));
            bottomBar.setPosition({0.0f, bottomBarTop});
            bottomBar.setFillColor(sf::Color::Black);
            target.draw(bottomBar);
        }
    }
    else if (vp.position.x > 0.0f)
    {
        // Left and right bars (Pillarbox)
        const float leftBarWidth = vp.position.x * w;
        sf::RectangleShape leftBar(sf::Vector2f(leftBarWidth, h));
        leftBar.setPosition({0.0f, 0.0f});
        leftBar.setFillColor(sf::Color::Black);
        target.draw(leftBar);

        const float rightBarLeft = (vp.position.x + vp.size.x) * w;
        const float rightBarWidth = w - rightBarLeft;
        if (rightBarWidth > 0.0f)
        {
            sf::RectangleShape rightBar(sf::Vector2f(rightBarWidth, h));
            rightBar.setPosition({rightBarLeft, 0.0f});
            rightBar.setFillColor(sf::Color::Black);
            target.draw(rightBar);
        }
    }
}

void Renderer::OnWindowResized(unsigned int width, unsigned int height)
{
    if (width > 0 && height > 0)
    {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        (void)m_renderTexture.resize({width, height}, settings);
        m_renderTexture.setSmooth(true);
        ResetCamera();
    }
}

float Renderer::GetEffectiveAspectRatio() const
{
    const sf::Vector2u winSize = m_window.GetNative().getSize();
    if (winSize.y == 0) return kMinAspect;
    const float aspect = static_cast<float>(winSize.x) / static_cast<float>(winSize.y);
    if (aspect < kMinAspect) return kMinAspect;
    if (aspect > kMaxAspect) return kMaxAspect;
    return aspect;
}

sf::FloatRect Renderer::GetLetterboxViewport() const
{
    const sf::Vector2u winSize = m_window.GetNative().getSize();
    if (winSize.x == 0 || winSize.y == 0)
        return sf::FloatRect({0.0f, 0.0f}, {1.0f, 1.0f});

    const float aspect = static_cast<float>(winSize.x) / static_cast<float>(winSize.y);

    if (aspect < kMinAspect)
    {
        // Window is narrower/taller than 16:10 -> Letterbox (top & bottom bars)
        const float vpHeight = aspect / kMinAspect;
        const float vpTop = (1.0f - vpHeight) * 0.5f;
        return sf::FloatRect({0.0f, vpTop}, {1.0f, vpHeight});
    }
    if (aspect > kMaxAspect)
    {
        // Window is wider than 16:9 -> Pillarbox (left & right bars)
        const float vpWidth = kMaxAspect / aspect;
        const float vpLeft = (1.0f - vpWidth) * 0.5f;
        return sf::FloatRect({vpLeft, 0.0f}, {vpWidth, 1.0f});
    }

    // Between 16:10 and 16:9 -> Full viewport
    return sf::FloatRect({0.0f, 0.0f}, {1.0f, 1.0f});
}

void Renderer::SetCamera(const Camera2D& camera)
{
    const Vector2f viewSize = GetLogicalViewSize();
    sf::View view(sf::Vector2f(0.f, 0.f), sf::Vector2f(viewSize.X, viewSize.Y));
    view.setCenter({camera.Position.X, camera.Position.Y});
    view.setRotation(sf::degrees(camera.Rotation));
    view.zoom(camera.Zoom);
    view.setViewport(GetLetterboxViewport());
    m_renderTexture.setView(view);
}

void Renderer::ResetCamera()
{
    const Vector2f viewSize = GetLogicalViewSize();
    sf::View view(sf::Vector2f(viewSize.X / 2.0f, viewSize.Y / 2.0f), sf::Vector2f(viewSize.X, viewSize.Y));
    view.setViewport(GetLetterboxViewport());
    m_renderTexture.setView(view);
}

Vector2f Renderer::GetLogicalViewSize() const
{
    return Vector2f{kLogicalHeight * GetEffectiveAspectRatio(), kLogicalHeight};
}

Vector2f Renderer::MapPixelToCoords(const Vector2f& pixelPos) const
{
    const Vector2f viewSize = GetLogicalViewSize();
    sf::View uiView(sf::Vector2f(viewSize.X / 2.0f, viewSize.Y / 2.0f), sf::Vector2f(viewSize.X, viewSize.Y));
    uiView.setViewport(GetLetterboxViewport());
    const sf::Vector2f mapped = m_renderTexture.mapPixelToCoords(
        sf::Vector2i(static_cast<int>(pixelPos.X), static_cast<int>(pixelPos.Y)),
        uiView
    );
    return Vector2f{mapped.x, mapped.y};
}

Vector2f Renderer::MapCoordsToPixel(const Vector2f& coords) const
{
    const sf::Vector2i mapped = m_renderTexture.mapCoordsToPixel(sf::Vector2f(coords.X, coords.Y));
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

void Renderer::DrawScannerEffect(float width, float height, const Transform2D& transform, Color color, float time, uint32_t shaderId)
{
    sf::RectangleShape rect({width, height});
    rect.setOrigin({width / 2.0f, height / 2.0f});
    ApplyTransform(rect, transform);
    rect.setFillColor(sf::Color::White);

    sf::RenderStates states;
    states.blendMode = sf::BlendAlpha;

    if (shaderId != 0)
    {
        if (sf::Shader* shader = m_resources.Get<sf::Shader>(shaderId))
        {
            shader->setUniform("u_progress", time);
            shader->setUniform("u_color", sf::Glsl::Vec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
            shader->setUniform("u_resolution", sf::Glsl::Vec2(width, height));
            states.shader = shader;
        }
    }

    m_renderTexture.draw(rect, states);
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

    m_vertexScratch.clear();
    m_vertexScratch.reserve(vertices.size());

    for (const auto& v : vertices)
    {
        m_vertexScratch.emplace_back(sf::Vector2f{v.X, v.Y}, ToSfColor(v.Color), sf::Vector2f{v.U, v.V});
    }

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);

    if (textureId != 0)
    {
        states.texture = m_resources.Get<sf::Texture>(textureId);
    }

    m_renderTexture.draw(m_vertexScratch.data(), m_vertexScratch.size(), ToSfPrimitiveType(type), states);
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
