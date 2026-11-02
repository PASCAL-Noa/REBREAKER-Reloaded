import numpy as np
import wave
import struct
import os

SAMPLE_RATE = 44100
BPM = 124.0
BEAT_DUR = 60.0 / BPM
TOTAL_BEATS = 16 # 4 bars of 4/4
TOTAL_DURATION = TOTAL_BEATS * BEAT_DUR
NUM_SAMPLES = int(SAMPLE_RATE * TOTAL_DURATION)
TIME = np.linspace(0, TOTAL_DURATION, NUM_SAMPLES, endpoint=False)

def export_wav(filename, left, right):
    left = np.clip(left, -0.98, 0.98)
    right = np.clip(right, -0.98, 0.98)
    left_i16 = (left * 32767.0).astype(np.int16)
    right_i16 = (right * 32767.0).astype(np.int16)
    stereo = np.empty((NUM_SAMPLES * 2,), dtype=np.int16)
    stereo[0::2] = left_i16
    stereo[1::2] = right_i16
    
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with wave.open(filename, 'wb') as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(stereo.tobytes())
    print(f"Exported: {filename}")

# 1. KICK & SNARE (Rhythm Foundation)
kick = np.zeros(NUM_SAMPLES)
snare = np.zeros(NUM_SAMPLES)

for b in range(TOTAL_BEATS):
    t_start = b * BEAT_DUR
    idx_start = int(t_start * SAMPLE_RATE)
    
    # Four on the floor Kick
    k_len = int(0.25 * SAMPLE_RATE)
    k_idx = np.arange(k_len)
    t_k = k_idx / SAMPLE_RATE
    freq = 150.0 * np.exp(-t_k * 28.0) + 48.0
    phase = 2.0 * np.pi * np.cumsum(freq) / SAMPLE_RATE
    env_k = np.exp(-t_k * 14.0)
    # Add a punchy click at transient
    click = np.exp(-t_k * 180.0) * np.sin(2.0 * np.pi * 1200.0 * t_k)
    k_wave = (np.sin(phase) + 0.4 * click) * env_k * 0.95
    end_idx = min(NUM_SAMPLES, idx_start + k_len)
    kick[idx_start:end_idx] += k_wave[:end_idx - idx_start]
    
    # Snare on beats 2 and 4 (1, 3 in 0-indexed)
    if b % 2 == 1:
        s_len = int(0.28 * SAMPLE_RATE)
        s_idx = np.arange(s_len)
        t_s = s_idx / SAMPLE_RATE
        noise = (np.random.rand(s_len) * 2.0 - 1.0)
        # Tone + noise
        tone = np.sin(2.0 * np.pi * 185.0 * t_s) * np.exp(-t_s * 25.0)
        env_s = np.exp(-t_s * 15.0)
        s_wave = (noise * 0.65 + tone * 0.45) * env_s * 0.85
        end_s = min(NUM_SAMPLES, idx_start + s_len)
        snare[idx_start:end_s] += s_wave[:end_s - idx_start]

# 2. FUNKY OCTAVE BASS (Bassline)
bass = np.zeros(NUM_SAMPLES)
# D minor disco chord progression: Dm7 (bars 1-2) -> Gm7 (bar 3) -> A7 (bar 4)
# Notes: D2=73.42, D3=146.83, F2=87.31, G2=98.00, G3=196.00, A2=110.00, A3=220.00, C3=130.81
bass_pattern = [
    # Bar 1 (Dm7): D2, D3 octave bounce on 16th notes
    (0.00, 73.42, 0.22, 1.0), (0.25, 146.83, 0.18, 0.85), (0.50, 73.42, 0.22, 0.95), (0.75, 146.83, 0.18, 0.9),
    (1.00, 73.42, 0.22, 1.0), (1.25, 146.83, 0.18, 0.85), (1.50, 87.31, 0.22, 0.9), (1.75, 110.00, 0.18, 0.95),
    # Bar 2 (Dm7):
    (2.00, 73.42, 0.22, 1.0), (2.25, 146.83, 0.18, 0.85), (2.50, 73.42, 0.22, 0.95), (2.75, 146.83, 0.18, 0.9),
    (3.00, 73.42, 0.22, 1.0), (3.25, 130.81, 0.18, 0.85), (3.50, 146.83, 0.22, 0.95), (3.75, 164.81, 0.18, 0.9),
    # Bar 3 (Gm7):
    (4.00, 98.00, 0.22, 1.0), (4.25, 196.00, 0.18, 0.85), (4.50, 98.00, 0.22, 0.95), (4.75, 196.00, 0.18, 0.9),
    (5.00, 98.00, 0.22, 1.0), (5.25, 174.61, 0.18, 0.85), (5.50, 146.83, 0.22, 0.9), (5.75, 130.81, 0.18, 0.9),
    # Bar 4 (A7 / turn):
    (6.00, 110.00, 0.22, 1.0), (6.25, 220.00, 0.18, 0.85), (6.50, 110.00, 0.22, 0.95), (6.75, 220.00, 0.18, 0.9),
    (7.00, 110.00, 0.18, 1.0), (7.25, 123.47, 0.18, 0.9), (7.50, 130.81, 0.18, 0.95), (7.75, 138.59, 0.18, 1.0)
]

