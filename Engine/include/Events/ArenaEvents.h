#pragma once
#include "Events/Event.h"
#include "Data/Color.h"
#include <string>

enum class ArenaEventType : int
{
    None = 0,
    TemporalFlux = 1,
    GravityShiftLeft = 2,
    GravityShiftRight = 3,
    GravityShiftDown = 4,
    CameraEarthquake = 5,
    SolarFlareBlackout = 6,
    MeteorShower = 7,
    UltimateLockdown = 8,
    Count = 9,

    // Backward compatibility aliases
    TemporalFluxSlow = TemporalFlux,
    TemporalFluxFast = TemporalFlux
};

enum class ArenaEventState : int
{
    Idle = 0,
    Warning,
    Active,
    Cooldown
};

#include <vector>

struct ArenaEventDescriptor
{
    ArenaEventType Type = ArenaEventType::None;
    const char*    Id = "none";
    const char*    DisplayName = "NONE";
    const char*    ShortLabel = "NONE";
    const char*    Description = "Arena is stable";
    Color          AccentColor = Colors::White;
};

inline const std::vector<ArenaEventDescriptor>& GetArenaEventCatalog()
{
    static const std::vector<ArenaEventDescriptor> catalog = {
        {
            ArenaEventType::TemporalFlux,
            "temporal_flux",
            "Temporal Flux",
            "TEMPO",
            "Alternating slow-motion and high-speed time warp",
            Color{180, 110, 255, 255}
        },
        {
            ArenaEventType::GravityShiftLeft,
            "gravity_left",
            "Gravity Shift (Left)",
            "GRAV L",
            "Arena tilts and pulls active balls to the left",
            Color{255, 175, 45, 255}
        },
        {
            ArenaEventType::GravityShiftRight,
            "gravity_right",
            "Gravity Shift (Right)",
            "GRAV R",
            "Arena tilts and pulls active balls to the right",
            Color{255, 175, 45, 255}
        },
        {
            ArenaEventType::GravityShiftDown,
            "gravity_down",
            "Heavy Gravity",
            "GRAVITY",
            "Heavy downward gravity pulls active balls",
            Color{255, 140, 30, 255}
        },
        {
            ArenaEventType::CameraEarthquake,
            "earthquake",
            "Camera Earthquake",
            "QUAKE",
            "Seismic tremor agitates ball trajectories",
            Color{255, 65, 80, 255}
        },
        {
            ArenaEventType::SolarFlareBlackout,
            "solar_blackout",
            "Solar Blackout",
            "ECLIPSE",
            "Arena lights cut out with sweeping searchlights",
            Color{255, 230, 90, 255}
        },
        {
            ArenaEventType::MeteorShower,
            "meteor_shower",
            "Meteor Shower",
            "METEORS",
            "Falling meteors rain across the arena",
            Color{255, 100, 35, 255}
        },
        {
            ArenaEventType::UltimateLockdown,
            "emp_lockdown",
            "EMP Lockdown",
            "EMP LOCK",
            "Electromagnetic pulse disables Ultimate abilities",
            Color{0, 235, 255, 255}
        }
    };
    return catalog;
}

inline const ArenaEventDescriptor* FindArenaEventDescriptor(ArenaEventType type)
{
    for (const auto& desc : GetArenaEventCatalog())
    {
        if (desc.Type == type)
        {
            return &desc;
        }
    }
    return nullptr;
}

inline const ArenaEventDescriptor* FindArenaEventDescriptorById(const std::string& id)
{
    for (const auto& desc : GetArenaEventCatalog())
    {
        if (id == desc.Id)
        {
            return &desc;
        }
    }
    return nullptr;
}

inline const char* GetArenaEventName(ArenaEventType type)
{
    switch (type)
    {
        case ArenaEventType::TemporalFlux:       return "TEMPORAL FLUX";
        case ArenaEventType::GravityShiftLeft:   return "GRAVITY SHIFT (LEFT)";
        case ArenaEventType::GravityShiftRight:  return "GRAVITY SHIFT (RIGHT)";
        case ArenaEventType::GravityShiftDown:   return "GRAVITY SHIFT (DOWN)";
        case ArenaEventType::CameraEarthquake:   return "CAMERA EARTHQUAKE";
        case ArenaEventType::SolarFlareBlackout: return "SOLAR FLARE / BLACKOUT";
        case ArenaEventType::MeteorShower:       return "METEOR SHOWER";
        case ArenaEventType::UltimateLockdown:   return "EMP ULTIMATE LOCKDOWN";
        default:                                 return "NONE";
    }
}

inline const char* GetArenaEventDescription(ArenaEventType type)
{
    if (const auto* desc = FindArenaEventDescriptor(type))
    {
        return desc->Description;
    }
    return "Arena is stable";
}

inline Color GetArenaEventColor(ArenaEventType type)
{
    if (const auto* desc = FindArenaEventDescriptor(type))
    {
        return desc->AccentColor;
    }
    return Colors::White;
}

struct ArenaEventWarningStartedEvent : public Event
{
    ArenaEventType EventType;
    float          WarningDuration;

    ArenaEventWarningStartedEvent(ArenaEventType type, float duration)
        : EventType(type), WarningDuration(duration) {}
};

struct ArenaEventActivatedEvent : public Event
{
    ArenaEventType EventType;
    float          EventDuration;

    ArenaEventActivatedEvent(ArenaEventType type, float duration)
        : EventType(type), EventDuration(duration) {}
};

struct ArenaEventEndedEvent : public Event
{
    ArenaEventType EventType;

    explicit ArenaEventEndedEvent(ArenaEventType type)
        : EventType(type) {}
};
