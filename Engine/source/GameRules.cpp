#include "Core/GameRules.h"

std::vector<RuleData> GameRules::GetPublicRules() const
{
    std::vector<RuleData> publicRules;
    for (const auto& [id, rule] : m_rules)
    {
        if (rule.Access == RuleAccess::Public)
        {
            publicRules.push_back(rule);
        }
    }
    return publicRules;
}