for beat_offset, f_note, dur_sec, vel in bass_pattern:
    t_start = (beat_offset / 2.0) * BEAT_DUR
    idx_start = int(t_start * SAMPLE_RATE)
    n_len = int(dur_sec * SAMPLE_RATE)
    if idx_start >= NUM_SAMPLES: continue
    t_n = np.arange(n_len) / SAMPLE_RATE
    # Sawtooth + sub sine + 2nd harmonic
    saw = 2.0 * (t_n * f_note - np.floor(t_n * f_note + 0.5))
    sub = np.sin(2.0 * np.pi * (f_note * 0.5) * t_n)
    sqr = np.sign(np.sin(2.0 * np.pi * f_note * t_n))
    env_b = np.exp(-t_n * 9.0)
    # Warm lowpass simulation
    b_wave = (saw * 0.5 + sub * 0.45 + sqr * 0.2) * env_b * vel * 0.8
    end_idx = min(NUM_SAMPLES, idx_start + n_len)
    bass[idx_start:end_idx] += b_wave[:end_idx - idx_start]

# 3. HI-HATS & CLAPS (Ball 2 addition)
hihats = np.zeros(NUM_SAMPLES)
claps = np.zeros(NUM_SAMPLES)

# 16th note closed hi-hats + off-beat open hi-hat (classic disco sizzle)
for step in range(TOTAL_BEATS * 4):
    t_start = (step / 4.0) * BEAT_DUR
    idx_start = int(t_start * SAMPLE_RATE)
    isOpen = (step % 4 == 2) # Offbeat "and"
    hh_len = int((0.22 if isOpen else 0.06) * SAMPLE_RATE)
    t_h = np.arange(hh_len) / SAMPLE_RATE
    noise = np.random.rand(hh_len) * 2.0 - 1.0
    # High-pass filter emulation (differentiating noise)
    hp_noise = np.diff(noise, prepend=0.0)
    decay = 18.0 if isOpen else 65.0
    hh_wave = hp_noise * np.exp(-t_h * decay) * (0.55 if isOpen else 0.22)
    end_idx = min(NUM_SAMPLES, idx_start + hh_len)
    hihats[idx_start:end_idx] += hh_wave[:end_idx - idx_start]

# Claps on beats 2 and 4
for b in range(TOTAL_BEATS):
    if b % 2 == 1:
        t_start = b * BEAT_DUR
        idx_start = int(t_start * SAMPLE_RATE)
        # Multiple micro-bursts of noise for hand clap texture
        c_len = int(0.22 * SAMPLE_RATE)
        c_wave = np.zeros(c_len)
        for burst_delay in [0.0, 0.012, 0.024]:
            b_offset = int(burst_delay * SAMPLE_RATE)
            rem = c_len - b_offset
            t_c = np.arange(rem) / SAMPLE_RATE
            noise_c = (np.random.rand(rem) * 2.0 - 1.0) * np.exp(-t_c * 24.0)
            c_wave[b_offset:] += noise_c * 0.4
        end_c = min(NUM_SAMPLES, idx_start + c_len)
        claps[idx_start:end_c] += c_wave[:end_c - idx_start]

