#include "UI/TextFeedback.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/AnimatorComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "Graphics/Renderer.h"
#include "TweenEffects/TweenEffects.h"

TextFeedback::TextFeedback(Registry& registry, GameContext& context, uint32_t fontId, uint32_t fireTexId)
    : m_registry(registry), m_context(context), m_fontId(fontId), m_fireTexId(fireTexId)
{
}

void TextFeedback::OnInit()
{
    m_comboFlame = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_comboFlame, Transform2D{Vector2f{0.0f, 0.0f}, 0.0f, Vector2f{0.0f, 0.0f}});
    
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
    for (auto it = m_floatingTexts.begin(); it != m_floatingTexts.end(); )
    {
        it->Timer -= dt;
        it->Position.Y -= 50.0f * dt;
        if (it->Timer <= 0.0f)
            it = m_floatingTexts.erase(it);
        else
            ++it;
    }

    if (m_registry.HasComponent<Transform2D>(m_comboFlame) && m_registry.HasComponent<TweenComponent>(m_comboFlame))
    {
        if (comboMultiplier != m_lastCombo)
        {
            auto& flameTransform = m_registry.GetComponent<Transform2D>(m_comboFlame);
            auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_comboFlame);
            
            if (comboMultiplier > 1)
            {
                float targetScale = 1.0f + (comboMultiplier * 0.5f);
                if (targetScale > 4.0f) targetScale = 4.0f;

                tweenComp.ActiveTweens.clear();
                TweenEffects::ComboFlameScale(tweenComp, flameTransform, targetScale);
            }
            else
            {
                tweenComp.ActiveTweens.clear();
                TweenEffects::ComboFlameScale(tweenComp, flameTransform, 0.0f);
            }
            
            m_lastCombo = comboMultiplier;
        }
    }
}

void TextFeedback::OnRender()
{
    // Draw floating texts
    for (const auto& ft : m_floatingTexts)
    {
        float alpha = ft.Timer / ft.InitialTimer;
        Color col = Colors::Yellow;
        col.a = static_cast<uint8_t>(alpha * 255);
        m_context.Render.DrawText(ft.Text, m_fontId, 32.0f, Transform2D{ft.Position}, col);
    }
}

void TextFeedback::SpawnComboText(const Vector2f& position, uint32_t comboMultiplier)
{
    FloatingText ft;
    ft.Position = position;
    ft.Text = "x" + std::to_string(comboMultiplier);
    ft.Timer = 1.0f;
    ft.InitialTimer = 1.0f;
    m_floatingTexts.push_back(ft);
}

void TextFeedback::SetFlamePosition(const Vector2f& position) const
{
    if (m_registry.HasComponent<Transform2D>(m_comboFlame))
    {
        auto& flameTransform = m_registry.GetComponent<Transform2D>(m_comboFlame);
        flameTransform.Position = position;
    }
}
