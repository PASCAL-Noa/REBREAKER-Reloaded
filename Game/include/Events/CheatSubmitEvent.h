#pragma once
#include "Events/Event.h"
#include <string>

struct CheatSubmitEvent : public Event
{
    std::string CheatCode;
    explicit CheatSubmitEvent(const std::string& code) : CheatCode(code) {}
};
