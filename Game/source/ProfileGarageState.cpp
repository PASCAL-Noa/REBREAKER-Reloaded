#include "UI/ProfileGarageState.h"
#include "Core/PlayerPrefs.h"
#include "PowerUps/PowerUpConfig.h"
#include <algorithm>
#include <array>
#include <random>

CosmeticSlot ProfileGarageState::TabIndexToSlot(int tabIdx) noexcept
{
    switch (tabIdx)
    {
        case 0: return CosmeticSlot::PaddleSkin;
        case 1: return CosmeticSlot::BallSkin;
        case 2: return CosmeticSlot::BallTrail;
        case 3: return CosmeticSlot::DeathEffect;
        default: return CosmeticSlot::PaddleSkin;
    }
}

const char* ProfileGarageState::SlotToShortTag(CosmeticSlot slot) noexcept
{
    switch (slot)
    {
        case CosmeticSlot::PrimaryColor:   return "COL 1";
        case CosmeticSlot::SecondaryColor: return "COL 2";
        case CosmeticSlot::PaddleSkin:     return "PADDLE";
        case CosmeticSlot::BallSkin:       return "BALL";
        case CosmeticSlot::BallTrail:      return "TRAIL";
        case CosmeticSlot::DeathEffect:    return "EXPLOSION";
        default:                           return "ITEM";
    }
}

void ProfileGarageState::Initialize()
{
    auto& pm = ProfileManager::Get();
    pm.LoadProfile();
    const int newlyUnlocked = pm.EvaluateAchievementUnlocks();
    if (newlyUnlocked > 0)
    {
        ShowToast("NEW MILESTONE UNLOCKED (+" + std::to_string(newlyUnlocked) + ")", Color{255, 220, 75, 255});
    }

    m_viewMode = ProfileViewMode::Garage;
    m_activeTab = 0;
    m_currentPage = 0;
    m_hoveredIndex = -1;
    m_pressedSlot = -1;
    m_colorModalOpen = false;
    m_colorChannel = 0;
    m_colorHoveredIndex = -1;
    m_pressedColorSwatch = -1;
    m_editingNickname = false;
    m_nicknameBuffer = pm.GetUsername();
    m_careerScrollY = 0.0f;
    m_resetArmed = false;
    m_resetArmTimer = 0.0f;

    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    RefreshColorModalItems();
    SelectEquippedInColorModal();
}

void ProfileGarageState::UpdateTimers(float dt)
{
    if (m_toastTimer > 0.0f)
    {
        m_toastTimer = std::max(0.0f, m_toastTimer - dt);
    }
    if (m_resetArmed)
    {
        m_resetArmTimer -= dt;
        if (m_resetArmTimer <= 0.0f)
        {
            m_resetArmed = false;
        }
    }
}

void ProfileGarageState::SetViewMode(ProfileViewMode mode)
{
    m_viewMode = mode;
    m_hoveredIndex = -1;
    m_pressedSlot = -1;
    if (mode == ProfileViewMode::Career)
    {
        ProfileManager::Get().EvaluateAchievementUnlocks();
    }
}

CosmeticSlot ProfileGarageState::GetActiveSlot() const noexcept
{
    return TabIndexToSlot(m_activeTab);
}

bool ProfileGarageState::SwitchTab(int tabIdx)
{
    tabIdx = std::clamp(tabIdx, 0, kTabCount - 1);
    if (m_activeTab == tabIdx && !m_currentItems.empty())
    {
        return false;
    }
    m_activeTab = tabIdx;
    m_hoveredIndex = -1;
    m_pressedSlot = -1;
    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    return true;
}

int ProfileGarageState::GetPageCount() const noexcept
{
    const int count = static_cast<int>(m_currentItems.size());
    if (count <= 0) return 1;
    return (count + kItemsPerPage - 1) / kItemsPerPage;
}

