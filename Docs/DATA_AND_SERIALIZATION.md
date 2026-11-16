# Data Serialization & File Formats Manual

This document details the persistent storage architectures, JSON schemas, player progress tracking, and file serialization protocols in **REBREAKER-Reloaded**.

All data files are stored in human-readable, portable JSON using `nlohmann::json`.

---

## 1. Storage Directory Structure

When running, the game automatically initializes and maintains the `SaveData/` directory:

```
REBREAKER-Reloaded/
└── SaveData/
    ├── prefs.json          # Player preferences & keybindings
    ├── profile.json        # Permanent workshop progression & currency
    └── leaderboard.json    # High scores & stage records
```

---

## 2. Level Format Specification (JSON)

Campaign and custom stages are defined as JSON files stored in `Resources/levels/level_<N>.json`.

### 2.1 JSON Schema Specification

```json
{
  "name": "Neon Citadel",
  "author": "Antigravity Team",
  "theme": "Cyberpunk",
  "music": "Resources/audio/music/Stage-Cyberpunk.ogg",
  "grid": {
    "columns": 14,
    "rows": 10,
    "brickWidth": 90.0,
    "brickHeight": 32.0,
    "spacingX": 8.0,
    "spacingY": 8.0,
    "offsetX": 0.0,
    "offsetY": -160.0
  },
  "bricks": [
    {
      "col": 0,
      "row": 0,
      "type": "Hard",
      "durability": 3,
      "score": 300
    },
    {
      "col": 1,
      "row": 0,
      "type": "Normal",
      "durability": 1,
      "score": 100
    },
    {
      "col": 2,
      "row": 0,
      "type": "Bomb",
      "durability": 1,
      "score": 250
    },
    {
      "col": 3,
      "row": 0,
      "type": "Unbreakable",
      "durability": -1,
      "score": 0
    }
  ]
}
```

### 2.2 Schema Definitions

| Field | Type | Description |
|---|---|---|
| `name` | string | Human-readable stage title displayed in the level carousel and HUD. |
| `author` | string | Author credit tag. |
| `theme` | string | Palette theme (`Cyberpunk`, `RetroSynth`, `ToxicLab`, `CosmicVoid`). Dictates background shaders and particle colors. |
| `music` | string | Relative path to background music stream asset. |
| `grid.columns` | int | Total number of horizontal brick slots ($10$--$20$). |
| `grid.rows` | int | Total number of vertical brick slots ($6$--$15$). |
| `grid.brickWidth` | float | Brick collider width in world units ($80.0$--$110.0$). |
| `grid.brickHeight` | float | Brick collider height in world units ($28.0$--$40.0$). |
| `bricks[].type` | string | Enum: `Normal`, `Hard`, `Unbreakable`, `Bomb`, `Item`, `Speed`, `Hazard`. |
| `bricks[].durability`| int | Number of hits required to break ($-1$ indicates infinite/unbreakable). |
| `bricks[].score` | int | Base score awarded on final destruction. |

---

## 3. Player Preferences (`PlayerPrefs`)

Located in `Core/include/Core/PlayerPrefs.h`, `PlayerPrefs` manages runtime settings serialized to `SaveData/prefs.json`.

```mermaid
graph LR
    Subsystem["SettingsUI / InputRemap"] -->|SetInt / SetFloat / SetBool| PP["PlayerPrefs Cache"]
    PP -->|PlayerPrefs::Save()| File["SaveData/prefs.json"]
    File -->|PlayerPrefs::Load()| PP
```

### 3.1 Preference Keys Dictionary

| Key Name | Data Type | Default | Description |
|---|---|---|---|
| `MasterVolume` | float | `80.0` | Global master audio volume ($0.0$--$100.0$). |
| `MusicVolume` | float | `70.0` | BGM streaming volume ($0.0$--$100.0$). |
| `SfxVolume` | float | `85.0` | SFX voice pool volume ($0.0$--$100.0$). |
| `AudioDevice` | string | `""` | Selected playback hardware identifier (empty for OS default). |
| `Fullscreen` | bool | `false` | Fullscreen display mode toggle. |
| `ResWidth` | int | `1920` | Window horizontal resolution. |
| `ResHeight` | int | `1080` | Window vertical resolution. |
| `ShadersEnabled` | bool | `true` | CRT barrel curvature and scanline shader toggle. |
| `ParticlesEnabled`| bool | `true` | Dynamic particle emitters toggle. |
| `KeyBind_MoveLeft` | int | KeyCode | Keyboard key code for moving left. |
| `KeyBind_MoveRight`| int | KeyCode | Keyboard key code for moving right. |
| `KeyBind_Launch` | int | KeyCode | Keyboard key code for launching ball / firing lasers. |
| `KeyBind_Pause` | int | KeyCode | Keyboard key code for pausing the game. |
| `JoyBind_MoveLeft` | int | Joy Button | Gamepad button/axis index for moving left. |
| `JoyBind_MoveRight`| int | Joy Button | Gamepad button/axis index for moving right. |
| `JoyBind_Launch` | int | Joy Button | Gamepad button index for launching ball. |
| `JoyBind_Pause` | int | Joy Button | Gamepad button index for pausing. |

---

## 4. Profile Progression (`ProfileManager`)

Located in `Core/include/Core/ProfileManager.h`, `ProfileManager` tracks player progression, unlocks, and currency balances across sessions.

### 4.1 Schema (`SaveData/profile.json`)

```json
{
  "currency": 12500,
  "lifetimeBricksBroken": 3482,
  "highScore": 184500,
  "highestLevel": 12,
  "powerUpTiers": {
    "MultiBall": 3,
    "FireBall": 2,
    "Laser": 4,
    "PaddleExpand": 5,
    "Sticky": 1,
    "Shield": 3
  },
  "ultimateLevels": {
    "PlasmaOrb": 2,
    "DiscoBall": 1,
    "PowerUpRain": 3
  }
}
```

### 4.2 Progression Mechanics
- **Currency (`Bank`)**: Earned by destroying bricks ($+10$ per normal, $+25$ per hard, $+50$ per bomb) and completing levels ($+1,000$ completion bonus).
- **Power-Up Tier Upgrades**: Each upgrade level (1 to 5) scales duration ($+1.5\text{s}$ per tier) or effect magnitude (e.g. laser fire rate, shield durability).
- **Ultimate Ability Upgrades**: Increases destructive radius, duration, and projectile counts.

---

## 5. Leaderboard Records (`LeaderboardManager`)

Located in `Game/include/LeaderboardManager.h`, manages high score tables stored in `SaveData/leaderboard.json`.

### 5.1 Schema (`SaveData/leaderboard.json`)

```json
{
  "records": [
    {
      "rank": 1,
      "playerName": "CYBER_ACE",
      "score": 245800,
      "levelReached": 15,
      "date": "2026-11-16T12:00:00Z"
    },
    {
      "rank": 2,
      "playerName": "NOA",
      "score": 198200,
      "levelReached": 14,
      "date": "2026-11-16T11:45:00Z"
    }
  ]
}
```

### 5.2 Ranking Algorithm
- Records are sorted descending by `score`.
- Tie-breaker: higher `levelReached`, followed by earliest timestamp.
- High score tables keep the Top 10 records persistently.

