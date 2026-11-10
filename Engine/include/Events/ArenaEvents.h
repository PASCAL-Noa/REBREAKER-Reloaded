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
    Count = 8,

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
        default:                                 return "NONE";
    }
}

inline const char* GetArenaEventDescription(ArenaEventType type)
{
    switch (type)
    {
        case ArenaEventType::TemporalFlux:       return "Dynamic multi-phase time warp alternating between slow-motion and high speed";
        case ArenaEventType::GravityShiftLeft:   return "Dynamic arena tilt pulling active balls to the left";
        case ArenaEventType::GravityShiftRight:  return "Dynamic arena tilt pulling active balls to the right";
        case ArenaEventType::GravityShiftDown:   return "Heavy gravity pulling active balls downwards";
        case ArenaEventType::CameraEarthquake:   return "Violent camera vibration and turbulent ball agitation";
        case ArenaEventType::SolarFlareBlackout: return "Dynamic darkness sweep and sweeping searchlights";
        case ArenaEventType::MeteorShower:       return "Environmental meteor projectiles raining from above";
        default:                                 return "Arena is stable";
    }
}

inline Color GetArenaEventColor(ArenaEventType type)
{
    switch (type)
    {
        case ArenaEventType::TemporalFlux:       return Color{180, 110, 255, 255}; // Deep Neon Violet
        case ArenaEventType::GravityShiftLeft:   return Color{255, 175, 45, 255};  // Amber
        case ArenaEventType::GravityShiftRight:  return Color{255, 175, 45, 255};  // Amber
        case ArenaEventType::GravityShiftDown:   return Color{255, 140, 30, 255};  // Deep Orange
        case ArenaEventType::CameraEarthquake:   return Color{255, 65, 80, 255};   // Crimson Red
        case ArenaEventType::SolarFlareBlackout: return Color{255, 230, 90, 255};  // Solar Gold
        case ArenaEventType::MeteorShower:       return Color{255, 100, 35, 255};  // Fiery Orange
        default:                                 return Colors::White;
    }
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