int ProfileGarageState::GetPageItemCount() const noexcept
{
    const int count = static_cast<int>(m_currentItems.size());
    const int start = m_currentPage * kItemsPerPage;
    if (start >= count) return 0;
    return std::min(kItemsPerPage, count - start);
}

int ProfileGarageState::GetGlobalIndexForSlot(int slotIdx) const noexcept
{
    if (slotIdx < 0 || slotIdx >= kItemsPerPage) return -1;
    const int idx = m_currentPage * kItemsPerPage + slotIdx;
    if (idx < 0 || idx >= static_cast<int>(m_currentItems.size())) return -1;
    return idx;
}

bool ProfileGarageState::NextPage()
{
    const int pages = GetPageCount();
    if (pages <= 1) return false;
    m_currentPage = (m_currentPage + 1) % pages;
    m_hoveredIndex = -1;
    m_pressedSlot = -1;
    return true;
}

bool ProfileGarageState::PrevPage()
{
    const int pages = GetPageCount();
    if (pages <= 1) return false;
    m_currentPage = (m_currentPage + pages - 1) % pages;
    m_hoveredIndex = -1;
    m_pressedSlot = -1;
    return true;
}

bool ProfileGarageState::SetPage(int pageIdx)
{
    const int pages = GetPageCount();
    const int clamped = std::clamp(pageIdx, 0, pages - 1);
    if (clamped == m_currentPage) return false;
    m_currentPage = clamped;
    m_hoveredIndex = -1;
    m_pressedSlot = -1;
    return true;
}

void ProfileGarageState::SetSelectedIndex(int globalIdx)
{
    if (m_currentItems.empty())
    {
        m_selectedIndex = -1;
        return;
    }
    m_selectedIndex = std::clamp(globalIdx, 0, static_cast<int>(m_currentItems.size()) - 1);
    m_currentPage = std::clamp(m_selectedIndex / kItemsPerPage, 0, GetPageCount() - 1);
}

const CosmeticItem* ProfileGarageState::GetInspectedItem() const noexcept
{
    if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_currentItems.size()))
    {
        return &m_currentItems[static_cast<size_t>(m_hoveredIndex)];
    }
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_currentItems.size()))
    {
        return &m_currentItems[static_cast<size_t>(m_selectedIndex)];
    }
    return nullptr;
}

ProfileActionResult ProfileGarageState::ActivateSelectedItem()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_currentItems.size()))
    {
        return ProfileActionResult::None;
    }

    auto& pm = ProfileManager::Get();
    const CosmeticItem item = m_currentItems[static_cast<size_t>(m_selectedIndex)];
    const CosmeticSlot slot = GetActiveSlot();

    if (item.Unlocked)
    {
        if (pm.GetEquippedCosmetic(slot) == item.Id)
        {
            return ProfileActionResult::AlreadyEquipped;
        }
        pm.EquipCosmetic(slot, item.Id);
        RefreshCategoryItems();
        ShowToast("EQUIPPED: " + item.Name, GetPreviewPrimaryColor());
        return ProfileActionResult::Equipped;
    }

    if (item.UnlockType == CosmeticUnlockType::Shop)
    {
        if (PowerUpManager::Get().GetCoins() >= item.Price)
        {
            if (pm.UnlockCosmetic(item.Id))
            {
                pm.EquipCosmetic(slot, item.Id);
                RefreshCategoryItems();
                ShowToast("UNLOCKED: " + item.Name, Color{255, 220, 75, 255});
                return ProfileActionResult::UnlockedAndEquipped;
            }
        }
        const int missing = item.Price - PowerUpManager::Get().GetCoins();
        ShowToast("NEED " + std::to_string(missing) + " MORE CR", Color{255, 90, 90, 255});
        return ProfileActionResult::InsufficientFunds;
    }

    ShowToast("LOCKED: " + item.UnlockRequirement, Color{255, 185, 70, 255});
    return ProfileActionResult::MilestoneLocked;
}