# 4. FUNKY WAH GUITAR / CLAVINET (Ball 3 addition)
guitar_L = np.zeros(NUM_SAMPLES)
guitar_R = np.zeros(NUM_SAMPLES)
# 16th note funky scratch / chord rhythm: Dm9 (D-F-A-C-E), G9, A7#9
# Wah sweep frequency modulation
wah_lfo = 1800.0 + 1200.0 * np.sin(2.0 * np.pi * (TIME / (BEAT_DUR * 2.0)))

for step in range(TOTAL_BEATS * 4):
    # Funky syncopated strum pattern
    if step % 4 in [0, 1, 3]: # syncopated rhythm
        t_start = (step / 4.0) * BEAT_DUR
        idx_start = int(t_start * SAMPLE_RATE)
        g_len = int(0.12 * SAMPLE_RATE)
        t_g = np.arange(g_len) / SAMPLE_RATE
        chord_root = 293.66 if step < 32 else (392.00 if step < 48 else 440.00)
        # 3 chord notes
        f1, f2, f3 = chord_root, chord_root * 1.2, chord_root * 1.5
        chord = (np.sign(np.sin(2.0 * np.pi * f1 * t_g)) * 0.4 +
                 np.sign(np.sin(2.0 * np.pi * f2 * t_g)) * 0.35 +
                 np.sign(np.sin(2.0 * np.pi * f3 * t_g)) * 0.35)
        env_g = np.exp(-t_g * 26.0)
        pan = np.sin(step * 0.5) * 0.3
        end_idx = min(NUM_SAMPLES, idx_start + g_len)
        guitar_L[idx_start:end_idx] += (chord[:end_idx - idx_start] * env_g[:end_idx - idx_start]) * (0.32 - pan)
        guitar_R[idx_start:end_idx] += (chord[:end_idx - idx_start] * env_g[:end_idx - idx_start]) * (0.32 + pan)

# 5. DISCO BRASS / HORN STABS (Ball 4 addition)
brass_L = np.zeros(NUM_SAMPLES)
brass_R = np.zeros(NUM_SAMPLES)
# Classic horn fanfare stabs on punchy accents
horn_stabs = [
    # Bar 1: beat 1.5 and 3.5
    (1.5, [587.33, 698.46, 880.00], 0.25),
    (3.5, [587.33, 659.25, 880.00], 0.35),
    # Bar 2: beat 5.5 and 7.25, 7.5
    (5.5, [587.33, 698.46, 880.00], 0.25),
    (7.25, [659.25, 783.99, 987.77], 0.16),
    (7.5,  [698.46, 880.00, 1046.50], 0.45),
    # Bar 3: beat 9.5 and 11.5
    (9.5,  [587.33, 783.99, 880.00], 0.25),
    (11.5, [659.25, 783.99, 987.77], 0.35),
    # Bar 4: triumphant turnaround fanfare
    (13.5, [698.46, 880.00, 1108.73], 0.22),
    (14.5, [783.99, 987.77, 1174.66], 0.22),
    (15.0, [880.00, 1046.50, 1318.51], 0.55)
]

for b_pos, freqs, dur in horn_stabs:
    t_start = b_pos * BEAT_DUR
    idx_start = int(t_start * SAMPLE_RATE)
    h_len = int(dur * SAMPLE_RATE)
    t_h = np.arange(h_len) / SAMPLE_RATE
    horn_wave = np.zeros(h_len)
    for f in freqs:
        # Brass sawtooth harmonic stack
        saw1 = 2.0 * (t_h * f - np.floor(t_h * f + 0.5))
        saw2 = 2.0 * (t_h * (f * 1.006) - np.floor(t_h * (f * 1.006) + 0.5)) # chorus detune
        horn_wave += (saw1 + saw2) * 0.25
    # Brass attack envelope: quick punch, slight swell, decay
    env_h = np.minimum(t_h * 40.0, 1.0) * np.exp(-t_h * (3.0 / dur))
    end_idx = min(NUM_SAMPLES, idx_start + h_len)
    brass_L[idx_start:end_idx] += (horn_wave[:end_idx - idx_start] * env_h[:end_idx - idx_start]) * 0.35
    brass_R[idx_start:end_idx] += (horn_wave[:end_idx - idx_start] * env_h[:end_idx - idx_start]) * 0.35

