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
    switch (mode)
    {
        case BlendMode::Add:      return sf::BlendAdd;
        case BlendMode::Multiply: return sf::BlendMultiply;
        case BlendMode::Alpha:
        default:                  return sf::BlendAlpha;
    }
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
    (void)m_lightMapTexture.resize(m_window.GetNative().getSize(), settings);
    m_lightMapTexture.setSmooth(true);
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
        (void)m_lightMapTexture.resize(winSize, settings);
        m_lightMapTexture.setSmooth(true);
        ResetCamera();
    }
    m_renderTexture.clear(sf::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a));
}

void Renderer::EndDraw(uint32_t postProcessShaderId, const PostProcessParams& params)
{
    m_renderTexture.display();

    m_window.Clear();

    const sf::Vector2u winSize = m_window.GetNative().getSize();
    m_window.GetNative().setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(winSize.x), static_cast<float>(winSize.y)})));
    
    sf::Sprite renderSprite(m_renderTexture.getTexture());

    if (postProcessShaderId != 0 && m_resources.Get<sf::Shader>(postProcessShaderId) != nullptr)
    {
        sf::Shader* shader = m_resources.Get<sf::Shader>(postProcessShaderId);
        shader->setUniform("screen_texture", sf::Shader::CurrentTexture);
        shader->setUniform("u_ca_intensity", params.ChromaticAberration);
        shader->setUniform("u_vignetteIntensity", params.VignetteIntensity);
        shader->setUniform("u_vignetteColor", sf::Glsl::Vec3(
            params.VignetteColor.r / 255.f,
            params.VignetteColor.g / 255.f,
            params.VignetteColor.b / 255.f
        ));
        shader->setUniform("u_flashIntensity", params.FlashIntensity);
        shader->setUniform("u_flashColor", sf::Glsl::Vec3(
            params.FlashColor.r / 255.f,
            params.FlashColor.g / 255.f,
            params.FlashColor.b / 255.f
        ));

        sf::RenderStates states;
        states.shader = shader;
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
        (void)m_lightMapTexture.resize({width, height}, settings);
        m_lightMapTexture.setSmooth(true);
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

void Renderer::DrawTextCentered(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color, BlendMode blendMode)
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        const sf::FloatRect bounds = sfText.getLocalBounds();
        sfText.setOrigin({bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + bounds.size.y * 0.5f});
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
        m_vertexScratch.emplace_back(sf::Vector2f{v.X, v.Y}, ToSfColor(v.Tint), sf::Vector2f{v.U, v.V});
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
    float dx = p2.X - p1.X;
    float dy = p2.Y - p1.Y;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.0f / 3.14159265f;

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

void Renderer::DrawSceneTransition(SceneTransitionType type, float progress, Color color)
{
    if (type == SceneTransitionType::None || progress <= 0.0001f)
    {
        return;
    }

    progress = std::clamp(progress, 0.0f, 1.0f);

    const sf::View prevView = m_renderTexture.getView();
    const Vector2f viewSize = GetLogicalViewSize();
    sf::View overlayView(sf::Vector2f(viewSize.X / 2.0f, viewSize.Y / 2.0f), sf::Vector2f(viewSize.X, viewSize.Y));
    overlayView.setViewport(GetLetterboxViewport());
    m_renderTexture.setView(overlayView);

    if (type == SceneTransitionType::FadeBlack || type == SceneTransitionType::FadeWhite || type == SceneTransitionType::CrossFade)
    {
        Color fadeCol = color;
        if (type == SceneTransitionType::FadeBlack) fadeCol = Colors::Black;
        else if (type == SceneTransitionType::FadeWhite) fadeCol = Colors::White;

        const auto alpha = static_cast<uint8_t>(std::clamp(progress * 255.0f, 0.0f, 255.0f));
        sf::RectangleShape rect(sf::Vector2f(viewSize.X, viewSize.Y));
        rect.setPosition({0.0f, 0.0f});
        rect.setFillColor(sf::Color(fadeCol.r, fadeCol.g, fadeCol.b, alpha));
        m_renderTexture.draw(rect);
    }
    else if (type == SceneTransitionType::IrisWipe)
    {
        const float cx = viewSize.X * 0.5f;
        const float cy = viewSize.Y * 0.5f;
        const float maxRadius = std::sqrt(cx * cx + cy * cy) + 40.0f;
        const float irisRadius = maxRadius * (1.0f - progress);

        if (irisRadius <= 1.0f || progress >= 0.999f)
        {
            // Fully covered: solid rectangle
            sf::RectangleShape rect(sf::Vector2f(viewSize.X, viewSize.Y));
            rect.setPosition({0.0f, 0.0f});
            rect.setFillColor(ToSfColor(color));
            m_renderTexture.draw(rect);
        }
        else
        {
            // Ring of triangles: outer edge covers whole viewport, inner circle is the aperture
            constexpr int kSegments = 64;
            const float outerRadius = maxRadius * 1.3f;
            std::vector<sf::Vertex> ringVertices;
            ringVertices.reserve((kSegments + 1) * 2);

            const sf::Color sfCol = ToSfColor(color);
            constexpr float kPi = 3.14159265f;

            for (int i = 0; i <= kSegments; ++i)
            {
                const float angle = static_cast<float>(i) * (2.0f * kPi / static_cast<float>(kSegments));
                const float cosA = std::cos(angle);
                const float sinA = std::sin(angle);

                ringVertices.emplace_back(sf::Vector2f(cx + irisRadius * cosA, cy + irisRadius * sinA), sfCol);
                ringVertices.emplace_back(sf::Vector2f(cx + outerRadius * cosA, cy + outerRadius * sinA), sfCol);
            }

            m_renderTexture.draw(ringVertices.data(), ringVertices.size(), sf::PrimitiveType::TriangleStrip);

            // Subtle luminous cyan iris rim
            sf::CircleShape irisBorder(irisRadius);
            irisBorder.setOrigin({irisRadius, irisRadius});
            irisBorder.setPosition({cx, cy});
            irisBorder.setFillColor(sf::Color::Transparent);
            irisBorder.setOutlineThickness(2.5f);
            irisBorder.setOutlineColor(sf::Color(0, 220, 255, static_cast<uint8_t>(180 * (1.0f - progress))));
            m_renderTexture.draw(irisBorder);
        }
    }
    else if (type == SceneTransitionType::CurtainWipe)
    {
        const float halfW = viewSize.X * 0.5f;
        const float curtainW = halfW * progress;

        // Left curtain
        sf::RectangleShape left(sf::Vector2f(curtainW, viewSize.Y));
        left.setPosition({0.0f, 0.0f});
        left.setFillColor(ToSfColor(color));
        m_renderTexture.draw(left);

        // Right curtain
        sf::RectangleShape right(sf::Vector2f(curtainW, viewSize.Y));
        right.setPosition({viewSize.X - curtainW, 0.0f});
        right.setFillColor(ToSfColor(color));
        m_renderTexture.draw(right);

        // Neon border strip along meeting edges
        if (curtainW > 0.0f && progress < 0.999f)
        {
            sf::RectangleShape leftBorder(sf::Vector2f(2.0f, viewSize.Y));
            leftBorder.setPosition({curtainW - 2.0f, 0.0f});
            leftBorder.setFillColor(sf::Color(0, 230, 255, 200));
            m_renderTexture.draw(leftBorder);

            sf::RectangleShape rightBorder(sf::Vector2f(2.0f, viewSize.Y));
            rightBorder.setPosition({viewSize.X - curtainW, 0.0f});
            rightBorder.setFillColor(sf::Color(0, 230, 255, 200));
            m_renderTexture.draw(rightBorder);
        }
    }

    m_renderTexture.setView(prevView);
}

void Renderer::BeginLightingPass(Color ambientDarkness)
{
    // Match current camera view and clear to ambient darkness level
    m_lightMapTexture.setView(m_renderTexture.getView());
    m_lightMapTexture.clear(sf::Color(ambientDarkness.r, ambientDarkness.g, ambientDarkness.b, ambientDarkness.a));
}

void Renderer::DrawPointLight(const Vector2f& position, float radius, Color color, float intensity, BlendMode blendMode)
{
    if (radius <= 0.0f || intensity <= 0.0f) return;

    constexpr int kSegments = 36;
    constexpr float kPi = 3.14159265f;

    const float clampedIntensity = std::clamp(intensity, 0.0f, 2.0f);
    const auto r = static_cast<uint8_t>(std::clamp(static_cast<float>(color.r) * clampedIntensity, 0.0f, 255.0f));
    const auto g = static_cast<uint8_t>(std::clamp(static_cast<float>(color.g) * clampedIntensity, 0.0f, 255.0f));
    const auto b = static_cast<uint8_t>(std::clamp(static_cast<float>(color.b) * clampedIntensity, 0.0f, 255.0f));
    const auto a = static_cast<uint8_t>(std::clamp(static_cast<float>(color.a) * std::min(1.0f, clampedIntensity), 0.0f, 255.0f));

    const sf::Color centerColor(r, g, b, a);
    const sf::Color edgeColor(0, 0, 0, 0);

    // Triangle fan with center vertex and outer circle vertices
    std::vector<sf::Vertex> fanVertices;
    fanVertices.reserve(kSegments + 2);
    fanVertices.emplace_back(sf::Vector2f(position.X, position.Y), centerColor);

    for (int i = 0; i <= kSegments; ++i)
    {
        const float angle = static_cast<float>(i) * (2.0f * kPi / static_cast<float>(kSegments));
        fanVertices.emplace_back(
            sf::Vector2f(position.X + radius * std::cos(angle), position.Y + radius * std::sin(angle)),
            edgeColor
        );
    }

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);
    m_lightMapTexture.draw(fanVertices.data(), fanVertices.size(), sf::PrimitiveType::TriangleFan, states);
}