void ProfileGarageState::NotifySelectedLockedItem()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_currentItems.size())) return;
    const CosmeticItem& item = m_currentItems[static_cast<size_t>(m_selectedIndex)];
    if (item.Unlocked) return;

    if (item.UnlockType == CosmeticUnlockType::Shop)
    {
        const int coins = PowerUpManager::Get().GetCoins();
        if (coins >= item.Price)
        {
            ShowToast(item.Name + " - " + std::to_string(item.Price) + " CR (CLICK AGAIN TO BUY)", Color{255, 220, 80, 255});
        }
        else
        {
            ShowToast(item.Name + " - NEED " + std::to_string(item.Price) + " CR", Color{255, 115, 110, 255});
        }
    }
    else
    {
        const auto prog = ProfileManager::Get().GetCosmeticMilestoneProgress(item.Id);
        ShowToast(item.Name + " - " + item.UnlockRequirement + " (" + std::to_string(prog.Current) + "/" + std::to_string(prog.Target) + ")", Color{255, 190, 75, 255});
    }
}

void ProfileGarageState::RandomizeEquippedLoadout()
{
    static std::mt19937 rng(std::random_device{}());
    auto& pm = ProfileManager::Get();
    const std::array<CosmeticSlot, 6> slots = {
        CosmeticSlot::PrimaryColor,
        CosmeticSlot::SecondaryColor,
        CosmeticSlot::PaddleSkin,
        CosmeticSlot::BallSkin,
        CosmeticSlot::BallTrail,
        CosmeticSlot::DeathEffect
    };

    for (CosmeticSlot slot : slots)
    {
        auto items = pm.GetCosmeticsBySlot(slot);
        std::vector<std::string> unlockedIds;
        for (const auto& it : items)
        {
            if (it.Unlocked) unlockedIds.push_back(it.Id);
        }
        if (!unlockedIds.empty())
        {
            std::uniform_int_distribution<size_t> dist(0, unlockedIds.size() - 1);
            pm.EquipCosmetic(slot, unlockedIds[dist(rng)]);
        }
    }

    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    RefreshColorModalItems();
    SelectEquippedInColorModal();
    ShowToast("LOADOUT RANDOMIZED", GetPreviewPrimaryColor());
}

void ProfileGarageState::OpenColorModal(int channel)
{
    m_viewMode = ProfileViewMode::Garage;
    m_colorModalOpen = true;
    m_colorChannel = std::clamp(channel, 0, 1);
    m_colorHoveredIndex = -1;
    m_pressedColorSwatch = -1;
    RefreshColorModalItems();
    SelectEquippedInColorModal();
}

void ProfileGarageState::CloseColorModal()
{
    m_colorModalOpen = false;
    m_colorHoveredIndex = -1;
    m_pressedColorSwatch = -1;
}

bool ProfileGarageState::SwitchColorChannel(int channel)
{
    channel = std::clamp(channel, 0, 1);
    if (m_colorChannel == channel && !m_colorModalItems.empty())
    {
        return false;
    }
    m_colorChannel = channel;
    m_colorHoveredIndex = -1;
    m_pressedColorSwatch = -1;
    RefreshColorModalItems();
    SelectEquippedInColorModal();
    return true;
}

void ProfileGarageState::SetColorSelectedIndex(int idx)
{
    if (m_colorModalItems.empty())
    {
        m_colorSelectedIndex = -1;
        return;
    }
    m_colorSelectedIndex = std::clamp(idx, 0, static_cast<int>(m_colorModalItems.size()) - 1);
}

const CosmeticItem* ProfileGarageState::GetInspectedColorItem() const noexcept
{
    if (m_colorHoveredIndex >= 0 && m_colorHoveredIndex < static_cast<int>(m_colorModalItems.size()))
    {
        return &m_colorModalItems[static_cast<size_t>(m_colorHoveredIndex)];
    }
    if (m_colorSelectedIndex >= 0 && m_colorSelectedIndex < static_cast<int>(m_colorModalItems.size()))
    {
        return &m_colorModalItems[static_cast<size_t>(m_colorSelectedIndex)];
    }
    return nullptr;
}

