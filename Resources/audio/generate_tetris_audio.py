import numpy as np
import wave
import struct
import os

SAMPLE_RATE = 44100

def write_wav(filename, samples, sample_rate=SAMPLE_RATE):
    samples = np.clip(samples, -0.95, 0.95)
    i16 = (samples * 32767.0).astype(np.int16)
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with wave.open(filename, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(i16.tobytes())
    print(f"Generated: {filename} ({len(samples)/sample_rate:.2f}s)")

def pulse_wave(freq, duration, duty=0.5, sample_rate=SAMPLE_RATE):
    num_samples = int(duration * sample_rate)
    t = np.linspace(0, duration, num_samples, endpoint=False)
    phase = (freq * t) % 1.0
    wave = np.where(phase < duty, 1.0, -1.0)
    # Quick anti-click envelope
    env = np.ones(num_samples)
    attack = min(int(0.005 * sample_rate), num_samples // 4)
    decay = min(int(0.02 * sample_rate), num_samples // 4)
    if attack > 0:
        env[:attack] = np.linspace(0, 1, attack)
    if decay > 0:
        env[-decay:] = np.linspace(1, 0, decay)
    return wave * env

def triangle_wave(freq, duration, sample_rate=SAMPLE_RATE):
    num_samples = int(duration * sample_rate)
    t = np.linspace(0, duration, num_samples, endpoint=False)
    phase = (freq * t) % 1.0
    wave = 2.0 * np.abs(2.0 * (phase - np.floor(phase + 0.5))) - 1.0
    return wave

# 1. Korobeiniki Theme (Tetris 8-bit Music Loop)
bpm = 140.0
beat_dur = 60.0 / bpm
sixteenth_dur = beat_dur / 4.0

# Note frequencies (Hz)
notes = {
    'E4': 329.63, 'B3': 246.94, 'C4': 261.63, 'D4': 293.66, 'A3': 220.00,
    'E5': 659.25, 'B4': 493.88, 'C5': 523.25, 'D5': 587.33, 'A4': 440.00,
    'G#4': 415.30, 'F5': 698.46, 'A5': 880.00, 'G#5': 830.61, 'F4': 349.23,
    'G4': 392.00, 'REST': 0.0
}

# Melody: (Note, duration in 16th notes)
melody_def = [
    # Bar 1
    ('E5', 4), ('B4', 2), ('C5', 2), ('D5', 4), ('C5', 2), ('B4', 2),
    # Bar 2
    ('A4', 4), ('A4', 2), ('C5', 2), ('E5', 4), ('D5', 2), ('C5', 2),
    # Bar 3
    ('B4', 6), ('C5', 2), ('D5', 4), ('E5', 4),
    # Bar 4
    ('C5', 4), ('A4', 4), ('A4', 6), ('REST', 2),
    # Bar 5
    ('D5', 4), ('F5', 2), ('A5', 2), ('A5', 4), ('G#5', 2), ('F5', 2),
    # Bar 6
    ('E5', 6), ('C5', 2), ('E5', 4), ('D5', 2), ('C5', 2),
    # Bar 7
    ('B4', 4), ('B4', 2), ('C5', 2), ('D5', 4), ('E5', 4),
    # Bar 8
    ('C5', 4), ('A4', 4), ('A4', 6), ('REST', 2)
]

total_16ths = sum(dur for _, dur in melody_def)
total_music_duration = total_16ths * sixteenth_dur
music_num_samples = int(total_music_duration * SAMPLE_RATE)
lead_track = np.zeros(music_num_samples)
bass_track = np.zeros(music_num_samples)
noise_track = np.zeros(music_num_samples)

cursor = 0
for note_name, dur_16th in melody_def:
    dur_sec = dur_16th * sixteenth_dur
    num_s = int(dur_sec * SAMPLE_RATE)
    if note_name != 'REST' and notes[note_name] > 0:
        freq = notes[note_name]
        tone = pulse_wave(freq, dur_sec, duty=0.25)
        lead_track[cursor:cursor + len(tone)] += tone * 0.45
    cursor += num_s

# Bassline (Triangle wave)
bass_progression = [
    ('E4', 4), ('B3', 4), ('E4', 4), ('G#4', 4),
    ('A3', 4), ('E4', 4), ('A3', 4), ('C4', 4),
    ('E4', 4), ('B3', 4), ('E4', 4), ('G#4', 4),
    ('A3', 4), ('E4', 4), ('A3', 4), ('REST', 4),
    ('D4', 4), ('A3', 4), ('D4', 4), ('F4', 4),
    ('C4', 4), ('G4', 4), ('C4', 4), ('E4', 4),
    ('E4', 4), ('B3', 4), ('E4', 4), ('G#4', 4),
    ('A3', 4), ('E4', 4), ('A3', 4), ('REST', 4)
]

b_cursor = 0
for note_name, dur_16th in bass_progression:
    dur_sec = dur_16th * sixteenth_dur
    num_s = int(dur_sec * SAMPLE_RATE)
    if note_name != 'REST' and notes[note_name] > 0:
        freq = notes[note_name] * 0.5 # One octave down
        tone = triangle_wave(freq, dur_sec)
        bass_track[b_cursor:b_cursor + len(tone)] += tone * 0.40
    b_cursor += num_s

# Chiptune percussion (Hi-hat noise ticks on 16ths, snare on 2 and 4)
for i in range(total_16ths):
    t_start = i * sixteenth_dur
    idx = int(t_start * SAMPLE_RATE)
    if i % 4 == 0: # Downbeat tick
        tick_len = int(0.015 * SAMPLE_RATE)
        noise = (np.random.rand(tick_len) * 2.0 - 1.0) * np.linspace(0.18, 0.0, tick_len)
        noise_track[idx:idx + tick_len] += noise
    elif i % 2 == 0: # Upbeat hat
        tick_len = int(0.008 * SAMPLE_RATE)
        noise = (np.random.rand(tick_len) * 2.0 - 1.0) * np.linspace(0.10, 0.0, tick_len)
        noise_track[idx:idx + tick_len] += noise

full_music = lead_track + bass_track + noise_track
write_wav("Resources/audio/music/tetris_8bit_theme.wav", full_music)

# 2. SFX Generation
# tetris_move.wav: short 0.04s blip
move_dur = 0.045
move_samples = int(move_dur * SAMPLE_RATE)
t_m = np.linspace(0, move_dur, move_samples, endpoint=False)
f_m = np.linspace(350, 480, move_samples)
p_m = 2.0 * np.pi * np.cumsum(f_m) / SAMPLE_RATE
env_m = np.exp(-t_m * 45.0)
sfx_move = np.sign(np.sin(p_m)) * env_m * 0.6
write_wav("Resources/audio/sfx/tetris_move.wav", sfx_move)

# tetris_rotate.wav: short 0.06s upward double chirp
rot_dur = 0.065
rot_samples = int(rot_dur * SAMPLE_RATE)
t_r = np.linspace(0, rot_dur, rot_samples, endpoint=False)
f_r = 520.0 + 380.0 * np.sin(2.0 * np.pi * 18.0 * t_r)
p_r = 2.0 * np.pi * np.cumsum(f_r) / SAMPLE_RATE
env_r = np.exp(-t_r * 30.0)
sfx_rotate = np.sign(np.sin(p_r)) * env_r * 0.65
write_wav("Resources/audio/sfx/tetris_rotate.wav", sfx_rotate)

# tetris_drop.wav: punchy impact thud
drop_dur = 0.12
drop_samples = int(drop_dur * SAMPLE_RATE)
t_d = np.linspace(0, drop_dur, drop_samples, endpoint=False)
f_d = 180.0 * np.exp(-t_d * 30.0) + 45.0
p_d = 2.0 * np.pi * np.cumsum(f_d) / SAMPLE_RATE
noise_click = (np.random.rand(drop_samples) * 2.0 - 1.0) * np.exp(-t_d * 80.0) * 0.35
sfx_drop = (np.sin(p_d) * 0.7 + noise_click) * np.exp(-t_d * 18.0) * 0.8
write_wav("Resources/audio/sfx/tetris_drop.wav", sfx_drop)

# tetris_line_clear.wav: energetic ascending retro chiptune arpeggio
chime_notes = [523.25, 659.25, 783.99, 1046.50] # C5, E5, G5, C6
sfx_clear = np.zeros(int(0.35 * SAMPLE_RATE))
step_dur = 0.075
for i, fn in enumerate(chime_notes):
    idx = int(i * step_dur * SAMPLE_RATE)
    tone = pulse_wave(fn, step_dur * 1.5, duty=0.35)
    sfx_clear[idx:idx + len(tone)] += tone * 0.55
write_wav("Resources/audio/sfx/tetris_line_clear.wav", sfx_clear)

# tetris_tetris.wav: 4-line Tetris epic triumphant fanfare
fanfare_notes = [523.25, 659.25, 783.99, 1046.50, 1318.51, 1567.98] # C5, E5, G5, C6, E6, G6
sfx_tetris = np.zeros(int(0.85 * SAMPLE_RATE))
ff_step = 0.09
for i, fn in enumerate(fanfare_notes):
    idx = int(i * ff_step * SAMPLE_RATE)
    dur = 0.25 if i == len(fanfare_notes) - 1 else ff_step * 1.4
    tone = pulse_wave(fn, dur, duty=0.5)
    end_idx = min(len(sfx_tetris), idx + len(tone))
    sfx_tetris[idx:end_idx] += tone[:end_idx - idx] * 0.6
write_wav("Resources/audio/sfx/tetris_tetris.wav", sfx_tetris)

# tetris_danger.wav: pulsing warning siren
danger_dur = 0.40
danger_samples = int(danger_dur * SAMPLE_RATE)
t_dg = np.linspace(0, danger_dur, danger_samples, endpoint=False)
f_dg = 750.0 + 350.0 * np.sin(2.0 * np.pi * 7.0 * t_dg)
p_dg = 2.0 * np.pi * np.cumsum(f_dg) / SAMPLE_RATE
env_dg = 0.7 + 0.3 * np.sin(2.0 * np.pi * 14.0 * t_dg)
sfx_danger = np.sign(np.sin(p_dg)) * env_dg * 0.55
write_wav("Resources/audio/sfx/tetris_danger.wav", sfx_danger)

print("All Tetris audio assets successfully generated!")
