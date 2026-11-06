#!/usr/bin/env python3
"""
REBREAKER-Reloaded 2D Sprite Asset Generator (#88)
Generates:
1. Glass Celestial Spheres / Energy Orbs (48x48) for in-game falling items with translucent crystal bubbles & glowing inner artifacts (NO grey pastille).
2. Workshop Hero Emblems (128x128) - Vibrant, high-definition standalone mobile illustrations with elemental aura and specular gloss (NO grey pastille).
3. Stage Hazards (Electric arc, gravity distortion, caution grid).
"""

import os
import math
from PIL import Image, ImageDraw

OUTPUT_DIR = "Resources/sprite"
ORB_DIR = os.path.join(OUTPUT_DIR, "orbs")
WORKSHOP_DIR = os.path.join(OUTPUT_DIR, "workshop")
HAZARD_DIR = os.path.join(OUTPUT_DIR, "hazards")

os.makedirs(ORB_DIR, exist_ok=True)
os.makedirs(WORKSHOP_DIR, exist_ok=True)
os.makedirs(HAZARD_DIR, exist_ok=True)

SCALE = 4  # 4x supersampling

def create_canvas(w, h):
    return Image.new("RGBA", (w * SCALE, h * SCALE), (0, 0, 0, 0))

def finalize(img, path, target_size):
    downscaled = img.resize(target_size, Image.Resampling.LANCZOS)
    downscaled.save(path, "PNG", optimize=True)
    print(f"Generated: {path} ({target_size[0]}x{target_size[1]})")

# ==============================================================================
# DRAW HIGH-IMPACT STYLIZED HERO ARTIFACTS (FULLY SCALABLE)
# ==============================================================================