ProfileActionResult ProfileGarageState::ActivateSelectedColor()
{
    if (m_colorSelectedIndex < 0 || m_colorSelectedIndex >= static_cast<int>(m_colorModalItems.size()))
    {
        return ProfileActionResult::None;
    }

    auto& pm = ProfileManager::Get();
    const CosmeticItem item = m_colorModalItems[static_cast<size_t>(m_colorSelectedIndex)];
    const CosmeticSlot slot = (m_colorChannel == 0) ? CosmeticSlot::PrimaryColor : CosmeticSlot::SecondaryColor;

    if (item.Unlocked)
    {
        if (pm.GetEquippedCosmetic(slot) == item.Id)
        {
            return ProfileActionResult::AlreadyEquipped;
        }
        pm.EquipCosmetic(slot, item.Id);
        RefreshColorModalItems();
        ShowToast(std::string(m_colorChannel == 0 ? "COL 1: " : "COL 2: ") + item.Name, item.SwatchColor);
        return ProfileActionResult::Equipped;
    }

    if (item.UnlockType == CosmeticUnlockType::Shop)
    {
        if (PowerUpManager::Get().GetCoins() >= item.Price)
        {
            if (pm.UnlockCosmetic(item.Id))
            {
                pm.EquipCosmetic(slot, item.Id);
                RefreshColorModalItems();
                ShowToast("UNLOCKED: " + item.Name, item.SwatchColor);
                return ProfileActionResult::UnlockedAndEquipped;
            }
        }
        const int missing = item.Price - PowerUpManager::Get().GetCoins();
        ShowToast("NEED " + std::to_string(missing) + " MORE CR", Color{255, 90, 90, 255});
        return ProfileActionResult::InsufficientFunds;
    }

    ShowToast("LOCKED: " + item.UnlockRequirement, Color{255, 185, 70, 255});
    return ProfileActionResult::MilestoneLocked;
}

void ProfileGarageState::NotifySelectedLockedColor()
{
    if (m_colorSelectedIndex < 0 || m_colorSelectedIndex >= static_cast<int>(m_colorModalItems.size())) return;
    const CosmeticItem& item = m_colorModalItems[static_cast<size_t>(m_colorSelectedIndex)];
    if (item.Unlocked) return;

    if (item.UnlockType == CosmeticUnlockType::Shop)
    {
        const int coins = PowerUpManager::Get().GetCoins();
        if (coins >= item.Price)
        {
            ShowToast(item.Name + " - " + std::to_string(item.Price) + " CR (CLICK AGAIN TO BUY)", Color{255, 220, 80, 255});
        }
        else
        {
            ShowToast(item.Name + " - NEED " + std::to_string(item.Price) + " CR", Color{255, 115, 110, 255});
        }
    }
    else
    {
        const auto prog = ProfileManager::Get().GetCosmeticMilestoneProgress(item.Id);
        ShowToast(item.Name + " - " + item.UnlockRequirement + " (" + std::to_string(prog.Current) + "/" + std::to_string(prog.Target) + ")", Color{255, 190, 75, 255});
    }
}

void ProfileGarageState::RandomizeEquippedColors()
{
    static std::mt19937 rng(std::random_device{}());
    auto& pm = ProfileManager::Get();
    for (CosmeticSlot slot : {CosmeticSlot::PrimaryColor, CosmeticSlot::SecondaryColor})
    {
        auto items = pm.GetCosmeticsBySlot(slot);
        std::vector<std::string> unlockedIds;
        for (const auto& it : items)
        {
            if (it.Unlocked) unlockedIds.push_back(it.Id);
        }
        if (!unlockedIds.empty())
        {
            std::uniform_int_distribution<size_t> dist(0, unlockedIds.size() - 1);
            pm.EquipCosmetic(slot, unlockedIds[dist(rng)]);
        }
    }
    RefreshColorModalItems();
    SelectEquippedInColorModal();
    ShowToast("COLORS RANDOMIZED", pm.GetEquippedPrimaryColor());
}