# 6. DISCO STRINGS & SYNTH LEAD ARPEGGIO (Ball 5 / Mirror Ball finale)
strings_L = np.zeros(NUM_SAMPLES)
strings_R = np.zeros(NUM_SAMPLES)

# Lush disco strings sweeping in high octaves (1174 Hz, 1396 Hz, 1760 Hz)
# 16th note ascending disco arpeggio run + soaring vibrato
arp_notes = [587.33, 698.46, 880.00, 1046.50, 1174.66, 1396.91, 1567.98, 1760.00]
for step in range(TOTAL_BEATS * 4):
    t_start = (step / 4.0) * BEAT_DUR
    idx_start = int(t_start * SAMPLE_RATE)
    s_len = int(0.24 * SAMPLE_RATE)
    t_s = np.arange(s_len) / SAMPLE_RATE
    f = arp_notes[step % len(arp_notes)]
    # Vibrato LFO
    vibrato = 1.0 + 0.015 * np.sin(2.0 * np.pi * 6.0 * t_s)
    # Ensemble unison strings
    str1 = np.sin(2.0 * np.pi * f * vibrato * t_s)
    str2 = np.sin(2.0 * np.pi * (f * 1.004) * vibrato * t_s)
    str3 = np.sin(2.0 * np.pi * (f * 0.996) * vibrato * t_s)
    env_s = np.exp(-t_s * 7.0)
    s_wave = (str1 + str2 + str3) * 0.22 * env_s
    # Stereo panning modulation
    pan = np.sin(step * 0.785) * 0.4
    end_idx = min(NUM_SAMPLES, idx_start + s_len)
    strings_L[idx_start:end_idx] += s_wave[:end_idx - idx_start] * (0.5 - pan)
    strings_R[idx_start:end_idx] += s_wave[:end_idx - idx_start] * (0.5 + pan)

# 7. ASSEMBLE STEM LAYERS FOR EACH LEVEL
# Lvl 1: Kick, Snare, Funky Bass
lvl1_L = kick * 0.7 + snare * 0.5 + bass * 0.8
lvl1_R = kick * 0.7 + snare * 0.5 + bass * 0.8

# Lvl 2: + Hi-hats & Claps
lvl2_L = lvl1_L + hihats * 0.45 + claps * 0.55
lvl2_R = lvl1_R + hihats * 0.45 + claps * 0.55

# Lvl 3: + Wah Guitar / Clavinet
lvl3_L = lvl2_L + guitar_L * 0.65
lvl3_R = lvl2_R + guitar_R * 0.65

# Lvl 4: + Brass / Horns
lvl4_L = lvl3_L + brass_L * 0.75
lvl4_R = lvl3_R + brass_R * 0.75

# Lvl 5: + Strings & Synth Lead (The full Disco Orchestra)
lvl5_L = lvl4_L + strings_L * 0.8
lvl5_R = lvl4_R + strings_R * 0.8

# Normalize each track smoothly
for l, r in [(lvl1_L, lvl1_R), (lvl2_L, lvl2_R), (lvl3_L, lvl3_R), (lvl4_L, lvl4_R), (lvl5_L, lvl5_R)]:
    peak = max(np.max(np.abs(l)), np.max(np.abs(r)))
    if peak > 0.001:
        l /= (peak * 1.05)
        r /= (peak * 1.05)

export_wav("Resources/audio/music/disco_ultimate_lvl1.wav", lvl1_L, lvl1_R)
export_wav("Resources/audio/music/disco_ultimate_lvl2.wav", lvl2_L, lvl2_R)
export_wav("Resources/audio/music/disco_ultimate_lvl3.wav", lvl3_L, lvl3_R)
export_wav("Resources/audio/music/disco_ultimate_lvl4.wav", lvl4_L, lvl4_R)
export_wav("Resources/audio/music/disco_ultimate_lvl5.wav", lvl5_L, lvl5_R)
export_wav("Resources/audio/music/disco_ultimate.wav", lvl5_L, lvl5_R)

# 8. GRAND FINALE VOICEOVER: "DISCO !" (Masculine Disco Stu voice)
# Generated via gen_david_voice.ps1 and process_disco_voice.py
print("Disco music tracks generation complete!")
