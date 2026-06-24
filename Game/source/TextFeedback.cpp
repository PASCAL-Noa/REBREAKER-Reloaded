#include "UI/TextFeedback.h"
#include "ECS/Components/UI/RectTransform.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/AnimatorComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "Graphics/Renderer.h"
#include "TweenEffects/TweenEffects.h"

TextFeedback::TextFeedback(Registry& registry, GameContext& context, uint32_t fontId, uint32_t fireTexId)
    : m_registry(registry), m_context(context), m_fontId(fontId), m_fireTexId(fireTexId)
{
}

void TextFeedback::OnInit(Entity canvasParent)
{
    m_comboFlame = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(m_comboFlame, RectTransform{
        .Position = {0.0f, 0.0f},
        .Size = {0.0f, 0.0f},
        .AnchorPoint = Anchor::TopLeft,
        .Parent = canvasParent
    });
    
    SpriteComponent flameSprite{m_fireTexId};
    flameSprite.Origin = Vector2f{10.0f, 24.0f};
    m_registry.AddComponent<SpriteComponent>(m_comboFlame, flameSprite);
    
    AnimatorComponent fireAnim;
    AnimationData fireAnimData;
    fireAnimData.FrameWidth = 20;
    fireAnimData.FrameHeight = 24;
    fireAnimData.FrameCount = 8;
    fireAnimData.FrameDuration = 0.1f;
    fireAnim.Play(fireAnimData);
    
    m_registry.AddComponent<AnimatorComponent>(m_comboFlame, fireAnim);
    m_registry.AddComponent<TweenComponent>(m_comboFlame, TweenComponent{});
}

void TextFeedback::OnUpdate(float dt, uint32_t comboMultiplier, uint32_t score)
{
    if (m_registry.HasComponent<RectTransform>(m_comboFlame) && m_registry.HasComponent<TweenComponent>(m_comboFlame))
    {
        if (comboMultiplier != m_lastCombo)
        {
            auto& flameTransform = m_registry.GetComponent<RectTransform>(m_comboFlame);
            auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_comboFlame);
            
            if (comboMultiplier > 1)
            {
                float targetScale = 1.0f + (comboMultiplier * 0.5f);
                if (targetScale > 4.0f) targetScale = 4.0f;

                tweenComp.ActiveTweens.clear();
                
                TweenConfig<Vector2f> config;
                config.Start = flameTransform.Size;
                config.End = Vector2f{40.0f * targetScale, 48.0f * targetScale};
                config.Duration = 0.3f;
                config.Ease = EasingFunctions::EasingType::EaseOutBounce;
                Entity entity = m_comboFlame;
                Registry* reg = &m_registry;
                config.Setter = [reg, entity](Vector2f val) {
                    if (reg->HasComponent<RectTransform>(entity)) {
                        reg->GetComponent<RectTransform>(entity).Size = val;
                    }
                };
                tweenComp.AddTween(config);
            }
            else
            {
                tweenComp.ActiveTweens.clear();
                
                TweenConfig<Vector2f> config;
                config.Start = flameTransform.Size;
                config.End = Vector2f{0.0f, 0.0f};
                config.Duration = 0.3f;
                config.Ease = EasingFunctions::EasingType::EaseOutQuad;
                Entity entity = m_comboFlame;
                Registry* reg = &m_registry;
                config.Setter = [reg, entity](Vector2f val) {
                    if (reg->HasComponent<RectTransform>(entity)) {
                        reg->GetComponent<RectTransform>(entity).Size = val;
                    }
                };
                tweenComp.AddTween(config);
            }
            
            m_lastCombo = comboMultiplier;
        }
    }
}

void TextFeedback::SpawnComboText(Entity canvasParent, const Vector2f& worldPosition, uint32_t comboMultiplier, const Camera2D& worldCamera)
{
    // Map world pos to physical screen pos
    m_context.Render.SetCamera(worldCamera);
    Vector2f physicalPixel = m_context.Render.MapCoordsToPixel(worldPosition);
    m_context.Render.ResetCamera();
    Vector2f uiPosition = m_context.Render.MapPixelToCoords(physicalPixel);

    Entity ft = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(ft, RectTransform{
        .Position = uiPosition,
        .Size = {100.0f, 50.0f},
        .AnchorPoint = Anchor::TopLeft,
        .Parent = canvasParent
    });
    
    m_registry.AddComponent<TextComponent>(ft, TextComponent{
        .Text = "x" + std::to_string(comboMultiplier),
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Colors::Yellow
    });
    
    m_registry.AddComponent<TweenComponent>(ft, TweenComponent{});
    auto& tweenComp = m_registry.GetComponent<TweenComponent>(ft);
    
    TweenConfig<float> moveTween;
    moveTween.Start = uiPosition.Y;
    moveTween.End = uiPosition.Y - 50.0f;
    moveTween.Duration = 1.0f;
    moveTween.Ease = EasingFunctions::EasingType::EaseOutCubic;
    Registry* reg = &m_registry;
    moveTween.Setter = [reg, ft](float val) {
        if (reg->HasComponent<RectTransform>(ft)) {
            reg->GetComponent<RectTransform>(ft).Position.Y = val;
        }
    };
    
    TweenConfig<float> fadeTween;
    fadeTween.Start = 255.0f;
    fadeTween.End = 0.0f;
    fadeTween.Duration = 1.0f;
    fadeTween.Ease = EasingFunctions::EasingType::EaseOutQuad;
    fadeTween.Setter = [reg, ft](float val) {
        if (reg->HasComponent<TextComponent>(ft)) {
            reg->GetComponent<TextComponent>(ft).Tint.a = static_cast<uint8_t>(val);
        }
    };
    fadeTween.OnComplete = [reg, ft]() {
        reg->DestroyEntity(ft);
    };
    
    tweenComp.AddTween(moveTween);
    tweenComp.AddTween(fadeTween);
}

void TextFeedback::SetFlamePosition(const RectTransform& referenceTransform, float xOffset, float yOffset) const
{
    if (m_registry.HasComponent<RectTransform>(m_comboFlame))
    {
        auto& flameTransform = m_registry.GetComponent<RectTransform>(m_comboFlame);
        flameTransform.Position = referenceTransform.Position;
        flameTransform.Position.X += xOffset;
        flameTransform.Position.Y += yOffset;
        flameTransform.AnchorPoint = referenceTransform.AnchorPoint;
        flameTransform.Parent = referenceTransform.Parent;
    }
}
