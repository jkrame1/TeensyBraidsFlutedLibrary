/*
 * AudioSynthBraidsFluted.h
 *
 * Fluted physical modeling oscillator for Teensy Audio Library.
 * Faithful port of the two-waveguide flute model from Mutable Instruments
 * Braids (RenderFluted), with added ASR envelope system.
 *
 * Original Braids code: Copyright 2012 Émilie Gillet (MIT License)
 * Teensy Audio adaptation: 2026
 *
 * Algorithm overview
 * ------------------
 * A flute is modelled as two coupled delay lines:
 *   - bore:  the resonant air column (long delay line)
 *   - jet:   the air stream from the embouchure (short delay line)
 *
 * The jet receives breath pressure (envelope + scaled noise).
 * Its output is passed through a nonlinear table (lut_blowing_jet) that
 * models turbulent jet instability — this is what makes it sound like
 * a flute rather than a plucked string.
 * The bore output is low-pass filtered (pitch-dependent damping), then
 * DC-blocked before being fed back into the jet as a reflection.
 *
 * Parameters
 * ----------
 *   timbre  (COLOR in Braids)  — controls jet delay length relative to bore,
 *                                 which changes the tonal character.
 *   color   (TIMBRE in Braids) — controls breath intensity (noise amount).
 *
 * Envelope System
 * ---------------
 * An ASR (Attack-Sustain-Release) envelope can modulate timbre, color, and/or
 * amplitude. Enable/disable routing for each parameter independently.
 * The envelope multiplies the base parameter value (0.0 = silent, 1.0 = full).
 */

#ifndef AudioSynthBraidsFluted_h
#define AudioSynthBraidsFluted_h

#include "Arduino.h"
#include "AudioStream.h"

// Delay line sizes — must match the LUT generation assumptions.
// bore = 4096, jet = 1024 (same as Braids kWGFBoreLength / kWGJetLength).
static const uint16_t kFluteBoreLength = 4096;
static const uint16_t kFluteJetLength  = 1024;

// LUT sizes
static const uint16_t LUT_BLOWING_ENVELOPE_SIZE = 256;
static const uint16_t LUT_BLOWING_JET_SIZE      = 256;
static const uint16_t LUT_FLUTE_BODY_FILTER_SIZE = 512;

// DC-blocking pole (fixed-point Q12, same value Braids uses: 0.995 * 4096)
static const int32_t kDCBlockingPole = 4076;

// Extern declarations for the LUT arrays (defined in the .cpp)
extern const uint16_t lut_blowing_envelope[];
extern const int16_t  lut_blowing_jet[];
extern const uint16_t lut_flute_body_filter[];

class AudioSynthBraidsFluted : public AudioStream
{
public:
    AudioSynthBraidsFluted() : AudioStream(0, NULL) {}

    void begin(void);

    // ---- Note control ----
    void noteOn(float frequencyHz);
    void noteOff(void);

    // ---- Base parameter setters (all 0.0 – 1.0) ----
    // These are the "base" values. The envelope modulates around these.
    void setTimbre(float t);   // jet/bore delay ratio  (COLOR on Braids)
    void setColor(float c);    // breath intensity      (TIMBRE on Braids)
    void setAmplitude(float a);

    // Pitch change while a note is held (re-tunes without re-striking)
    void setFrequency(float frequencyHz);

    // ---- Envelope control ----
    // Attack/Release times in milliseconds
    void setAttack(float milliseconds);   // default: 10ms
    void setRelease(float milliseconds);  // default: 100ms
    
    // Envelope routing — enable/disable envelope modulation per parameter
    void setEnvelopeRouting(bool routeTimbre, bool routeColor, bool routeAmplitude);
    
    // Envelope depth — how much the envelope affects each parameter (0.0 – 1.0)
    // depth = 0.0: no modulation (parameter stays at base value)
    // depth = 1.0: full modulation (parameter goes from 0 to base value)
    void setTimbreEnvDepth(float depth);
    void setColorEnvDepth(float depth);
    void setAmplitudeEnvDepth(float depth);

    virtual void update(void);

private:
    // ----- delay lines (int8_t, matching Braids) -----
    int8_t  bore_[kFluteBoreLength];
    int8_t  jet_ [kFluteJetLength];

    // ----- waveguide state -----
    uint16_t delay_ptr_;          // shared write pointer (wraps per line)
    uint16_t excitation_ptr_;     // index into blowing envelope

    // ----- filter state -----
    int32_t lp_state_;            // bore body lowpass (1-pole IIR)
    int32_t dc_blocking_x0_;      // DC block: previous input
    int32_t dc_blocking_y0_;      // DC block: previous output

    // ----- base parameters (set by user, before envelope modulation) -----
    float   frequency_;           // Hz
    float   timbre_base_;         // 0..1  → jet delay ratio
    float   color_base_;          // 0..1  → breath noise intensity
    float   amplitude_base_;      // 0..1  output gain

    bool    strike_;              // flag: clear delay lines on next update

    // ----- envelope state -----
    enum EnvStage { ENV_IDLE, ENV_ATTACK, ENV_SUSTAIN, ENV_RELEASE };
    EnvStage env_stage_;
    float    env_level_;          // current envelope level (0.0 – 1.0)
    float    attack_increment_;   // per-sample increment during attack
    float    release_increment_;  // per-sample increment during release (negative)
    
    // Envelope routing
    bool     env_route_timbre_;
    bool     env_route_color_;
    bool     env_route_amplitude_;
    
    // Envelope depth (how much the envelope affects each param)
    float    env_depth_timbre_;
    float    env_depth_color_;
    float    env_depth_amplitude_;

    // ----- effective parameters (after envelope modulation) -----
    float    timbre_effective_;
    float    color_effective_;
    float    amplitude_effective_;

    // ----- derived values (recomputed when params change) -----
    uint32_t bore_delay_;         // Q16 bore delay in samples
    uint32_t jet_delay_;          // Q16 jet delay in samples
    uint16_t breath_intensity_;   // scaled noise amount (matches Braids range)
    uint16_t filter_coefficient_; // pitch-dependent LP coeff (Q12, 0..4096)

    // Internal methods
    void recomputeDelays(void);
    void updateEnvelope(void);           // advance envelope by one sample
    void applyEnvelopeModulation(void);  // compute effective params from base + envelope
    
    // Simple RNG
    uint32_t rng_state_;
    inline int16_t randomSample(void);
};

#endif // AudioSynthBraidsFluted_h
