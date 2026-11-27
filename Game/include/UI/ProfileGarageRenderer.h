#pragma once

#include "UI/ProfileGarageState.h"
#include "UI/ProfileGarageLayout.h"
#include "ECS/Components/TrailComponent.h"
#include "Graphics/CosmeticRenderer.h"
#include "Data/Vertex.h"
#include <vector>
#include <cstdint>

class Renderer;

struct ProfileRenderAssets
{
    uint32_t FontId = 0;
    uint32_t PaddleTexId = 0;
    uint32_t BallTexId = 0;
};

struct ProfileShowcaseSnapshot
{
    float AnimTime = 0.0f;
    Vector2f BallPos{0.0f, 0.0f};
    const TrailComponent* Trail = nullptr;
    const CosmeticExplosionPool* Explosions = nullptr;
    std::vector<Vertex>* TrailScratch = nullptr;
};

class ProfileGarageRenderer
{
public:
    static void Render(
        Renderer& renderer,
        const ProfileGarageState& state,
        const ProfileRenderAssets& assets,
        const ProfileShowcaseSnapshot& showcase,
        const Vector2f& viewSize,
        const Vector2f& mouse);

private:
    static void RenderBackground(Renderer& r, const Vector2f& viewSize, Color primary);
    static void RenderCornerFrames(Renderer& r, const Vector2f& viewSize, Color primary, Color secondary);
    static void RenderTopHud(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, float animTime, const Vector2f& viewSize, const Vector2f& mouse, Color primary);
    static void RenderSideRail(Renderer& r, const ProfileGarageState& state, const Vector2f& viewSize, const Vector2f& mouse, Color primary, Color secondary);
    static void RenderFocusedShowcase(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, const ProfileShowcaseSnapshot& showcase, const Vector2f& viewSize);
    static void RenderCategoryTabs(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, const Vector2f& viewSize, const Vector2f& mouse, Color primary);
    static void RenderPaginatedDrawer(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, const Vector2f& viewSize, const Vector2f& mouse, Color pri, Color sec);
    static void RenderColorModal(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, const ProfileShowcaseSnapshot& showcase, const Vector2f& viewSize, const Vector2f& mouse);
    static void RenderCareerView(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, float animTime, const Vector2f& viewSize, const Vector2f& mouse, Color primary, Color secondary);
    static void RenderToast(Renderer& r, const ProfileGarageState& state, const ProfileRenderAssets& assets, const Vector2f& viewSize);
};
