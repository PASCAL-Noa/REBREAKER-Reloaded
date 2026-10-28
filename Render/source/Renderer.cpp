#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Camera2D.h"
#include "ECS/Components/SpriteComponent.h"
#include <sstream>
#include <vector>

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
        shader->setUniform("u_previewRect", sf::Glsl::Vec4(
            params.PreviewRect[0],
            params.PreviewRect[1],
            params.PreviewRect[2],
            params.PreviewRect[3]
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
    auto& rt = GetActiveRenderTarget();
    if (&rt == &m_renderTexture)
    {
        const Vector2f viewSize = GetLogicalViewSize();
        sf::View view(sf::Vector2f(0.f, 0.f), sf::Vector2f(viewSize.X, viewSize.Y));
        view.setCenter({camera.Position.X, camera.Position.Y});
        view.setRotation(sf::degrees(camera.Rotation));
        view.zoom(camera.Zoom);
        view.setViewport(GetLetterboxViewport());
        rt.setView(view);
    }
    else
    {
        const sf::Vector2u texSize = rt.getSize();
        sf::View view(sf::Vector2f(0.f, 0.f), sf::Vector2f(static_cast<float>(texSize.x), static_cast<float>(texSize.y)));
        view.setCenter({camera.Position.X, camera.Position.Y});
        view.setRotation(sf::degrees(camera.Rotation));
        view.zoom(camera.Zoom);
        rt.setView(view);
    }
}

void Renderer::ResetCamera()
{
    auto& rt = GetActiveRenderTarget();
    if (&rt == &m_renderTexture)
    {
        const Vector2f viewSize = GetLogicalViewSize();
        sf::View view(sf::Vector2f(viewSize.X / 2.0f, viewSize.Y / 2.0f), sf::Vector2f(viewSize.X, viewSize.Y));
        view.setViewport(GetLetterboxViewport());
        rt.setView(view);
    }
    else
    {
        const sf::Vector2u texSize = rt.getSize();
        sf::View view(sf::Vector2f(static_cast<float>(texSize.x) * 0.5f, static_cast<float>(texSize.y) * 0.5f),
                      sf::Vector2f(static_cast<float>(texSize.x), static_cast<float>(texSize.y)));
        rt.setView(view);
    }
}

void Renderer::PushRenderTarget(sf::RenderTexture& target)
{
    m_targetStack.push_back(&target);
}

void Renderer::PopRenderTarget()
{
    if (!m_targetStack.empty())
    {
        m_targetStack.pop_back();
    }
}

sf::RenderTexture& Renderer::GetActiveRenderTarget()
{
    if (!m_targetStack.empty() && m_targetStack.back() != nullptr)
    {
        return *m_targetStack.back();
    }
    return m_renderTexture;
}

const sf::RenderTexture& Renderer::GetActiveRenderTarget() const
{
    if (!m_targetStack.empty() && m_targetStack.back() != nullptr)
    {
        return *m_targetStack.back();
    }
    return m_renderTexture;
}

void Renderer::DrawRawTexture(const sf::Texture& texture, float width, float height, const Transform2D& transform, Color tint, uint32_t shaderId, float time, float effectProgress)
{
    sf::Sprite sprite(texture);
    const sf::Vector2u texSize = texture.getSize();
    if (texSize.x == 0 || texSize.y == 0) return;

    sprite.setOrigin({static_cast<float>(texSize.x) * 0.5f, static_cast<float>(texSize.y) * 0.5f});
    sprite.setScale({(width / static_cast<float>(texSize.x)) * transform.Scale.X,
                     (height / static_cast<float>(texSize.y)) * transform.Scale.Y});
    sprite.setPosition({transform.Position.X, transform.Position.Y});
    sprite.setRotation(sf::degrees(transform.Rotation));
    sprite.setColor(ToSfColor(tint));

    sf::RenderStates states;
    states.blendMode = sf::BlendAlpha;

    if (shaderId != 0 && sf::Shader::isAvailable())
    {
        if (sf::Shader* shader = m_resources.Get<sf::Shader>(shaderId))
        {
            shader->setUniform("u_texture", sf::Shader::CurrentTexture);
            shader->setUniform("u_time", time);
            shader->setUniform("u_switch_progress", effectProgress);
            shader->setUniform("u_resolution", sf::Glsl::Vec2(width, height));
            states.shader = shader;
        }
    }

    GetActiveRenderTarget().draw(sprite, states);
}

Vector2f Renderer::GetLogicalViewSize() const
{
    return Vector2f{kLogicalHeight * GetEffectiveAspectRatio(), kLogicalHeight};
}

std::array<float, 4> Renderer::WorldRectToNormalizedUV(const Vector2f& center, const Vector2f& size) const
{
    const Vector2f viewSize = GetLogicalViewSize();
    if (viewSize.X <= 0.0f || viewSize.Y <= 0.0f)
        return {0.0f, 0.0f, 0.0f, 0.0f};

    float lxMin = (center.X - size.X * 0.5f) / viewSize.X;
    float lxMax = (center.X + size.X * 0.5f) / viewSize.X;
    float lyMin = (center.Y - size.Y * 0.5f) / viewSize.Y;
    float lyMax = (center.Y + size.Y * 0.5f) / viewSize.Y;

    const sf::FloatRect vp = GetLetterboxViewport();
    float uMin = vp.position.x + lxMin * vp.size.x;
    float uMax = vp.position.x + lxMax * vp.size.x;
    // SFML RenderTexture FBO has Y inverted in sprite texture coordinates
    float vMin = 1.0f - (vp.position.y + lyMax * vp.size.y);
    float vMax = 1.0f - (vp.position.y + lyMin * vp.size.y);

    return { uMin, vMin, uMax, vMax };
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
    
    GetActiveRenderTarget().draw(sprite, states);
}

void Renderer::DrawCircle(float radius, const Transform2D& transform, Color color, BlendMode blendMode)
{
    sf::CircleShape circle(radius);
    circle.setOrigin({radius, radius});
    ApplyTransform(circle, transform);
    circle.setFillColor(ToSfColor(color));
    RenderItem(GetActiveRenderTarget(), circle, blendMode);
}

void Renderer::DrawRectangle(float width, float height, const Transform2D& transform, Color color, BlendMode blendMode)
{
    sf::RectangleShape rect({width, height});
    rect.setOrigin({width / 2.0f, height / 2.0f});
    ApplyTransform(rect, transform);
    rect.setFillColor(ToSfColor(color));
    RenderItem(GetActiveRenderTarget(), rect, blendMode);
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

    GetActiveRenderTarget().draw(rect, states);
}

void Renderer::DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color, BlendMode blendMode)
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        ApplyTransform(sfText, transform);
        sfText.setFillColor(ToSfColor(color));
        RenderItem(GetActiveRenderTarget(), sfText, blendMode);
    }
}

