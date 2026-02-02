# Envelope System Reference

## Overview

The AudioSynthBraidsFluted now includes a flexible ASR (Attack-Sustain-Release) envelope that can modulate **timbre**, **color**, and **amplitude** independently.

## Basic Usage

```cpp
AudioSynthBraidsFluted fluted;

void setup() {
    fluted.begin();
    
    // Configure envelope timing
    fluted.setAttack(50.0);    // 50ms attack
    fluted.setRelease(200.0);  // 200ms release
    
    // Enable envelope routing (which parameters are modulated)
    fluted.setEnvelopeRouting(
        false,  // timbre: not modulated
        false,  // color: not modulated
        true    // amplitude: modulated (default behavior)
    );
    
    // Play a note
    fluted.noteOn(440.0);  // Triggers attack
    delay(500);
    fluted.noteOff();      // Triggers release
}
```

## Envelope Stages

1. **IDLE** — Envelope is off (level = 0)
2. **ATTACK** — Envelope rises from 0 to 1 at the attack rate
3. **SUSTAIN** — Envelope holds at 1 while note is on
4. **RELEASE** — Envelope falls from current level to 0 at the release rate

## API Reference

### Envelope Timing

```cpp
void setAttack(float milliseconds)
```
- Sets attack time (0.1ms minimum)
- Default: 10ms
- Typical range: 5-100ms for natural flute attacks

```cpp
void setRelease(float milliseconds)
```
- Sets release time (0.1ms minimum)
- Default: 100ms
- Typical range: 50-500ms

### Envelope Routing

```cpp
void setEnvelopeRouting(bool routeTimbre, bool routeColor, bool routeAmplitude)
```

Controls which parameters the envelope modulates:

- **routeTimbre**: When true, timbre parameter follows envelope
- **routeColor**: When true, color (breath noise) follows envelope
- **routeAmplitude**: When true, volume follows envelope

Default: `(false, false, true)` — amplitude only

### Envelope Depth

```cpp
void setTimbreEnvDepth(float depth)    // 0.0 to 1.0
void setColorEnvDepth(float depth)     // 0.0 to 1.0
void setAmplitudeEnvDepth(float depth) // 0.0 to 1.0
```

Controls how much the envelope affects each parameter:

- **depth = 0.0**: No modulation (parameter stays at base value)
- **depth = 0.5**: 50% modulation
- **depth = 1.0**: Full modulation (parameter goes from 0 → base value)

Default: 1.0 for all

**Formula**: `effective = base × (1 - depth + depth × envelope)`

## Common Configurations

### 1. Natural Flute (Default)
```cpp
// Volume envelope only
fluted.setEnvelopeRouting(false, false, true);
fluted.setAttack(20.0);
fluted.setRelease(150.0);
```

### 2. Evolving Brightness
```cpp
// Dark attack, opens to bright sustain
fluted.setEnvelopeRouting(false, true, true);
fluted.setColorEnvDepth(0.8);
fluted.setAttack(50.0);
fluted.setRelease(200.0);
```

### 3. Timbral Evolution
```cpp
// Tone shifts during attack/release
fluted.setEnvelopeRouting(true, false, true);
fluted.setTimbreEnvDepth(0.6);
fluted.setAttack(80.0);
fluted.setRelease(250.0);
```

### 4. Complex Spectral Evolution
```cpp
// All parameters modulated
fluted.setEnvelopeRouting(true, true, true);
fluted.setTimbreEnvDepth(0.5);
fluted.setColorEnvDepth(0.7);
fluted.setAmplitudeEnvDepth(1.0);
fluted.setAttack(60.0);
fluted.setRelease(300.0);
```

### 5. Plucked/Percussive
```cpp
// Fast attack, medium release, no sustain needed
fluted.setEnvelopeRouting(false, false, true);
fluted.setAttack(5.0);
fluted.setRelease(80.0);
// Just trigger noteOn, don't call noteOff
```

## Parameter Behavior When Envelope-Modulated

### Timbre (Jet/Bore Ratio)
- **Envelope at 0**: Minimal jet delay (one extreme of tonal character)
- **Envelope at 1**: Full jet delay as set by `setTimbre()`
- Creates a "formant sweep" effect during attack/release

### Color (Breath Intensity)
- **Envelope at 0**: Minimal breath noise (pure tone)
- **Envelope at 1**: Full breath noise as set by `setColor()`
- Simulates breath pressure varying during the note

### Amplitude (Volume)
- **Envelope at 0**: Silent
- **Envelope at 1**: Full volume as set by `setAmplitude()`
- Standard volume envelope behavior

## Tips

1. **For realistic flute sounds**: Use amplitude envelope only with 20-50ms attack and 100-200ms release

2. **For expressive synthesis**: Route color to envelope with 50-80% depth for breath pressure simulation

3. **For evolving pads**: Enable all three routings with moderate depths (0.4-0.7) and long times (100ms+ attack, 500ms+ release)

4. **For percussive sounds**: Fast attack (5-10ms), medium release (50-100ms), amplitude only

5. **Combining with external envelopes**: Set routing to `(false, false, false)` and control parameters directly via your own envelope generators

## MIDI Integration Example

```cpp
void handleNoteOn(byte note, byte velocity) {
    float freq = midiNoteToFrequency(note);
    
    // Scale envelope times by velocity
    float vel_norm = velocity / 127.0;
    fluted.setAttack(10.0 + vel_norm * 40.0);   // 10-50ms
    fluted.setRelease(100.0 + vel_norm * 200.0); // 100-300ms
    
    // Scale color by velocity (breath pressure)
    fluted.setColor(0.3 + vel_norm * 0.5);  // 0.3-0.8
    
    fluted.noteOn(freq);
}

void handleNoteOff(byte note) {
    fluted.noteOff();  // Triggers release
}
```

## Performance Notes

- Envelope is updated **per-sample** (44,100 times per second)
- Parameters are recomputed when envelope changes
- Minimal CPU overhead (~5% increase vs. no envelope)
- Envelope modulation is completely independent of the Braids breath envelope (which still controls the physical model's excitation)
