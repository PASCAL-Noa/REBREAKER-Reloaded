#pragma once

#include "Core/ProfileManager.h"
#include "Data/Color.h"
#include "Math/Vector2.h"
#include <string>
#include <vector>
#include <cstdint>

enum class ProfileActionResult : uint8_t
{
    None = 0,
    Equipped = 1,
    UnlockedAndEquipped = 2,
    InsufficientFunds = 3,
    MilestoneLocked = 4,
    AlreadyEquipped = 5
};

class ProfileGarageState
{
public:
    static constexpr int kTabCount = 4;
    static constexpr int kSideButtonCount = 3;
    static constexpr int kGridCols = 4;
    static constexpr int kGridRows = 2;
    static constexpr int kItemsPerPage = kGridCols * kGridRows;
    static constexpr int kColorSwatchesPerRow = 8;

    void Initialize();
    void UpdateTimers(float dt);

    [[nodiscard]] int GetActiveTab() const noexcept { return m_activeTab; }
    [[nodiscard]] CosmeticSlot GetActiveSlot() const noexcept;
    bool SwitchTab(int tabIdx);

    [[nodiscard]] int GetCurrentPage() const noexcept { return m_currentPage; }
    [[nodiscard]] int GetPageCount() const noexcept;
    [[nodiscard]] int GetPageItemCount() const noexcept;
    [[nodiscard]] int GetGlobalIndexForSlot(int slotIdx) const noexcept;
    bool NextPage();
    bool PrevPage();
    bool SetPage(int pageIdx);

    [[nodiscard]] const std::vector<CosmeticItem>& GetCurrentItems() const noexcept { return m_currentItems; }
    [[nodiscard]] int GetSelectedIndex() const noexcept { return m_selectedIndex; }
    [[nodiscard]] int GetHoveredIndex() const noexcept { return m_hoveredIndex; }
    void SetHoveredIndex(int globalIdx) noexcept { m_hoveredIndex = globalIdx; }
    void SetSelectedIndex(int globalIdx);
    [[nodiscard]] const CosmeticItem* GetInspectedItem() const noexcept;

    [[nodiscard]] int GetPressedSlot() const noexcept { return m_pressedSlot; }
    void SetPressedSlot(int slotIdx) noexcept { m_pressedSlot = slotIdx; }
    [[nodiscard]] int GetPressedColorSwatch() const noexcept { return m_pressedColorSwatch; }
    void SetPressedColorSwatch(int swatchIdx) noexcept { m_pressedColorSwatch = swatchIdx; }

    ProfileActionResult ActivateSelectedItem();
    void NotifySelectedLockedItem();
    void RandomizeEquippedLoadout();

    [[nodiscard]] bool IsColorModalOpen() const noexcept { return m_colorModalOpen; }
    void OpenColorModal(int channel);
    void CloseColorModal();
    [[nodiscard]] int GetColorChannel() const noexcept { return m_colorChannel; }
    bool SwitchColorChannel(int channel);
    [[nodiscard]] const std::vector<CosmeticItem>& GetColorModalItems() const noexcept { return m_colorModalItems; }
    [[nodiscard]] int GetColorSelectedIndex() const noexcept { return m_colorSelectedIndex; }
    [[nodiscard]] int GetColorHoveredIndex() const noexcept { return m_colorHoveredIndex; }
    void SetColorHoveredIndex(int idx) noexcept { m_colorHoveredIndex = idx; }
    void SetColorSelectedIndex(int idx);
    [[nodiscard]] const CosmeticItem* GetInspectedColorItem() const noexcept;
    ProfileActionResult ActivateSelectedColor();
    void NotifySelectedLockedColor();
    void RandomizeEquippedColors();

    [[nodiscard]] bool IsEditingNickname() const noexcept { return m_editingNickname; }
    [[nodiscard]] const std::string& GetNicknameBuffer() const noexcept { return m_nicknameBuffer; }
    void BeginNicknameEdit();
    void AppendNicknameChars(const std::string& chars);
    void BackspaceNickname();
    void CommitNicknameEdit();
    void CancelNicknameEdit();

    void ShowToast(const std::string& text, Color color);
    [[nodiscard]] const std::string& GetToastText() const noexcept { return m_toastText; }
    [[nodiscard]] Color GetToastColor() const noexcept { return m_toastColor; }
    [[nodiscard]] float GetToastTimer() const noexcept { return m_toastTimer; }

    [[nodiscard]] Color GetPreviewPrimaryColor() const;
    [[nodiscard]] Color GetPreviewSecondaryColor() const;
    [[nodiscard]] PaddleSkinStyle GetPreviewPaddleStyle() const;
    [[nodiscard]] BallSkinStyle GetPreviewBallStyle() const;
    [[nodiscard]] TrailStyle GetPreviewTrailStyle() const;
    [[nodiscard]] DeathEffectStyle GetPreviewDeathStyle() const;

    static CosmeticSlot TabIndexToSlot(int tabIdx) noexcept;
    static const char* SlotToShortTag(CosmeticSlot slot) noexcept;

    void RefreshCategoryItems();
    void RefreshColorModalItems();

private:
    void SelectEquippedInCurrentTab();
    void SelectEquippedInColorModal();

    int m_activeTab = 0;
    int m_currentPage = 0;
    std::vector<CosmeticItem> m_currentItems;
    int m_selectedIndex = 0;
    int m_hoveredIndex = -1;
    int m_pressedSlot = -1;

    bool m_colorModalOpen = false;
    int m_colorChannel = 0;
    std::vector<CosmeticItem> m_colorModalItems;
    int m_colorSelectedIndex = 0;
    int m_colorHoveredIndex = -1;
    int m_pressedColorSwatch = -1;

    bool m_editingNickname = false;
    std::string m_nicknameBuffer;

    std::string m_toastText;
    Color m_toastColor{0, 235, 255, 255};
    float m_toastTimer = 0.0f;
};