def draw_inner_item(d, cx, cy, item_type, theme, scale=1.0):
    r, g, b = theme

    def S(val):
        return int(round(val * scale))
    def pt(x, y):
        return (cx + S(x), cy + S(y))
    def pts(arr):
        return [(cx + S(x), cy + S(y)) for (x, y) in arr]
    def box(x1, y1, x2, y2):
        return [cx + S(x1), cy + S(y1), cx + S(x2), cy + S(y2)]
    def W(w):
        return max(1, int(round(w * scale)))

    if item_type == "storm":
        # Crackling electric lightning bolt with energetic plasma arcs and sparks
        bolt_glow = pts([
            (14, -68), (-38, -6), (-8, -6),
            (-32, 68), (38, 6), (8, 6), (28, -68)
        ])
        d.polygon(bolt_glow, fill=(255, 235, 40, 95))
        # Outer vibrant rim
        bolt_outer = pts([
            (12, -64), (-34, -5), (-6, -5),
            (-28, 64), (34, 5), (6, 5), (25, -64)
        ])
        d.polygon(bolt_outer, fill=(255, 210, 0, 255), outline=(255, 255, 255, 255))
        # Main solid lightning bolt
        bolt_main = pts([
            (10, -58), (-28, -3), (-4, -3),
            (-22, 58), (28, 3), (4, 3), (20, -58)
        ])
        d.polygon(bolt_main, fill=(255, 245, 60, 255))
        # Inner superheated white plasma core
        bolt_core = pts([
            (7, -50), (-18, -1), (-2, -1),
            (-14, 50), (18, 1), (2, 1), (15, -50)
        ])
        d.polygon(bolt_core, fill=(255, 255, 255, 255))
        # Floating electric sparks & lightning bolts around
        for sx, sy in [(-44, -32), (44, -22), (-40, 36), (40, 32), (0, -62), (0, 62)]:
            d.ellipse(box(sx - 6, sy - 6, sx + 6, sy + 6), fill=(255, 255, 200, 240))
            d.line([pt(sx - 10, sy), pt(sx + 10, sy)], fill=(255, 255, 255, 255), width=W(3))
            d.line([pt(sx, sy - 10), pt(sx, sy + 10)], fill=(255, 255, 255, 255), width=W(3))

    elif item_type == "multiball":
        # 3 Major Celestial Energy Spheres with hyper-dense energy matrix bonds
        d.line([pt(-42, 24), pt(42, 24)], fill=(0, 220, 255, 140), width=W(16))
        d.line([pt(-42, 24), pt(0, -42)], fill=(0, 220, 255, 140), width=W(16))
        d.line([pt(42, 24), pt(0, -42)], fill=(0, 220, 255, 140), width=W(16))
        d.line([pt(-42, 24), pt(42, 24)], fill=(255, 255, 255, 240), width=W(5))
        d.line([pt(-42, 24), pt(0, -42)], fill=(255, 255, 255, 240), width=W(5))
        d.line([pt(42, 24), pt(0, -42)], fill=(255, 255, 255, 240), width=W(5))
        # Central reactor spark
        d.ellipse(box(-12, -2, 12, 20), fill=(255, 255, 255, 230))

        # 3 High-Energy Spheres with 3D gradient shading & specular gloss
        for bx, by, br in [(-42, 24, 26), (42, 24, 26), (0, -42, 26)]:
            d.ellipse(box(bx - br - 8, by - br - 8, bx + br + 8, by + br + 8), fill=(0, 180, 255, 95))
            d.ellipse(box(bx - br, by - br, bx + br, by + br), fill=(0, 210, 255, 255), outline=(255, 255, 255, 255), width=W(5))
            d.ellipse(box(bx - br + 5, by - br + 5, bx + 2, by + 2), fill=(255, 255, 255, 240))

    elif item_type == "fireball":
        # Raging Hellfire Meteor with layered flames and burning embers
        flame_outer = pts([
            (0, -72), (28, -34), (54, 0),
            (42, 46), (18, 64), (-18, 64),
            (-42, 46), (-54, 0), (-28, -34)
        ])
        d.polygon(flame_outer, fill=(255, 50, 10, 255), outline=(255, 170, 30, 255))
        flame_mid = pts([
            (0, -48), (20, -16), (34, 16),
            (16, 48), (-16, 48),
            (-34, 16), (-20, -16)
        ])
        d.polygon(flame_mid, fill=(255, 150, 20, 255))
        flame_core = pts([
            (0, -26), (14, 0), (12, 32),
            (0, 42), (-12, 32), (-14, 0)
        ])
        d.polygon(flame_core, fill=(255, 250, 140, 255), outline=(255, 255, 255, 255))
        for fx, fy in [(-34, -55), (32, -60), (48, -26), (-44, -18)]:
            d.ellipse(box(fx - 5, fy - 5, fx + 5, fy + 5), fill=(255, 210, 50, 255))

    elif item_type == "bigball":
        # Colossal Heavy Metallic Core with Reinforced Titanium Spikes & Neon Seams
        d.ellipse(box(-54, -54, 54, 54), fill=(255, 110, 20, 255), outline=(255, 230, 170, 255), width=W(7))
        d.ellipse(box(-38, -38, 38, 38), outline=(255, 245, 210, 220), width=W(4))
        for off in [(-18, -12), (18, -12), (0, 22)]:
            d.ellipse(box(off[0] - 9, off[1] - 9, off[0] + 9, off[1] + 9), fill=(140, 35, 0, 255), outline=(255, 190, 60, 255), width=W(3))
        for deg in [0, 60, 120, 180, 240, 300]:
            rad = math.radians(deg)
            nx = 54 * math.cos(rad)
            ny = 54 * math.sin(rad)
            d.ellipse(box(nx - 7, ny - 7, nx + 7, ny + 7), fill=(255, 250, 190, 255))

    elif item_type == "ghost":
        # Luminous Cyber Spectral Wraith with haunting glowing eyes
        ghost_body = [
            (-40, -18), (-40, 42), (-26, 26),
            (-14, 48), (0, 28), (14, 48),
            (26, 26), (40, 42), (40, -18)
        ]
        d.polygon(pts(ghost_body), fill=(160, 240, 255, 130))
        d.ellipse(box(-40, -56, 40, 14), fill=(160, 240, 255, 130))
        d.polygon(pts([(p[0], p[1] - 4) for p in ghost_body]), fill=(210, 248, 255, 250), outline=(255, 255, 255, 255), width=W(3))
        d.ellipse(box(-36, -52, 36, 10), fill=(210, 248, 255, 250))
        # Piercing glowing cyber eyes
        d.ellipse(box(-20, -26, -6, -8), fill=(15, 30, 75, 255))
        d.ellipse(box(6, -26, 20, -8), fill=(15, 30, 75, 255))
        d.ellipse(box(-17, -24, -9, -12), fill=(0, 240, 255, 255))
        d.ellipse(box(9, -24, 17, -12), fill=(0, 240, 255, 255))

    elif item_type == "tempo":
        # Chrono-Warp Quantum Pocketwatch / Time Dilation Ring
        d.ellipse(box(-52, -52, 52, 52), fill=(40, 15, 65, 255), outline=(190, 90, 255, 255), width=W(8))
        d.ellipse(box(-38, -38, 38, 38), outline=(240, 170, 255, 220), width=W(4))
        for deg in range(0, 360, 30):
            rad = math.radians(deg)
            r1, r2 = 32, 45
            d.line([pt(r1 * math.cos(rad), r1 * math.sin(rad)), pt(r2 * math.cos(rad), r2 * math.sin(rad))], fill=(255, 255, 255, 255), width=W(3))
        d.line([pt(0, 0), pt(0, -36)], fill=(255, 255, 255, 255), width=W(7))
        d.line([pt(0, 0), pt(24, 6)], fill=(255, 220, 60, 255), width=W(6))
        d.ellipse(box(-8, -8, 8, 8), fill=(255, 255, 255, 255))

    elif item_type == "expand":
        # Ultra-Wide High-Tech Shielded Paddle with outward holographic thrusters
        d.rounded_rectangle(box(-56, -20, 56, 20), radius=S(8), fill=(40, 225, 75, 255), outline=(255, 255, 255, 255), width=W(4))
        d.line([pt(-48, 0), pt(48, 0)], fill=(210, 255, 220, 220), width=W(4))
        d.polygon(pts([(-82, 0), (-60, -26), (-60, 26)]), fill=(130, 255, 160, 255), outline=(255, 255, 255, 255))
        d.polygon(pts([(82, 0), (60, -26), (60, 26)]), fill=(130, 255, 160, 255), outline=(255, 255, 255, 255))

    elif item_type == "shrink":
        # Compact Hyper-Dense Carbon Vessel with Inward Kinetic Pressure Cones
        d.rounded_rectangle(box(-26, -20, 26, 20), radius=S(6), fill=(245, 50, 50, 255), outline=(255, 255, 255, 255), width=W(4))
        d.polygon(pts([(-36, 0), (-64, -26), (-64, 26)]), fill=(255, 145, 145, 255), outline=(255, 255, 255, 255))
        d.polygon(pts([(36, 0), (64, -26), (64, 26)]), fill=(255, 145, 145, 255), outline=(255, 255, 255, 255))

    elif item_type == "extralife":
        # Brilliant Cyber Heart with pulse facets
        heart_pts = []
        for deg in range(0, 360, 8):
            t = math.radians(deg)
            hx = 16 * (math.sin(t) ** 3)
            hy = -(13 * math.cos(t) - 5 * math.cos(2*t) - 2 * math.cos(3*t) - math.cos(4*t))
            heart_pts.append((hx * 3.4, hy * 3.3 + 6))
        d.polygon(pts(heart_pts), fill=(255, 45, 120, 255), outline=(255, 255, 255, 255), width=W(4))
        inner_pts = [(p[0] * 0.65, (p[1] - 6) * 0.65 + 6) for p in heart_pts]
        d.polygon(pts(inner_pts), fill=(255, 150, 200, 255))
        d.ellipse(box(-30, -34, -12, -18), fill=(255, 255, 255, 240))

    elif item_type == "sticky":
        # Viscous Super-Adhesive Amber Resin Drop with honey matrix
        drop = pts([
            (0, -60), (22, -20), (46, 20),
            (30, 54), (-30, 54), (-46, 20), (-22, -20)
        ])
        d.polygon(drop, fill=(250, 175, 30, 255), outline=(255, 245, 170, 255), width=W(4))
        d.ellipse(box(-20, 6, 4, 38), fill=(255, 240, 130, 220))
        d.ellipse(box(16, 24, 28, 40), fill=(255, 255, 255, 240))

    elif item_type == "barrier":
        # High-Voltage Hexagonal Energy Shield Grid
        d.line([pt(-72, 26), pt(72, 26)], fill=(0, 240, 255, 255), width=W(11))
        d.line([pt(-72, 26), pt(72, 26)], fill=(255, 255, 255, 255), width=W(4))
        for off in [-36, 0, 36]:
            chev = [pt(off - 20, 14), pt(off, -26), pt(off + 20, 14)]
            d.line(chev, fill=(0, 240, 255, 255), width=W(7))
            d.line(chev, fill=(255, 255, 255, 255), width=W(3))

    elif item_type == "magnet":
        # Supercharged Gravitational Horseshoe Magnet with Polarized Aura
        d.arc(box(-46, -28, 46, 54), 0, 180, fill=(220, 50, 255, 255), width=W(24))
        d.line([pt(-34, 0), pt(-34, -38)], fill=(220, 50, 255, 255), width=W(24))
        d.line([pt(-34, -38), pt(-34, -56)], fill=(255, 50, 80, 255), width=W(24))
        d.line([pt(34, 0), pt(34, -38)], fill=(220, 50, 255, 255), width=W(24))
        d.line([pt(34, -38), pt(34, -56)], fill=(0, 190, 255, 255), width=W(24))
        d.rectangle(box(-46, -58, -22, -54), fill=(255, 255, 255, 255))
        d.rectangle(box(22, -58, 46, -54), fill=(255, 255, 255, 255))
        d.arc(box(-26, -72, 26, -38), 180, 360, fill=(255, 255, 255, 240), width=W(5))

    elif item_type == "regen":
        # Perpetual Motion Eco-Crystal Loop
        d.arc(box(-46, -46, 46, 46), 25, 155, fill=(30, 255, 175, 255), width=W(11))
        d.arc(box(-46, -46, 46, 46), 205, 335, fill=(30, 255, 175, 255), width=W(11))
        d.polygon(pts([(-28, -48), (-56, -48), (-42, -28)]), fill=(30, 255, 175, 255))
        d.polygon(pts([(28, 48), (56, 48), (42, 28)]), fill=(30, 255, 175, 255))
        d.polygon(pts([(0, -22), (20, 0), (0, 22), (-20, 0)]), fill=(255, 255, 255, 255), outline=(100, 255, 210, 255), width=W(3))

    elif item_type == "glitch":
        # Chromatic Cyber Skull with holographic displacement
        d.ellipse(box(-38, -50, 32, 10), fill=(0, 220, 255, 160))
        d.rectangle(box(-24, 2, 18, 38), fill=(0, 220, 255, 160))
        d.ellipse(box(-34, -48, 36, 12), fill=(255, 30, 90, 255), outline=(255, 255, 255, 255))
        d.rectangle(box(-20, 4, 22, 40), fill=(255, 30, 90, 255))
        d.ellipse(box(-18, -24, -4, -6), fill=(10, 10, 20, 255))
        d.ellipse(box(6, -24, 20, -6), fill=(10, 10, 20, 255))
        d.line([pt(-66, 10), pt(66, 10)], fill=(255, 255, 255, 255), width=W(4))