void Renderer::DrawTextCentered(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color, BlendMode blendMode)
{
    if (sf::Font* font = m_resources.Get<sf::Font>(fontId))
    {
        if (text.find('\n') != std::string::npos)
        {
            std::vector<std::string> lines;
            std::stringstream ss(text);
            std::string line;
            while (std::getline(ss, line))
            {
                lines.push_back(line);
            }
            if (!lines.empty())
            {
                float lineSpacing = fontSize * 1.25f;
                float totalH = static_cast<float>(lines.size() - 1) * lineSpacing;
                float startY = transform.Position.Y - totalH * 0.5f;

                for (size_t i = 0; i < lines.size(); ++i)
                {
                    if (lines[i].empty()) continue;
                    sf::Text sfLine(*font, lines[i], static_cast<unsigned int>(fontSize));
                    const sf::FloatRect bounds = sfLine.getLocalBounds();
                    sfLine.setOrigin({bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + bounds.size.y * 0.5f});
                    Transform2D lineTransform = transform;
                    lineTransform.Position.Y = startY + static_cast<float>(i) * lineSpacing;
                    ApplyTransform(sfLine, lineTransform);
                    sfLine.setFillColor(ToSfColor(color));
                    RenderItem(GetActiveRenderTarget(), sfLine, blendMode);
                }
                return;
            }
        }

        sf::Text sfText(*font, text, static_cast<unsigned int>(fontSize));
        const sf::FloatRect bounds = sfText.getLocalBounds();
        sfText.setOrigin({bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + bounds.size.y * 0.5f});
        ApplyTransform(sfText, transform);
        sfText.setFillColor(ToSfColor(color));
        RenderItem(GetActiveRenderTarget(), sfText, blendMode);
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

std::string Renderer::WrapText(const std::string& text, float maxWidth, uint32_t fontId, float fontSize) const
{
    if (maxWidth <= 0.0f || text.empty()) return text;

    sf::Font* font = m_resources.Get<sf::Font>(fontId);
    auto measureWidth = [&](const std::string& str) -> float {
        if (font)
        {
            sf::Text sfText(*font, str, static_cast<unsigned int>(fontSize));
            return sfText.getLocalBounds().size.x;
        }
        return static_cast<float>(str.size()) * (fontSize * 0.55f);
    };

    std::istringstream stream(text);
    std::string result;
    std::string paragraph;
    bool firstParagraph = true;

    while (std::getline(stream, paragraph))
    {
        if (!firstParagraph)
        {
            result += '\n';
        }
        firstParagraph = false;

        if (paragraph.empty()) continue;

        std::istringstream wordStream(paragraph);
        std::string currentLine;
        std::string word;

        while (wordStream >> word)
        {
            if (currentLine.empty())
            {
                currentLine = word;
            }
            else
            {
                std::string testLine = currentLine + " " + word;
                if (measureWidth(testLine) <= maxWidth)
                {
                    currentLine = testLine;
                }
                else
                {
                    if (!result.empty() && result.back() != '\n')
                    {
                        result += '\n';
                    }
                    result += currentLine;
                    currentLine = word;
                }
            }
        }

        if (!currentLine.empty())
        {
            if (!result.empty() && result.back() != '\n')
            {
                result += '\n';
            }
            result += currentLine;
        }
    }

    return result;
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

    GetActiveRenderTarget().draw(m_vertexScratch.data(), m_vertexScratch.size(), ToSfPrimitiveType(type), states);
}

void Renderer::DrawRectangleOutline(float width, float height, const Transform2D& transform, Color color, float thickness)
{
    sf::RectangleShape rect({width, height});

    rect.setOrigin({width / 2.0f, height / 2.0f});

    ApplyTransform(rect, transform);
    rect.setFillColor(sf::Color::Transparent);
    rect.setOutlineColor(ToSfColor(color));
    rect.setOutlineThickness(thickness);
    RenderItem(GetActiveRenderTarget(), rect, BlendMode::Alpha);
}

void Renderer::DrawCircleOutline(float radius, const Transform2D& transform, Color color, float thickness)
{
    sf::CircleShape circle(radius);
    
    circle.setOrigin({radius, radius});

    ApplyTransform(circle, transform);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineColor(ToSfColor(color));
    circle.setOutlineThickness(thickness);
    RenderItem(GetActiveRenderTarget(), circle, BlendMode::Alpha);
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

    GetActiveRenderTarget().draw(line);
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
    auto& rt = GetActiveRenderTarget();
    const sf::Vector2u targetSize = rt.getSize();
    if (m_lightMapTexture.getSize() != targetSize && targetSize.x > 0 && targetSize.y > 0)
    {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        (void)m_lightMapTexture.resize(targetSize, settings);
        m_lightMapTexture.setSmooth(true);
    }

    // Match current camera view and clear to ambient darkness level
    m_lightMapTexture.setView(rt.getView());
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

    // Triangle fan with center vertex and outer circle vertices (zero heap allocation)
    std::array<sf::Vertex, kSegments + 2> fanVertices;
    fanVertices[0] = sf::Vertex(sf::Vector2f(position.X, position.Y), centerColor);

    for (int i = 0; i <= kSegments; ++i)
    {
        const float angle = static_cast<float>(i) * (2.0f * kPi / static_cast<float>(kSegments));
        fanVertices[i + 1] = sf::Vertex(
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

    std::array<sf::Vertex, kSegments + 2> fanVertices;
    fanVertices[0] = sf::Vertex(sf::Vector2f(position.X, position.Y), apexColor);

    for (int i = 0; i <= kSegments; ++i)
    {
        const float angle = startAngle + (static_cast<float>(i) / static_cast<float>(kSegments)) * totalAngle;
        fanVertices[i + 1] = sf::Vertex(
            sf::Vector2f(position.X + radius * std::cos(angle), position.Y + radius * std::sin(angle)),
            edgeColor
        );
    }

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);
    m_lightMapTexture.draw(fanVertices.data(), fanVertices.size(), sf::PrimitiveType::TriangleFan, states);
}

void Renderer::DrawCapsuleLight(const Vector2f& start, const Vector2f& end, float radius, Color color, float intensity, BlendMode blendMode)
{
    if (radius <= 0.0f || intensity <= 0.0f) return;

    const float dx = end.X - start.X;
    const float dy = end.Y - start.Y;
    const float len = std::sqrt(dx * dx + dy * dy);

    if (len < 0.001f)
    {
        DrawPointLight(start, radius, color, intensity, blendMode);
        return;
    }

    const float clampedIntensity = std::clamp(intensity, 0.0f, 2.0f);
    const auto r = static_cast<uint8_t>(std::clamp(static_cast<float>(color.r) * clampedIntensity, 0.0f, 255.0f));
    const auto g = static_cast<uint8_t>(std::clamp(static_cast<float>(color.g) * clampedIntensity, 0.0f, 255.0f));
    const auto b = static_cast<uint8_t>(std::clamp(static_cast<float>(color.b) * clampedIntensity, 0.0f, 255.0f));
    const auto a = static_cast<uint8_t>(std::clamp(static_cast<float>(color.a) * std::min(1.0f, clampedIntensity), 0.0f, 255.0f));

    const sf::Color centerColor(r, g, b, a);
    const sf::Color edgeColor(0, 0, 0, 0);

    const float tx = dx / len;
    const float ty = dy / len;
    const float nx = -ty;
    const float ny = tx;

    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(blendMode);

    // 1. Central rectangular falloff body (zero heap allocation)
    const sf::Vector2f c1(start.X, start.Y);
    const sf::Vector2f c2(end.X, end.Y);
    const sf::Vector2f t1(start.X + nx * radius, start.Y + ny * radius);
    const sf::Vector2f t2(end.X + nx * radius, end.Y + ny * radius);
    const sf::Vector2f b1(start.X - nx * radius, start.Y - ny * radius);
    const sf::Vector2f b2(end.X - nx * radius, end.Y - ny * radius);

    std::array<sf::Vertex, 12> bodyVertices = {
        sf::Vertex(c1, centerColor), sf::Vertex(c2, centerColor), sf::Vertex(t2, edgeColor),
        sf::Vertex(c1, centerColor), sf::Vertex(t2, edgeColor), sf::Vertex(t1, edgeColor),
        sf::Vertex(c1, centerColor), sf::Vertex(b1, edgeColor), sf::Vertex(b2, edgeColor),
        sf::Vertex(c1, centerColor), sf::Vertex(b2, edgeColor), sf::Vertex(c2, centerColor)
    };

    m_lightMapTexture.draw(bodyVertices.data(), bodyVertices.size(), sf::PrimitiveType::Triangles, states);

    // 2. End-cap semicircles (zero heap allocation)
    constexpr int kCapSegments = 16;
    constexpr float kPi = 3.14159265f;

    // Right cap at `end`
    std::array<sf::Vertex, kCapSegments + 2> rightCap;
    rightCap[0] = sf::Vertex(c2, centerColor);
    for (int i = 0; i <= kCapSegments; ++i)
    {
        const float angle = -kPi * 0.5f + (static_cast<float>(i) / static_cast<float>(kCapSegments)) * kPi;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);
        const float px = end.X + (tx * cosA - ty * sinA) * radius;
        const float py = end.Y + (ty * cosA + tx * sinA) * radius;
        rightCap[i + 1] = sf::Vertex(sf::Vector2f(px, py), edgeColor);
    }
    m_lightMapTexture.draw(rightCap.data(), rightCap.size(), sf::PrimitiveType::TriangleFan, states);

    // Left cap at `start`
    std::array<sf::Vertex, kCapSegments + 2> leftCap;
    leftCap[0] = sf::Vertex(c1, centerColor);
    for (int i = 0; i <= kCapSegments; ++i)
    {
        const float angle = kPi * 0.5f + (static_cast<float>(i) / static_cast<float>(kCapSegments)) * kPi;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);
        const float px = start.X + (tx * cosA - ty * sinA) * radius;
        const float py = start.Y + (ty * cosA + tx * sinA) * radius;
        leftCap[i + 1] = sf::Vertex(sf::Vector2f(px, py), edgeColor);
    }
    m_lightMapTexture.draw(leftCap.data(), leftCap.size(), sf::PrimitiveType::TriangleFan, states);
}

void Renderer::EndLightingPass(BlendMode compositeBlendMode)
{
    m_lightMapTexture.display();

    // Composite light map over active render target in normalized screen overlay space
    auto& rt = GetActiveRenderTarget();
    const sf::View prevView = rt.getView();
    const sf::Vector2u texSize = rt.getSize();
    const sf::View screenView(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(texSize.x), static_cast<float>(texSize.y)}));
    rt.setView(screenView);

    sf::Sprite lightSprite(m_lightMapTexture.getTexture());
    sf::RenderStates states;
    states.blendMode = ToSfBlendMode(compositeBlendMode);
    rt.draw(lightSprite, states);

    rt.setView(prevView);
}