void ProfileGarageState::BeginNicknameEdit()
{
    m_editingNickname = true;
    m_nicknameBuffer = ProfileManager::Get().GetUsername();
}

void ProfileGarageState::AppendNicknameChars(const std::string& chars)
{
    for (char ch : chars)
    {
        if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t') continue;
        if (static_cast<unsigned char>(ch) >= 32 && m_nicknameBuffer.size() < 16)
        {
            m_nicknameBuffer.push_back(ch);
        }
    }
}

void ProfileGarageState::BackspaceNickname()
{
    if (!m_nicknameBuffer.empty())
    {
        m_nicknameBuffer.pop_back();
    }
}

void ProfileGarageState::CommitNicknameEdit()
{
    std::string trimmed = m_nicknameBuffer;
    while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
    if (trimmed.empty()) trimmed = "PLAYER";
    ProfileManager::Get().SetUsername(trimmed);
    m_nicknameBuffer = ProfileManager::Get().GetUsername();
    m_editingNickname = false;
    ShowToast("CALLSIGN UPDATED: " + m_nicknameBuffer, Color{255, 224, 85, 255});
}

void ProfileGarageState::CancelNicknameEdit()
{
    m_nicknameBuffer = ProfileManager::Get().GetUsername();
    m_editingNickname = false;
}

void ProfileGarageState::AddCareerScroll(float deltaY)
{
    m_careerScrollY = std::max(0.0f, m_careerScrollY + deltaY);
}

void ProfileGarageState::ClampCareerScroll(float maxScroll)
{
    m_careerScrollY = std::clamp(m_careerScrollY, 0.0f, std::max(0.0f, maxScroll));
}

bool ProfileGarageState::ArmOrPerformReset()
{
    if (!m_resetArmed)
    {
        m_resetArmed = true;
        m_resetArmTimer = 4.0f;
        ShowToast("CLICK AGAIN WITHIN 4S TO CONFIRM FULL RESET", Color{255, 110, 75, 255});
        return false;
    }
    PerformFullSaveReset();
    return true;
}

void ProfileGarageState::PerformFullSaveReset()
{
    PowerUpManager::Get().ResetToDefaults();
    ProfileManager::Get().ResetToDefaults();
    PlayerPrefs::Save();
    m_resetArmed = false;
    m_resetArmTimer = 0.0f;
    m_nicknameBuffer = ProfileManager::Get().GetUsername();
    RefreshCategoryItems();
    SelectEquippedInCurrentTab();
    RefreshColorModalItems();
    SelectEquippedInColorModal();
    ShowToast("SAVE DATA RESET TO DEFAULTS", Color{255, 110, 90, 255});
}

void ProfileGarageState::ShowToast(const std::string& text, Color color)
{
    m_toastText = text;
    m_toastColor = color;
    m_toastTimer = 2.2f;
}

Color ProfileGarageState::GetPreviewPrimaryColor() const
{
    const auto& pm = ProfileManager::Get();
    if (m_colorModalOpen && m_colorChannel == 0)
    {
        if (const CosmeticItem* it = GetInspectedColorItem())
        {
            return it->SwatchColor;
        }
    }
    return pm.GetEquippedPrimaryColor();
}

Color ProfileGarageState::GetPreviewSecondaryColor() const
{
    const auto& pm = ProfileManager::Get();
    if (m_colorModalOpen && m_colorChannel == 1)
    {
        if (const CosmeticItem* it = GetInspectedColorItem())
        {
            return it->SwatchColor;
        }
    }
    return pm.GetEquippedSecondaryColor();
}