# ==============================================================================
# 1. CELESTIAL ENERGY SPHERES (48x48) FOR IN-GAME FALLING ITEMS
# Crystal glass bubble with glowing rim and specular glint (NO grey disc)
# ==============================================================================

def generate_spheres():
    items = [
        ("multiball", "orb_multiball.png", (0, 220, 255)),
        ("expand",    "orb_expand.png",    (50, 255, 80)),
        ("shrink",    "orb_shrink.png",    (255, 60, 60)),
        ("storm",     "orb_storm.png",     (255, 235, 40)),
        ("tempo",     "orb_tempo.png",     (180, 70, 255)),
        ("extralife", "orb_extralife.png", (255, 100, 180)),
        ("bigball",   "orb_bigball.png",   (255, 120, 20)),
        ("fireball",  "orb_fireball.png",  (255, 60, 20)),
        ("sticky",    "orb_sticky.png",    (245, 175, 40)),
        ("barrier",   "orb_barrier.png",   (0, 240, 255)),
        ("magnet",    "orb_magnet.png",    (220, 60, 255)),
        ("ghost",     "orb_ghost.png",     (160, 240, 255)),
        ("regen",     "orb_regen.png",     (32, 255, 176)),
        ("glitch",    "orb_glitch.png",    (255, 30, 90)),
    ]

    w, h = 48 * SCALE, 48 * SCALE
    cx, cy = w // 2, h // 2
    r_outer = 48 * SCALE // 2 - 6

    for item_type, filename, theme in items:
        img = create_canvas(48, 48)
        d = ImageDraw.Draw(img)
        cr, cg, cb = theme

        # Subtle translucent crystal bubble interior (soft element tint at 40 alpha)
        d.ellipse([cx - r_outer, cy - r_outer, cx + r_outer, cy + r_outer],
                  fill=(cr, cg, cb, 35),
                  outline=(cr, cg, cb, 255), width=6)
        
        # Inner neon glow ring
        r_glow = r_outer - 4
        d.ellipse([cx - r_glow, cy - r_glow, cx + r_glow, cy + r_glow],
                  outline=(min(255, cr + 60), min(255, cg + 60), min(255, cb + 60), 180), width=3)

        # Specular crystal lens crescent glint on upper-left
        d.arc([cx - r_outer + 8, cy - r_outer + 6, cx + r_outer - 8, cy + 12],
              200, 320, fill=(255, 255, 255, 220), width=5)

        # Scaled inner artifact floating in the glass sphere
        draw_inner_item(d, cx, cy, item_type, theme, scale=0.95)

        finalize(img, os.path.join(ORB_DIR, filename), (48, 48))

