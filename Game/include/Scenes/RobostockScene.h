#pragma once

#include "Scenes/DefaultScene.h"
#include "Editor/CustomLevelLibrary.h"
#include <string>
#include <vector>

class RobostockScene : public DefaultScene
{
public:
    RobostockScene();
    explicit RobostockScene(std::string initialSelectedLevelId);
    ~RobostockScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

protected:
    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    enum class DragKind : uint8_t
    {
        None = 0,
        LevelCard = 1,
        PlaylistSlot = 2
    };

    enum class ModalMode : uint8_t
    {
        None = 0,
        RenameLevel = 1,
        CreatePlaylist = 2,
        RenamePlaylist = 3
    };

    struct CardRect
    {
        Vector2f Center{0.0f, 0.0f};
        Vector2f Size{0.0f, 0.0f};
        [[nodiscard]] bool Contains(const Vector2f& p) const noexcept
        {
            return std::abs(p.X - Center.X) <= Size.X * 0.5f &&
                   std::abs(p.Y - Center.Y) <= Size.Y * 0.5f;
        }
    };

    void HandleInput(GameContext& context);
    void HandleModalInput(GameContext& context);
    void OpenModal(ModalMode mode, const std::string& initialText);
    void ConfirmModal(GameContext& context);

    void RenderTopBar(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);
    void RenderVaultGrid(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);
    void RenderPlaylistPanel(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);
    void RenderDragGhost(GameContext& context);
    void RenderModal(GameContext& context, const Vector2f& viewSize, const Vector2f& mouse);

    void RenderMiniBrickPreview(GameContext& context, const CustomLevelEntry& entry, const Vector2f& center, const Vector2f& size) const;
    void DrawIconPlay(GameContext& context, const Vector2f& pos, float size, Color color) const;
    void DrawIconEdit(GameContext& context, const Vector2f& pos, float size, Color color) const;
    void DrawIconTrash(GameContext& context, const Vector2f& pos, float size, Color color) const;
    void DrawIconPlus(GameContext& context, const Vector2f& pos, float size, Color color) const;
    void DrawIconRename(GameContext& context, const Vector2f& pos, float size, Color color) const;
    void DrawBotBadge(GameContext& context, const Vector2f& pos, float size) const;

    [[nodiscard]] CardRect GetVaultCardRect(int visibleSlot, const Vector2f& viewSize) const;
    [[nodiscard]] CardRect GetPlaylistSlotRect(int slotIdx, const Vector2f& viewSize) const;

    CustomLevelLibrary m_library;
    std::string        m_initialSelectedId;
    int                m_selectedLevelIdx = 0;
    int                m_vaultPage = 0;
    int                m_activePlaylistIdx = 0;

    // Drag & Drop state
    DragKind           m_dragKind = DragKind::None;
    int                m_dragSourceIdx = -1;
    Vector2f           m_dragStartPos{0.0f, 0.0f};
    Vector2f           m_dragCurrentPos{0.0f, 0.0f};
    bool               m_dragActive = false;

    // Rename / Create Modal state
    ModalMode          m_modalMode = ModalMode::None;
    std::string        m_modalTextBuffer;

    // Assets & Feedback
    uint32_t           m_fontId = 0;
    uint32_t           m_clickSfxId = 0;
    uint32_t           m_switchSfxId = 0;
    float              m_animTimer = 0.0f;
};