PaddleSkinStyle ProfileGarageState::GetPreviewPaddleStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ProfileViewMode::Garage && !m_colorModalOpen && GetActiveSlot() == CosmeticSlot::PaddleSkin)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolvePaddleSkinStyle(it->Id);
        }
    }
    return pm.GetEquippedPaddleSkinStyle();
}

BallSkinStyle ProfileGarageState::GetPreviewBallStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ProfileViewMode::Garage && !m_colorModalOpen && GetActiveSlot() == CosmeticSlot::BallSkin)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolveBallSkinStyle(it->Id);
        }
    }
    return pm.GetEquippedBallSkinStyle();
}

TrailStyle ProfileGarageState::GetPreviewTrailStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ProfileViewMode::Garage && !m_colorModalOpen && GetActiveSlot() == CosmeticSlot::BallTrail)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolveTrailStyle(it->Id);
        }
    }
    return pm.GetEquippedTrailStyle();
}

DeathEffectStyle ProfileGarageState::GetPreviewDeathStyle() const
{
    const auto& pm = ProfileManager::Get();
    if (m_viewMode == ProfileViewMode::Garage && !m_colorModalOpen && GetActiveSlot() == CosmeticSlot::DeathEffect)
    {
        if (const CosmeticItem* it = GetInspectedItem())
        {
            return ProfileManager::ResolveDeathEffectStyle(it->Id);
        }
    }
    return pm.GetEquippedDeathEffectStyle();
}

void ProfileGarageState::RefreshCategoryItems()
{
    m_currentItems = ProfileManager::Get().GetCosmeticsBySlot(GetActiveSlot());
    if (m_currentItems.empty())
    {
        m_selectedIndex = -1;
        m_currentPage = 0;
    }
    else
    {
        m_selectedIndex = std::clamp(m_selectedIndex, 0, static_cast<int>(m_currentItems.size()) - 1);
        m_currentPage = std::clamp(m_currentPage, 0, GetPageCount() - 1);
    }
}

void ProfileGarageState::RefreshColorModalItems()
{
    const CosmeticSlot slot = (m_colorChannel == 0) ? CosmeticSlot::PrimaryColor : CosmeticSlot::SecondaryColor;
    m_colorModalItems = ProfileManager::Get().GetCosmeticsBySlot(slot);
    if (m_colorModalItems.empty())
    {
        m_colorSelectedIndex = -1;
    }
    else
    {
        m_colorSelectedIndex = std::clamp(m_colorSelectedIndex, 0, static_cast<int>(m_colorModalItems.size()) - 1);
    }
}

void ProfileGarageState::SelectEquippedInCurrentTab()
{
    const std::string equippedId = ProfileManager::Get().GetEquippedCosmetic(GetActiveSlot());
    m_selectedIndex = 0;
    for (int i = 0; i < static_cast<int>(m_currentItems.size()); ++i)
    {
        if (m_currentItems[static_cast<size_t>(i)].Id == equippedId)
        {
            m_selectedIndex = i;
            break;
        }
    }
    m_currentPage = std::clamp(m_selectedIndex / kItemsPerPage, 0, GetPageCount() - 1);
}

void ProfileGarageState::SelectEquippedInColorModal()
{
    const CosmeticSlot slot = (m_colorChannel == 0) ? CosmeticSlot::PrimaryColor : CosmeticSlot::SecondaryColor;
    const std::string equippedId = ProfileManager::Get().GetEquippedCosmetic(slot);
    m_colorSelectedIndex = 0;
    for (int i = 0; i < static_cast<int>(m_colorModalItems.size()); ++i)
    {
        if (m_colorModalItems[static_cast<size_t>(i)].Id == equippedId)
        {
            m_colorSelectedIndex = i;
            break;
        }
    }
}