# ==============================================================================
# 2. WORKSHOP HERO EMBLEMS (128x128) - MOBILE CARD ARTWORK HERO VISUALS
# Pure vibrant illustration with elemental energy corona & specular sparkles (NO grey pastille)
# ==============================================================================

def generate_workshop_badges():
    items = [
        ("multiball", "badge_multiball.png", (0, 220, 255)),
        ("expand",    "badge_expand.png",    (50, 255, 80)),
        ("shrink",    "badge_shrink.png",    (255, 60, 60)),
        ("storm",     "badge_storm.png",     (255, 235, 40)),
        ("tempo",     "badge_tempo.png",     (180, 70, 255)),
        ("extralife", "badge_extralife.png", (255, 100, 180)),
        ("bigball",   "badge_bigball.png",   (255, 120, 20)),
        ("fireball",  "badge_fireball.png",  (255, 60, 20)),
        ("sticky",    "badge_sticky.png",    (245, 175, 40)),
        ("barrier",   "badge_barrier.png",   (0, 240, 255)),
        ("magnet",    "badge_magnet.png",    (220, 60, 255)),
        ("ghost",     "badge_ghost.png",     (160, 240, 255)),
        ("regen",     "badge_regen.png",     (32, 255, 176)),
    ]

    w, h = 128 * SCALE, 128 * SCALE  # 512 x 512
    cx, cy = w // 2, h // 2

    for item_type, filename, theme in items:
        img = create_canvas(128, 128)
        d = ImageDraw.Draw(img)
        cr, cg, cb = theme

        # 1. Radiant Atmospheric Elemental Energy Corona (soft glowing bloom behind the item)
        for rad, alpha in [(230, 20), (200, 38), (170, 60), (140, 85), (105, 120)]:
            d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad],
                      fill=(cr, cg, cb, alpha))

        # 2. Glowing Cardinal Energy Rays (Pulsing Starburst Accent)
        for ang_deg in [0, 45, 90, 135, 180, 225, 270, 315]:
            rad = math.radians(ang_deg)
            x1 = cx + 80 * math.cos(rad)
            y1 = cy + 80 * math.sin(rad)
            x2 = cx + 220 * math.cos(rad)
            y2 = cy + 220 * math.sin(rad)
            d.line([(x1, y1), (x2, y2)], fill=(cr, cg, cb, 60), width=6)

        # 3. Floating Sparkling Energy Motes & Crystals
        for ang_deg, dist, sz in [(30, 180, 8), (120, 170, 7), (210, 190, 8), (300, 175, 9)]:
            rad = math.radians(ang_deg)
            px = cx + dist * math.cos(rad)
            py = cy + dist * math.sin(rad)
            d.ellipse([px - sz, py - sz, px + sz, py + sz], fill=(255, 255, 255, 210))
            d.line([(px - sz * 2, py), (px + sz * 2, py)], fill=(255, 255, 255, 230), width=2)
            d.line([(px, py - sz * 2), (px, py + sz * 2)], fill=(255, 255, 255, 230), width=2)

        # 4. Hero Artifact Scaled to Grand Full Prominence (scale = 2.85)
        draw_inner_item(d, cx, cy, item_type, theme, scale=2.85)

        finalize(img, os.path.join(WORKSHOP_DIR, filename), (128, 128))

