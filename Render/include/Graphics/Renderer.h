#pragma once
#include "System/Window.h"
#include "Resources/ResourceManager.h"
#include "Data/SceneTransitionType.h"
#include "Data/Color.h"
#include "Data/BlendMode.h"
#include "Data/PrimitiveType.h"
#include "Data/Vertex.h"
#include "Data/PostProcessParams.h"
#include "Math/Vector2.h"
#include <string>
#include <array>

struct Camera2D;
struct Transform2D;
struct SpriteComponent;

class Renderer
{
public:
    Renderer(Window& window, ResourceManager& resources);
    ~Renderer() = default;

    void                BeginDraw(Color clearColor = Colors::Black);
    void                EndDraw(uint32_t postProcessShaderId = 0, const PostProcessParams& params = PostProcessParams{});

    void                SetCamera(const Camera2D& camera);
    void                ResetCamera();

    Window&             GetWindow() { return m_window; }
    const Window&       GetWindow() const { return m_window; }

    [[nodiscard]] Vector2f  GetLogicalViewSize() const;
    [[nodiscard]] sf::FloatRect GetLetterboxViewport() const;
    [[nodiscard]] float     GetEffectiveAspectRatio() const;
    [[nodiscard]] std::array<float, 4> WorldRectToNormalizedUV(const Vector2f& center, const Vector2f& size) const;
    void                    OnWindowResized(unsigned int width, unsigned int height);
    [[nodiscard]] Vector2f  MapPixelToCoords(const Vector2f& pixelPos) const;
    [[nodiscard]] Vector2f  MapCoordsToPixel(const Vector2f& coords) const;

    void                DrawSprite(const SpriteComponent& spriteData, const Transform2D& transform, BlendMode blendMode = BlendMode::Alpha);
    void                DrawCircle(float radius, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawRectangle(float width, float height, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawText(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawTextCentered(const std::string& text, uint32_t fontId, float fontSize, const Transform2D& transform, Color color = Colors::White, BlendMode blendMode = BlendMode::Alpha);
    void                DrawVertices(const std::vector<Vertex>& vertices, PrimitiveType type, uint32_t textureId = 0, BlendMode blendMode = BlendMode::Alpha);
    void                DrawRectangleOutline(float width, float height, const Transform2D& transform, Color color, float thickness = 2.0f);
    void                DrawCircleOutline(float radius, const Transform2D& transform, Color color, float thickness = 2.0f);
    void                DrawLine(const Vector2f& p1, const Vector2f& p2, Color color, float thickness = 1.0f);
    void                DrawScannerEffect(float width, float height, const Transform2D& transform, Color color, float time, uint32_t shaderId);
    void                DrawSceneTransition(SceneTransitionType type, float progress, Color color = Colors::Black);

    // 2D Dynamic Lighting Pipeline
    void                BeginLightingPass(Color ambientDarkness);
    void                DrawPointLight(const Vector2f& position, float radius, Color color, float intensity = 1.0f, BlendMode blendMode = BlendMode::Add);
    void                DrawConeLight(const Vector2f& position, float radius, float directionDeg, float coneAngleDeg, Color color, float intensity = 1.0f, BlendMode blendMode = BlendMode::Add);
    void                DrawCapsuleLight(const Vector2f& start, const Vector2f& end, float radius, Color color, float intensity = 1.0f, BlendMode blendMode = BlendMode::Add);
    void                EndLightingPass(BlendMode compositeBlendMode = BlendMode::Multiply);

    Vector2f            GetTextureSize(uint32_t textureId) const;
    Vector2f            GetTextSize(const std::string& text, uint32_t fontId, float fontSize) const;
    [[nodiscard]] std::string WrapText(const std::string& text, float maxWidth, uint32_t fontId, float fontSize) const;

private:
    void                DrawLetterboxBars(sf::RenderTarget& target, const sf::Vector2u& size) const;

    Window&             m_window;
    ResourceManager&    m_resources;
    sf::RenderTexture   m_renderTexture;
    sf::RenderTexture   m_lightMapTexture;
    std::vector<sf::Vertex> m_vertexScratch;
};