void Renderer::DrawConeLight(const Vector2f& position, float radius, float directionDeg, float coneAngleDeg, Color color, float intensity, BlendMode blendMode)
{
    if (radius <= 0.0f || intensity <= 0.0f || coneAngleDeg <= 0.0f) return;

    constexpr int kSegments = 24;
    constexpr float kDegToRad = 3.14159265f / 180.0f;

    const float clampedIntensity = std::clamp(intensity, 0.0f, 2.0f);
    const auto r = static_cast<uint8_t>(std::clamp(static_cast<float>(color.r) * clampedIntensity, 0.0f, 255.0f));
    const auto g = static_cast<uint8_t>(std::clamp(static_cast<float>(color.g) * clampedIntensity, 0.0f, 255.0f));
    const auto b = static_cast<uint8_t>(std::clamp(static_cast<float>(color.b) * clampedIntensity, 0.0f, 255.0f));
    const auto a = static_cast<uint8_t>(std::clamp(static_cast<float>(color.a) * std::min(1.0f, clampedIntensity), 0.0f, 255.0f));

    const sf::Color apexColor(r, g, b, a);
    const sf::Color edgeColor(0, 0, 0, 0);

    const float halfCone = (coneAngleDeg * 0.5f) * kDegToRad;
    const float centerRad = directionDeg * kDegToRad;
    const float startAngle = centerRad - halfCone;
    const float totalAngle = coneAngleDeg * kDegToRad;

    std::vector<sf::Vertex> fanVertices;
    fanVertices.reserve(kSegments + 2);
    fanVertices.emplace_back(sf::Vector2f(position.X, position.Y), apexColor);

    for (int i = 0; i <= kSegments; ++i)
    {
        const float angle = startAngle + (static_cast<float>(i) / static_cast<float>(kSegments)) * totalAngle;
        fanVertices.emplace_back(
            sf::Vector2f(position.X + radius * std::cos(angle), position.Y + radius * std::sin(angle)),
            edgeColor
        );
    }

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);
    m_lightMapTexture.draw(fanVertices.data(), fanVertices.size(), sf::PrimitiveType::TriangleFan, states);
}

void Renderer::EndLightingPass(BlendMode compositeBlendMode)
{
    m_lightMapTexture.display();

    // Composite light map over m_renderTexture in normalized screen overlay space
    const sf::View prevView = m_renderTexture.getView();
    const sf::Vector2u texSize = m_renderTexture.getSize();
    const sf::View screenView(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(texSize.x), static_cast<float>(texSize.y)}));
    m_renderTexture.setView(screenView);

    sf::Sprite lightSprite(m_lightMapTexture.getTexture());
    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(compositeBlendMode);
    m_renderTexture.draw(lightSprite, states);

    m_renderTexture.setView(prevView);
}


