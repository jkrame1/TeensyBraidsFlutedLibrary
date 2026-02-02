# Technical Documentation: Fluted Algorithm

## Overview

The fluted algorithm is a physical modeling synthesis technique that simulates the sound of a flute-like instrument. It's based on the Karplus-Strong algorithm and waveguide synthesis principles.

## Algorithm Details

### Core Components

1. **Delay Line (Waveguide)**
   - Represents the resonant air column in a flute
   - Length determines the fundamental frequency
   - Size: 2048 samples (supports frequencies down to ~21 Hz at 44.1kHz)
   
2. **Noise Excitation**
   - Simulates air turbulence at the embouchure
   - Implemented with a Linear Congruential Generator (LCG)
   - Higher during attack, reduced during sustain
   
3. **Feedback Loop**
   - Controlled by the `timbre` parameter
   - Range: 0.9 to 0.995 (higher = more resonance/sustain)
   - Creates the sustained oscillation characteristic of wind instruments
   
4. **Dampening Filter**
   - Simple first-order lowpass filter
   - Controlled by the `color` parameter
   - Simulates energy loss in the resonator
   - Higher color = brighter sound (less dampening)

### Signal Flow

```
Noise Generator → Waveguide Delay → Lowpass Filter → Feedback
                      ↑                                  |
                      └──────────────────────────────────┘
```

### Parameter Mappings

#### Timbre (0.0 to 1.0)
- Controls feedback amount and resonance
- 0.0: Minimal feedback (short, percussive sound)
- 0.5: Moderate feedback (balanced tone)
- 1.0: Maximum feedback (long sustain, pure tone)

#### Color (0.0 to 1.0)
- Controls brightness and dampening
- 0.0: Maximum dampening (dark, muted tone)
- 0.5: Moderate dampening (natural flute sound)
- 1.0: Minimum dampening (bright, clear tone)

### Frequency Calculation

The delay line length is calculated as:
```
delay_samples = sample_rate / frequency
```

For precise tuning, linear interpolation is used when reading from the delay line to achieve fractional delay lengths.

### Envelope

A simple envelope generator shapes the amplitude:
- **Attack**: Fast rise when note is triggered
- **Sustain**: Holds at maximum while note is on
- **Release**: Slow decay when note is released

The noise excitation also follows an envelope to simulate breath control:
- Maximum noise during attack (simulating initial breath turbulence)
- Reduced noise during sustain (steady tone production)

## Comparison to Original Braids

This implementation stays true to the core algorithm while adapting it for the Teensy Audio Library:

1. **Sample Rate**: Fixed at 44.1 kHz (Teensy Audio Library standard)
2. **Block Processing**: Processes 128 samples per update (vs. 24 in Braids)
3. **Precision**: Uses float for parameters, int16_t for audio
4. **Memory**: Static allocation (no dynamic memory)

## Performance Considerations

- CPU Usage: Moderate (primarily delay line operations and interpolation)
- Memory Usage: ~4 KB for delay line plus code/stack
- Suitable for real-time performance on Teensy 3.x and 4.x

## Tuning and Calibration

The algorithm is self-calibrating based on the requested frequency. However, you may want to adjust:

1. **Feedback Range**: Modify the 0.9-0.995 range in `processWaveguide()` if you need different resonance characteristics
2. **Dampening Range**: Adjust the 0.5-0.99 range for different timbral ranges
3. **Noise Amount**: Scale the noise generation if the attack is too harsh or too soft

## Sound Design Tips

### Classic Flute
- Timbre: 0.6-0.8
- Color: 0.5-0.7
- Play legato melodies

### Breathy Flute
- Timbre: 0.4-0.6
- Color: 0.3-0.5
- Shorter note durations

### Clarinet-like
- Timbre: 0.8-0.95
- Color: 0.6-0.8
- Use lower frequencies

### Percussive/Plucked
- Timbre: 0.3-0.5
- Color: 0.7-0.9
- Very short note durations

### Ethereal/Whistle
- Timbre: 0.85-0.98
- Color: 0.75-0.95
- High frequencies with vibrato

## Further Enhancements

Possible additions for future versions:

1. **Vibrato**: Add LFO modulation to frequency
2. **Breath Pressure**: Map velocity or CC to noise amount
3. **Overblowing**: Implement harmonic mode switching
4. **Polyphony**: Multiple voice allocation
5. **Reverb**: Built-in room simulation
6. **Key Noise**: Add mechanical key click sounds

## References

- Karplus, K., & Strong, A. (1983). "Digital Synthesis of Plucked-String and Drum Timbres"
- Smith, J. O. (2010). "Physical Audio Signal Processing"
- Mutable Instruments Braids source code (MIT License)
- Teensy Audio Library documentation

## License

Based on Mutable Instruments Braids (MIT License)
Adapted for Teensy Audio Library, 2026
