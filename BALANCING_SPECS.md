# REBREAKER: Reloaded - Complete Balancing Specifications

> Comprehensive reference document detailing all numerical values, mechanics, durations, scaling formulas, perks, hazards, arena events, brick tiers, and anti-idle balancing mechanisms for **REBREAKER: Reloaded**.

---

## Table of Contents
1. [Endless Mode Wave Progression & Mathematical Formulas](#1-endless-mode-wave-progression--mathematical-formulas)
2. [Anti-Self-Play & Intra-Wave Difficulty Escalation](#2-anti-self-play--intra-wave-difficulty-escalation)
3. [Tactical Power-Ups (Workshop Levels 1 to 5)](#3-tactical-power-ups)
4. [Ultimate Abilities & Meter Economy](#4-ultimate-abilities--meter-economy)
5. [Roguelite Draft Perks (All 18 Perks)](#5-roguelite-draft-perks)
6. [Maluses & Tactical Hazard Zones](#6-maluses--tactical-hazard-zones)
7. [Dynamic Arena Events](#7-dynamic-arena-events)
8. [Brick Types & Armor Tier Durability](#8-brick-types--armor-tier-durability)
9. [Debug & Power-Up Tester Controls](#9-debug--power-up-tester-controls)

---

## 1. Endless Mode Wave Progression & Mathematical Formulas

Endless mode shifts away from a fixed brick-count target to a pure **survival endurance** model: waves advance strictly when the required survival duration has elapsed.

### Wave Target Duration Formula
Let $w \ge 1$ denote the current wave number. The target duration $T(w)$ in seconds is defined by:

$$
T(w) =
\begin{cases}
60.0\text{ s} & \text{if } w \le 1 \\
60.0 + \dfrac{w - 1}{99.0} \times 420.0\text{ s} & \text{if } 1 < w \le 100 \\
\min\Big(600.0\text{ s},\; 480.0 + (w - 100) \times 4.0\text{ s}\Big) & \text{if } w > 100
\end{cases}
$$

### Key Wave Milestones:
- **Wave 1**: $60.0\text{ s}$ ($1.0\text{ minute}$) - Warmup wave, 0 arena hazards.
- **Wave 10**: $98.2\text{ s}$ (~$1.6\text{ minutes}$) - First significant difficulty ramp.
- **Wave 25**: $161.8\text{ s}$ (~$2.7\text{ minutes}$) - Multi-hazard arenas and tier 2 armor.
- **Wave 50**: $267.9\text{ s}$ (~$4.5\text{ minutes}$) - Midpoint benchmark.
- **Wave 75**: $373.9\text{ s}$ (~$6.2\text{ minutes}$) - Dense armor and high creep frequency.
- **Wave 100**: $480.0\text{ s}$ ($8.0\text{ minutes}$) - Pinnacle endurance challenge.
- **Wave 130+**: $600.0\text{ s}$ ($10.0\text{ minutes}$) - Hard ceiling cap per wave.

---

## 2. Anti-Self-Play & Intra-Wave Difficulty Escalation

To prevent the game from "playing itself" when multiple automated power-ups (MultiBall, FireBall, Tesla Discharge) are acquired, several dynamic intra-wave mechanics ensure continuous action and paddle engagement.

Let $P = \dfrac{t_{\text{wave}}}{T(w)} \in [0.0, 1.0]$ denote the elapsed time progress of the current wave.

### 2.1 Dynamic Fluid Board Replenishment & Paddle Safety Clearance
- **Continuous Fluid Inflow**: The board is never empty. If all breakable bricks are cleared ($0$ remaining), $3$ full rows immediately spawn across the top ($Y = -350, -310, -270\text{ px}$) with zero delay.
- **Proactive Replenishment**: When breakable bricks drop below $14$, new rows seamlessly backfill from the top with a smooth $1.0\text{ s}$ rate limiter, ensuring uninterrupted brick flow.
- **Strict Paddle Safety Buffer**: The player's paddle resides at $Y = 300\text{ px}$. Downward grid movement is strictly capped at $Y_{\text{max}} = 85\text{ px}$, enforcing a minimum safety buffer of over $200\text{ px}$ above the paddle. Bricks never drop close to the paddle.
- **Top Row Backfill**: If the ceiling has already reached the maximum safe descent ($Y = 85\text{ px}$), new bricks materialize directly into unoccupied upper row slots without pushing existing bricks downwards.
- **Emergency Vaporize**: Any brick reaching $Y \ge 120\text{ px}$ is instantly vaporized with visual sparks.
- **HUD Cleanliness**: The brick counter is omitted from the HUD banner, focusing cleanly on Wave, Survival Time, Elapsed Time %, Score Multiplier, and Seed.

### 2.2 Intra-Wave Creep Acceleration
- Base downward creep interval decreases by wave: $I_{\text{base}}(w) = \max\Big(4.5\text{ s},\; (18.0 - (w - 1) \times 0.75) \times M_{\text{creep}}\Big)$.
- Modulated by intra-wave progress: $I(w, P) = I_{\text{base}}(w) \times (1.0 - 0.35 \times P)$.
- As the clock ticks down, the ceiling drops **up to 35% faster**, demanding active board clearance.

### 2.3 Intra-Wave Brick Hardening
- Procedural cell density and armor probability scale upwards as $P$ increases.
- Top rows dropped after $P \ge 0.35$ feature higher chances of Hard (3-hit) and Armored bricks.

### 2.4 Mid-Wave Hazard Injection ($P \ge 0.50$)
- At 50% wave progress, an arena hazard zone (e.g. UltimateDrain or ScreenGlitch) spawns dynamically at an unexpected location to disrupt passive paddle positioning.

### 2.5 Climax Rush ($P \ge 0.85$, Final 15%)
- Visual HUD warning: `[!] WAVE CLIMAX - FINAL SURGE ENGAGED [!]`.
- Minimum ball velocity is enforced at $580\text{ px/s}$: sluggish or looping balls receive an immediate kinetic nudge.
- Overdrive Capacitor perk triggers during this phase.

---

## 3. Tactical Power-Ups

All power-ups can be upgraded from **Level 1 to Level 5** in the Workshop using credits earned from playing.

### 3.1 Multi-Ball (SplitStream)
- **Tint**: Electric Cyan (`#00DCFF`)
- **Ejection Angle**: $\pm 30.0^\circ$
- **Max Active Balls**: 12 balls
- **Upgrade Cost**: $100 \times \text{Level}$ credits (100, 200, 300, 400, 500 CR)
- **Per-Level Stats**:
  - **Lv 1**: +2 balls per active ball (initial 3)
  - **Lv 2**: +3 balls per active ball (initial 4)
  - **Lv 3**: +4 balls per active ball (initial 5)
  - **Lv 4**: +5 balls per active ball (initial 6)
  - **Lv 5**: +6 balls per active ball (rapidly hits cap)

### 3.2 Expand Paddle
- **Tint**: Emerald Green (`#32FF50`)
- **Base Duration**: $10.0\text{ s} + 2.0\text{ s}$ per level
- **Width Multiplier**: $\times 1.20$ base ($+0.20$ per level)
- **Bonus Capsule Drop Chance**: $10\%$ base ($+10\%$ per level)
- **Per-Level Stats**:
  - **Lv 1**: Width $144\text{ px}$ ($\times 1.20$) | Duration: $10.0\text{ s}$
  - **Lv 2**: Width $168\text{ px}$ ($\times 1.40$) | Duration: $12.0\text{ s}$
  - **Lv 3**: Width $192\text{ px}$ ($\times 1.60$) | Duration: $14.0\text{ s}$ | 2 simultaneous drops
  - **Lv 4**: Width $216\text{ px}$ ($\times 1.80$) | Duration: $16.0\text{ s}$ | 2 simultaneous drops
  - **Lv 5**: Width $240\text{ px}$ ($\times 2.00$, double!) | Duration: $18.0\text{ s}$ | 3 simultaneous drops

### 3.3 Shrink Paddle (Agility Mode)
- **Tint**: Kinetic Red (`#FF3C3C`)
- **Width Multiplier**: $\times 0.65$ ($78\text{ px}$)
- **Base Duration**: $10.0\text{ s} + 2.0\text{ s}$ per level
- **Base Paddle Speed**: $700\text{ px/s}$
- **Per-Level Stats**:
  - **Lv 1**: Speed $+20\%$ ($840\text{ px/s}$) | Duration: $10.0\text{ s}$
  - **Lv 2**: Speed $+40\%$ ($980\text{ px/s}$) | Duration: $12.0\text{ s}$
  - **Lv 3**: Speed $+60\%$ ($1120\text{ px/s}$) | Duration: $14.0\text{ s}$
  - **Lv 4**: Speed $+80\%$ ($1260\text{ px/s}$) | Duration: $16.0\text{ s}$
  - **Lv 5**: Speed $+100\%$ ($1400\text{ px/s}$, supersonic) | Duration: $18.0\text{ s}$

### 3.4 Storm (Thunderstorm)
- **Tint**: Electric Gold (`#FFEB28`)
- **Lightning Radius**: $55.0\text{ px}$
- **Cooldown between strikes**: $0.55\text{ s}$ (Lv 1-4); $0.16\text{ s}$ (Lv 5 Tempest)
- **Per-Level Stats**:
  - **Lv 1**: 1 lightning bolt ($2\text{ HP}$ damage)
  - **Lv 2**: 2 successive bolts ($2\text{ HP}$ damage each)
  - **Lv 3**: 3 successive bolts ($2\text{ HP}$ damage each)
  - **Lv 4**: 3 supercharged bolts ($3\text{ HP}$ damage each)
  - **Lv 5 (Tempest Mode)**: 60 continuous strikes over $10.0\text{ s}$

### 3.5 Tempo Ball
- **Tint**: Plasma Violet (`#B446FF`)
- **Approach Velocity (Slow Zone)**: $280\text{ px/s}$ between $Y = 60\text{ px}$ and $Y = 280\text{ px}$
- **Per-Level Stats**:
  - **Lv 1**: Offensive speed $960\text{ px/s}$ ($+60\%$) | Paddle speed $+10\%$ | Duration: $10.0\text{ s}$
  - **Lv 2**: Offensive speed $1080\text{ px/s}$ ($+80\%$) | Paddle speed $+20\%$ | Duration: $11.0\text{ s}$
  - **Lv 3**: Offensive speed $1200\text{ px/s}$ ($+100\%$) | Paddle speed $+30\%$ | Duration: $12.0\text{ s}$
  - **Lv 4**: Offensive speed $1320\text{ px/s}$ ($+120\%$) | Paddle speed $+40\%$ | Duration: $13.0\text{ s}$
  - **Lv 5**: Offensive speed $1440\text{ px/s}$ ($+140\%$) | Paddle speed $+50\%$ | Duration: $14.0\text{ s}$

### 3.6 Extra Life
- **Heart Tier Palette**:
  - **1 to 3 hearts**: **Red** Hearts (Base Health)
  - **4 to 6 hearts**: **Yellow** Hearts (Energy Overcharge)
  - **7 to 9 hearts**: **Green** Hearts (Titan Plating)
- **Max Cap**: 9 lives (3 green hearts)
- **Per-Level Stats**:
  - **Lv 1**: +1 Life | Cap: 4 lives (unlocks 1st yellow heart)
  - **Lv 2**: +1 Life | Cap: 5 lives
  - **Lv 3**: +1 Yellow Heart guaranteed | Cap: 6 lives
  - **Lv 4**: +2 Yellow Hearts guaranteed | Cap: 7 lives (unlocks 1st green heart)
  - **Lv 5**: +2 Green Hearts | Cap: 9 lives

### 3.7 Big Ball
- **Tint**: Heavy Amber (`#FF6E14`)
- **Radius**: $40.0\text{ px}$ ($\times 2.0$ radius, $\times 4.0$ surface)
- **Piercing**: Smashes through light and medium bricks without deflection
- **Duration**: $10.0\text{ s} + 2.0\text{ s}$ per level ($10.0\text{ s}$ to $18.0\text{ s}$)

### 3.8 Fire Ball
- **Tint**: Incendiary Orange-Red (`#FF320A`)
- **Blast Radius**: $75.0\text{ px}$
- **Splash Damage**: Destroys surrounding bricks in radius
- **Duration**: $8.0\text{ s} + 2.0\text{ s}$ per level ($8.0\text{ s}$ to $16.0\text{ s}$)

### 3.9 Sticky Paddle
- **Tint**: Honey Amber (`#D79B1E`)
- **Hold Duration**: Up to $3.0\text{ s}$ before auto-launch
- **Power Duration**: $12.0\text{ s} + 2.0\text{ s}$ per level ($12.0\text{ s}$ to $20.0\text{ s}$)

### 3.10 Bottom Barrier
- **Tint**: Barrier Cyan (`#00C3F0`)
- **Position**: Spans entire bottom arena threshold
- **Hit Durability**: 2 ball bounces before collapsing (Lv 1-2), 3 bounces (Lv 3-4), 4 bounces (Lv 5)

### 3.11 Magnet Paddle
- **Tint**: Magnetic Magenta (`#BE32E6`)
- **Pull Force**: $240\text{ px/s}^2$ up to $560\text{ px/s}^2$ at Lv 5
- **Attraction Target**: Bonus capsules falling towards the floor

### 3.12 Ghost Ball
- **Tint**: Ethereal Blue (`#70C0E0`)
- **Mechanics**: Phases through standard bricks to strike the upper rows, phase-collapsing on hard bricks
- **Duration**: $8.0\text{ s} + 2.0\text{ s}$ per level ($8.0\text{ s}$ to $16.0\text{ s}$)

---

## 4. Ultimate Abilities & Meter Economy

- **Meter Size**: 100 energy units.
- **Charge Sources**:
  - Normal brick hit: $+1.5$ units.
  - Hard / Metal brick hit: $+3.0$ units.
  - Combo streak $> 5$: $+0.5$ bonus unit per hit.
  - Siphon perks / cards: $+5.0$ units per trigger.
- **Drain Hazards**:
  - UltimateDrain hazard zone drains $40.0$ units on touch.
  - EMP Ultimate Lockdown event locks meter activation for $45.0\text{ s}$.

---

## 5. Roguelite Draft Perks

Endless mode offers card drafts:
- **Periodic Supply**: Every $60.0\text{ s}$ of survival time. Standard / lower rarity cards.
- **Wave Clear**: Every 3 waves cleared (Wave 3, 6, 9...). Higher rarity chances.

| ID | Name | Rarity | Category | Description |
|:---|:---|:---|:---|:---|
| 0 | **TITAN CHASSIS** | Common | Stat Boost | +20% Permanent paddle width for maximum defensive reach. |
| 1 | **HYPER SERVO** | Common | Stat Boost | +15% Lateral paddle velocity for rapid response. |
| 2 | **HULL REINFORCEMENT** | Rare | Stat Boost | +1 Extra reserve life added immediately. |
| 3 | **KINETIC OVERDRIVE** | Uncommon | Stat Boost | +25% Score multiplier and increased ball kinetic momentum. |
| 4 | **HEAVY IMPACT** | Epic | Ball Modifier | +1 Ball damage per hit (destroys medium bricks in 1 hit). |
| 5 | **THERMAL ROUNDS** | Rare | Ball Modifier | Ignites active balls into incendiary explosive fireballs. |
| 6 | **TESLA DISCHARGE** | Epic | Buff Synergy | 25% chance on each impact to trigger chain lightning arcs. |
| 7 | **SPLIT STREAM** | Uncommon | Buff Synergy | Instantly deploys 2 additional kinetic balls into play. |
| 8 | **PLASMA DETONATION** | Epic | Buff Synergy | Direct impacts on hard bricks trigger explosive shockwaves. |
| 9 | **MAGNETIC TETHER** | Uncommon | Buff Synergy | Projects a magnetic field pulling bonus capsules to the paddle. |
| 10 | **AEGIS BARRIER** | Rare | Endless Utility | Deploys an energy barrier across the floor to prevent ball loss. |
| 11 | **REACTOR SURGE** | Legendary | Endless Utility | Instantly supercharges the Ultimate meter to 100% capacity. |
| 12 | **WAR CHEST BOUNTY** | Uncommon | Endless Utility | +250 Credits deposited directly into your Workshop vault. |
| 13 | **COMBAT MULTIPLIER** | Common | Endless Utility | +25% Permanent Endless score multiplier for this entire run. |
| 14 | **CHRONO DILATION** | Common | Endless Utility | Slows down time dilation and retards ceiling descent rate by 30%. |
| 15 | **OVERDRIVE CAPACITOR** | Rare | Tactical Boost | +15% Ball velocity and +1 bonus damage during wave climax. |
| 16 | **SHIELD SIPHON** | Uncommon | Energy Siphon | Breaking hardened bricks has a 20% chance to restore +5% Ultimate. |
| 17 | **VORTEX PULSE** | Epic | Arena Defense | Clearing brick clusters releases shockwaves that clear hazard zones. |

---

## 6. Maluses & Tactical Hazard Zones

Hazard zones spawn on the arena field. Balls and paddles passing through them receive immediate tactical debuffs:

1. **ScreenGlitch (`#00EBFF`)**:
   - Triggers chromatic optical aberration and triple-vision paddle projection.
   - Duration: $1.2\text{ s}$ to $2.0\text{ s}$.
2. **UltimateDrain (`#EB2DFF`)**:
   - Instantly siphons $40.0$ to $50.0$ units from the Ultimate ability charge meter.
   - Flashes purple warning flare.
3. **Dynamic Camera Tilt (`#FFB428`)**:
   - Tilts the gameplay camera view by $\pm 2.5^\circ$ for $1.2\text{ s}$ to $2.2\text{ s}$.
4. **Camera Zoom Pulse (`#B432C8`)**:
   - Pulses camera scale in and out by $\pm 5\%$ rhythmically for $1.2\text{ s}$.
5. **Audio Warp Distortion (`#5F37B9`)**:
   - Warps master audio pitch between $0.88\times$ and $1.15\times$ for acoustic disorientation.
6. **AllMaluses (`#DC234B`)**:
   - Triggers combined visual chromatic glitch, camera tilt, and temporal audio warp.

---

## 7. Dynamic Arena Events

Dynamic events trigger periodically (interval scales from $35\text{ s}$ down to $16\text{ s}$ in late waves):

- **TEMPORAL FLUX**: Slows down or accelerates arena time scale ($0.65\times$ or $1.4\times$).
- **GRAVITY SHIFT (LEFT / RIGHT / DOWN)**: Applies lateral or downward continuous acceleration force to all balls.
- **CAMERA EARTHQUAKE**: Violent camera shake ($12\text{ px}$ amplitude) simulating seismic shocks.
- **SOLAR FLARE / BLACKOUT**: Stadium lights dim drastically, forcing reliance on ball glow and paddle illumination.
- **METEOR SHOWER**: Incendiary debris rain across columns from ceiling to floor.
- **EMP ULTIMATE LOCKDOWN**: $45.0\text{ s}$ complete lockout of Ultimate activations. Blinking HUD status alert.

---

## 8. Brick Types & Armor Tier Durability

| Brick Type | Base HP | Armor Tier 0 | Armor Tier 1 (Wave 3+) | Armor Tier 2 (Wave 5+) | Armor Tier 3 (Wave 7+) |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Light** | 1 | 1 hit | 2 hits | 2 hits | 3 hits |
| **Medium** | 2 | 2 hits | 3 hits | 4 hits | 5 hits |
| **Hard** | 3 | 3 hits | 5 hits | 7 hits | 9 hits |
| **Bomb** | 1 | Explodes on contact, destroying neighbors in $75\text{ px}$ radius |
| **Special** | 1 | Guaranteed tactical capsule drop on destruction |
| **Metal** | $\infty$ | Unbreakable by standard balls; damaged only by lasers or high-tier penetrations |

---

## 9. Debug & Power-Up Tester Controls

Press **[TAB]** or **[T]** during any gameplay session to toggle the **Power-Up Tester Tool**:

- **[1] to [8]**: Instantly applies MultiBall, ExpandPaddle, ShrinkPaddle, Storm, TempoBall, ExtraLife, BigBall, FireBall.
- **[9]**: Applies StickyPaddle.
- **[0]**: Spawns BottomBarrier.
- **[M]**: Applies MagnetPaddle.
- **[H]**: Applies GhostBall.
- **[K]**: Applies GlitchMalus debuff.
- **[J]**: Spawns tactical HazardZone on field.
- **[E]**: Toggles / cycles Dynamic Arena Events immediately.
- **[U]**: Cycles all power-up levels $+1$ (Lv 1 -> 5).
- **[P]**: Drops tactical capsule directly above paddle.
- **[O]**: Drops all 12 capsules simultaneously.
- **[R]**: Resets ball and paddle to baseline state.
- **[B]**: Refills and regenerates the entire brick formation.
- **[N]**: Advances immediately to the next wave / stage.
- **[F3]**: Toggles full diagnostic HUD overlay (FPS, wave time, creep interval, active hazards, climax status).