# ==============================================================================
# 3. STAGE HAZARDS
# ==============================================================================

def generate_hazards():
    w, h = 64 * SCALE, 64 * SCALE
    img = create_canvas(64, 64)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, h], fill=(20, 22, 28, 220))
    for offset in range(-w, w * 2, 32):
        d.line([offset, 0, offset + h, h], fill=(255, 60, 80, 180), width=10)
    d.rectangle([2, 2, w - 3, h - 3], outline=(255, 50, 100, 240), width=4)
    finalize(img, os.path.join(HAZARD_DIR, "hazard_zone_pattern.png"), (64, 64))

    w, h = 128 * SCALE, 64 * SCALE
    img = create_canvas(128, 64)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, h], fill=(15, 20, 45, 190), outline=(80, 160, 255, 240), width=4)
    cx, cy = w // 2, h // 2
    for y_offset in [-16, 0, 16]:
        arc = [
            (8, cy + y_offset),
            (w // 4, cy + y_offset - 24),
            (w // 2 - 20, cy + y_offset + 20),
            (w // 2 + 20, cy + y_offset - 16),
            (3 * w // 4, cy + y_offset + 18),
            (w - 9, cy + y_offset)
        ]
        d.line(arc, fill=(160, 220, 255, 240), width=5)
        d.line(arc, fill=(255, 255, 255, 255), width=2)
    finalize(img, os.path.join(HAZARD_DIR, "hazard_electric.png"), (128, 64))

    img = create_canvas(128, 64)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, h], fill=(30, 15, 45, 190), outline=(180, 80, 255, 240), width=4)
    for gx in range(w // 6, w, w // 4):
        d.line([gx, 16, gx, h - 28], fill=(210, 120, 255, 220), width=6)
        d.polygon([(gx, h - 14), (gx - 18, h - 36), (gx + 18, h - 36)], fill=(230, 160, 255, 255))
    finalize(img, os.path.join(HAZARD_DIR, "hazard_gravity.png"), (128, 64))

if __name__ == "__main__":
    print("Generating Glass Spheres for falling items...")
    generate_spheres()
    print("Generating Workshop Mobile Badges...")
    generate_workshop_badges()
    print("Generating Stage Hazards...")
    generate_hazards()
    print("All sprites generated successfully!")
