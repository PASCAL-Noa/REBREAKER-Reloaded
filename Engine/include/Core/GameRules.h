#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

enum class RuleAccess : uint8_t
{
    Public,
    Private
};

namespace Rule
{
    enum class Graphics : uint32_t
    {
        EnableShader = 0x1000,
        Fullscreen
    };

    enum class Gameplay : uint32_t
    {
        Invincible = 0x2000,
        InfiniteLives
    };

    enum class Debug : uint32_t
    {
        ShowCollider = 0x3000,
        ShowFPS
    };
}

struct RuleData
{
    std::string DisplayName;
    bool        Value;
    RuleAccess  Access;
    uint32_t    Category;
};

class GameRules
{
public:
    template<typename E>
    void RegisterRule(E id, const std::string& name, bool defaultValue, RuleAccess access)
    {
        uint32_t rawId = static_cast<uint32_t>(id);
        uint32_t category = rawId & 0xF000;
        m_rules[rawId] = { name, defaultValue, access, category };
    }

    template<typename E>
    void SetRule(E id, bool value)
    {
        uint32_t rawId = static_cast<uint32_t>(id);
        auto it = m_rules.find(rawId);
        if (it != m_rules.end())
        {
            it->second.Value = value;
        }
    }

    template<typename E>
    [[nodiscard]] bool GetRule(E id) const
    {
        uint32_t rawId = static_cast<uint32_t>(id);
        auto it = m_rules.find(rawId);
        if (it != m_rules.end())
        {
            return it->second.Value;
        }
        return false;
    }

    [[nodiscard]] std::vector<RuleData> GetPublicRules() const;

private:
    std::unordered_map<uint32_t, RuleData> m_rules;
};