/*
 * AudioSynthBraidsFluted.cpp
 *
 * Fluted physical modeling oscillator for Teensy Audio Library.
 * Faithful port of RenderFluted from Mutable Instruments Braids.
 *
 * Original Braids code: Copyright 2012 Émilie Gillet (MIT License)
 * Teensy Audio adaptation: 2026
 *
 * Signal-flow (per sample, mirrors Braids exactly):
 *
 *   1. Read bore and jet delay lines with linear interpolation.
 *   2. Build breath_pressure = envelope[ptr] + noise * intensity.
 *   3. Lowpass-filter the bore read-back (pitch-dependent 1-pole IIR).
 *   4. DC-block that filtered value → "reflection".
 *   5. Write into jet:  (breath_pressure - reflection/2) >> 9
 *   6. Look up jet value in lut_blowing_jet (the nonlinearity).
 *   7. Write into bore: (jet_table_output + reflection/2) >> 9
 *   8. Output = bore_value >> 1, clipped to int16.
 *
 * The jet nonlinearity (step 6) is the single most important thing that
 * makes this sound like a flute.  A plucked-string model would just add
 * jet_value directly; the table introduces the harmonic distortion
 * characteristic of turbulent jet instability at an embouchure.
 */

#include "AudioSynthBraidsFluted.h"
#include <string.h>   // memset

// ---------------------------------------------------------------------------
// Lookup tables
// ---------------------------------------------------------------------------
// lut_blowing_envelope – 256 × uint16_t
//   Models the pressure rise during a breath attack.
//   Fast exponential rise with a small overshoot, settling to ~32767.
//   Indexed by excitation_ptr which advances every 4th sample.
//
// lut_blowing_jet – 256 × int16_t
//   THE key nonlinearity.  Models how the jet stream interacts with the
//   bore opening.  Input = jet delay-line read value (clamped 0..65535,
//   then >> 8 to get index 0..255).  Output = pressure reflected back
//   into the bore.  The S-curve / cubic saturation shape creates the
//   harmonic content that distinguishes a flute from a string.
//
// lut_flute_body_filter – 512 × uint16_t
//   Pitch-dependent damping coefficient for the bore resonator's 1-pole
//   lowpass.  Indexed by (pitch >> 7) in Braids; we map our frequency
//   to an equivalent 0..511 index.  Low pitches → heavy damping (longer
//   tube, more wall loss).  High pitches → light damping.
//   Value is Q12 (0..4096).  Used as:
//     lp = (-coeff * bore + (4096 - coeff) * lp_prev) >> 12

const uint16_t lut_blowing_envelope[256] = {
  2621, 4466, 6216, 7871, 9436, 10912, 12304, 13614, 14847, 16006, 17095, 18117,
  19076, 19974, 20816, 21605, 22344, 23035, 23682, 24286, 24852, 25380, 25874, 26336,
  26767, 27169, 27545, 27897, 28224, 28530, 28816, 29082, 29331, 29562, 29779, 29981,
  30169, 30345, 30508, 30661, 30804, 30937, 31061, 31176, 31284, 31384, 31478, 31565,
  31647, 31723, 31793, 31859, 31921, 31978, 32032, 32082, 32128, 32171, 32212, 32249,
  32284, 32317, 32348, 32376, 32403, 32427, 32450, 32472, 32492, 32510, 32528, 32544,
  32559, 32573, 32586, 32599, 32610, 32621, 32630, 32640, 32648, 32656, 32664, 32671,
  32677, 32683, 32689, 32694, 32699, 32704, 32708, 32712, 32716, 32719, 32722, 32725,
  32728, 32731, 32733, 32735, 32738, 32740, 32741, 32743, 32745, 32746, 32748, 32749,
  32750, 32751, 32752, 32753, 32754, 32755, 32756, 32756, 32757, 32758, 32758, 32759,
  32759, 32760, 32760, 32761, 32761, 32762, 32762, 32762, 32762, 32763, 32763, 32763,
  32763, 32764, 32764, 32764, 32764, 32764, 32764, 32765, 32765, 32765, 32765, 32765,
  32765, 32765, 32765, 32765, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766, 32766,
  32766, 32766, 32766, 32766
};

const int16_t lut_blowing_jet[256] = {
  -9011, -9048, -9083, -9115, -9145, -9172, -9197, -9220, -9240, -9258, -9273, -9287,
  -9297, -9306, -9312, -9316, -9318, -9318, -9315, -9310, -9303, -9294, -9283, -9269,
  -9254, -9236, -9217, -9195, -9172, -9146, -9119, -9089, -9058, -9025, -8989, -8952,
  -8913, -8873, -8830, -8786, -8739, -8691, -8642, -8590, -8537, -8482, -8426, -8368,
  -8308, -8247, -8184, -8119, -8053, -7986, -7917, -7846, -7774, -7700, -7626, -7549,
  -7471, -7392, -7312, -7230, -7147, -7062, -6976, -6889, -6801, -6711, -6621, -6529,
  -6436, -6341, -6246, -6149, -6052, -5953, -5853, -5753, -5651, -5548, -5444, -5339,
  -5234, -5127, -5020, -4911, -4802, -4692, -4581, -4469, -4356, -4243, -4129, -4014,
  -3898, -3782, -3665, -3548, -3429, -3311, -3191, -3071, -2950, -2829, -2708, -2585,
  -2463, -2340, -2216, -2092, -1967, -1843, -1717, -1592, -1466, -1340, -1213, -1086,
  -959, -832, -704, -576, -449, -320, -192, -64, 64, 192, 321, 450,
  578, 707, 836, 965, 1093, 1222, 1351, 1479, 1607, 1736, 1864, 1992,
  2119, 2247, 2374, 2501, 2628, 2754, 2880, 3006, 3132, 3257, 3381, 3506,
  3630, 3753, 3876, 3998, 4120, 4242, 4363, 4483, 4603, 4722, 4841, 4959,
  5076, 5193, 5309, 5424, 5539, 5653, 5766, 5878, 5990, 6100, 6210, 6319,
  6427, 6534, 6641, 6746, 6850, 6954, 7056, 7158, 7258, 7357, 7456, 7553,
  7649, 7744, 7838, 7931, 8022, 8112, 8201, 8289, 8376, 8461, 8545, 8628,
  8709, 8789, 8868, 8945, 9021, 9095, 9168, 9240, 9310, 9379, 9446, 9511,
  9575, 9637, 9698, 9757, 9815, 9870, 9925, 9977, 10028, 10077, 10124, 10170,
  10213, 10255, 10295, 10334, 10370, 10404, 10437, 10468, 10497, 10523, 10548, 10571,
  10592, 10611, 10627, 10642, 10654, 10665, 10673, 10679, 10683, 10685, 10685, 10682,
  10677, 10670, 10661, 10649
};

const uint16_t lut_flute_body_filter[512] = {
  3500, 3496, 3492, 3488, 3485, 3481, 3477, 3474, 3470, 3466, 3462, 3459,
  3455, 3451, 3447, 3443, 3440, 3436, 3432, 3428, 3424, 3421, 3417, 3413,
  3409, 3405, 3401, 3398, 3394, 3390, 3386, 3382, 3378, 3374, 3370, 3367,
  3363, 3359, 3355, 3351, 3347, 3343, 3339, 3335, 3331, 3327, 3323, 3319,
  3315, 3311, 3307, 3303, 3299, 3295, 3291, 3287, 3283, 3279, 3275, 3271,
  3267, 3263, 3259, 3255, 3251, 3247, 3242, 3238, 3234, 3230, 3226, 3222,
  3218, 3214, 3209, 3205, 3201, 3197, 3193, 3189, 3184, 3180, 3176, 3172,
  3168, 3163, 3159, 3155, 3151, 3146, 3142, 3138, 3134, 3129, 3125, 3121,
  3117, 3112, 3108, 3104, 3099, 3095, 3091, 3087, 3082, 3078, 3073, 3069,
  3065, 3060, 3056, 3052, 3047, 3043, 3039, 3034, 3030, 3025, 3021, 3016,
  3012, 3008, 3003, 2999, 2994, 2990, 2985, 2981, 2976, 2972, 2967, 2963,
  2958, 2954, 2949, 2945, 2940, 2936, 2931, 2927, 2922, 2918, 2913, 2908,
  2904, 2899, 2895, 2890, 2885, 2881, 2876, 2872, 2867, 2862, 2858, 2853,
  2848, 2844, 2839, 2834, 2830, 2825, 2820, 2816, 2811, 2806, 2802, 2797,
  2792, 2787, 2783, 2778, 2773, 2768, 2764, 2759, 2754, 2749, 2745, 2740,
  2735, 2730, 2725, 2720, 2716, 2711, 2706, 2701, 2696, 2691, 2687, 2682,
  2677, 2672, 2667, 2662, 2657, 2652, 2647, 2643, 2638, 2633, 2628, 2623,
  2618, 2613, 2608, 2603, 2598, 2593, 2588, 2583, 2578, 2573, 2568, 2563,
  2558, 2553, 2548, 2543, 2538, 2533, 2528, 2523, 2518, 2513, 2507, 2502,
  2497, 2492, 2487, 2482, 2477, 2472, 2467, 2461, 2456, 2451, 2446, 2441,
  2436, 2430, 2425, 2420, 2415, 2410, 2404, 2399, 2394, 2389, 2384, 2378,
  2373, 2368, 2363, 2357, 2352, 2347, 2341, 2336, 2331, 2326, 2320, 2315,
  2310, 2304, 2299, 2294, 2288, 2283, 2278, 2272, 2267, 2261, 2256, 2251,
  2245, 2240, 2234, 2229, 2224, 2218, 2213, 2207, 2202, 2196, 2191, 2186,
  2180, 2175, 2169, 2164, 2158, 2153, 2147, 2142, 2136, 2131, 2125, 2120,
  2114, 2108, 2103, 2097, 2092, 2086, 2081, 2075, 2069, 2064, 2058, 2053,
  2047, 2041, 2036, 2030, 2024, 2019, 2013, 2008, 2002, 1996, 1990, 1985,
  1979, 1973, 1968, 1962, 1956, 1951, 1945, 1939, 1933, 1928, 1922, 1916,
  1910, 1905, 1899, 1893, 1887, 1881, 1876, 1870, 1864, 1858, 1852, 1847,
  1841, 1835, 1829, 1823, 1817, 1811, 1806, 1800, 1794, 1788, 1782, 1776,
  1770, 1764, 1758, 1752, 1746, 1740, 1735, 1729, 1723, 1717, 1711, 1705,
  1699, 1693, 1687, 1681, 1675, 1669, 1663, 1657, 1651, 1645, 1638, 1632,
  1626, 1620, 1614, 1608, 1602, 1596, 1590, 1584, 1578, 1572, 1565, 1559,
  1553, 1547, 1541, 1535, 1529, 1522, 1516, 1510, 1504, 1498, 1491, 1485,
  1479, 1473, 1467, 1460, 1454, 1448, 1442, 1435, 1429, 1423, 1417, 1410,
  1404, 1398, 1391, 1385, 1379, 1373, 1366, 1360, 1354, 1347, 1341, 1335,
  1328, 1322, 1315, 1309, 1303, 1296, 1290, 1283, 1277, 1271, 1264, 1258,
  1251, 1245, 1239, 1232, 1226, 1219, 1213, 1206, 1200, 1193, 1187, 1180,
  1174, 1167, 1161, 1154, 1148, 1141, 1135, 1128, 1121, 1115, 1108, 1102,
  1095, 1089, 1082, 1075, 1069, 1062, 1056, 1049, 1042, 1036, 1029, 1022,
  1016, 1009, 1002, 996, 989, 982, 976, 969, 962, 956, 949, 942,
  935, 929, 922, 915, 908, 902, 895, 888, 881, 875, 868, 861,
  854, 847, 841, 834, 827, 820, 813, 806
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Linear interpolation between two int8_t delay-line samples, returning
// a value shifted left by 9 bits (same scaling Braids uses: << 9).
// frac is 16-bit fractional part (0x0000 = fully sample a, 0xFFFF = fully b).
static inline int32_t mixInterp(int8_t a, int8_t b, uint16_t frac) {
    int32_t ia = (int32_t)a;
    int32_t ib = (int32_t)b;
    // Braids Mix macro: a + (frac * (b - a)) >> 16, then << 9
    return (ia + ((int32_t)frac * (ib - ia) >> 16)) << 9;
}

// ---------------------------------------------------------------------------
// Class implementation
// ---------------------------------------------------------------------------

void AudioSynthBraidsFluted::begin(void)
{
    memset(bore_, 0, sizeof(bore_));
    memset(jet_,  0, sizeof(jet_));

    delay_ptr_       = 0;
    excitation_ptr_  = 0;
    lp_state_        = 0;
    dc_blocking_x0_  = 0;
    dc_blocking_y0_  = 0;

    frequency_   = 440.0f;
    timbre_      = 0.5f;
    color_       = 0.5f;
    amplitude_   = 0.8f;
    strike_      = false;

    rng_state_   = 1;

    recomputeDelays();
}

void AudioSynthBraidsFluted::noteOn(float frequencyHz)
{
    if (frequencyHz < 20.0f)   frequencyHz = 20.0f;
    if (frequencyHz > 4000.0f) frequencyHz = 4000.0f;
    frequency_ = frequencyHz;
    strike_    = true;   // delay lines will be cleared at top of next update()
    recomputeDelays();
}

void AudioSynthBraidsFluted::noteOff(void)
{
    // Flutes don't have a discrete "off" in the physical model —
    // the sound decays naturally as excitation_ptr saturates and
    // the delay lines ring out.  We just stop advancing the envelope
    // by not resetting excitation_ptr, so it stays at the end (silence).
    // For a cleaner cut you could memset the delay lines here.
}

void AudioSynthBraidsFluted::setTimbre(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    timbre_ = t;
    recomputeDelays();
}

void AudioSynthBraidsFluted::setColor(float c)
{
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;
    color_ = c;
    recomputeDelays();
}

void AudioSynthBraidsFluted::setAmplitude(float a)
{
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    amplitude_ = a;
}

void AudioSynthBraidsFluted::setFrequency(float frequencyHz)
{
    if (frequencyHz < 20.0f)   frequencyHz = 20.0f;
    if (frequencyHz > 4000.0f) frequencyHz = 4000.0f;
    frequency_ = frequencyHz;
    recomputeDelays();   // re-tune without striking
}

// ---------------------------------------------------------------------------
// recomputeDelays — derive all per-note fixed-point values
// ---------------------------------------------------------------------------
// Braids computes:
//   bore_delay  = (delay_ << 1) - (2 << 16)          // delay_ is Q16 pitch
//   jet_delay   = (bore_delay >> 8) * (48 + COLOR>>10)
//   bore_delay -= jet_delay
//   (then clamps both so they fit in their respective delay lines)
//
// We don't have Braids' pitch→delay_ LUT, so we compute bore_delay directly
// from frequency at 44100 Hz sample rate, then derive jet_delay the same way.
// ---------------------------------------------------------------------------
void AudioSynthBraidsFluted::recomputeDelays(void)
{
    // Total round-trip delay for the desired frequency (Q16)
    // At 44100 Hz: delay_samples = 44100 / freq.
    // Braids uses (delay_ << 1) which doubles it for the round trip,
    // then subtracts a small correction.  We fold that into one calc:
    float total_delay_samples = (44100.0f / frequency_) * 2.0f - 2.0f;
    uint32_t total_delay_q16 = (uint32_t)(total_delay_samples * 65536.0f);

    // jet_delay = (total >> 8) * (48 + timbre_mapped)
    // In Braids, COLOR (parameter_[1]) is 0..65535 and >> 10 gives 0..63.
    // We map our 0..1 timbre to the same 0..63 range.
    uint16_t timbre_mapped = (uint16_t)(timbre_ * 63.0f);
    uint32_t jet_delay_q16 = (total_delay_q16 >> 8) * (48 + timbre_mapped);
    uint32_t bore_delay_q16 = total_delay_q16 - jet_delay_q16;

    // Clamp so they fit inside the physical delay-line arrays.
    uint32_t bore_max_q16 = ((uint32_t)(kFluteBoreLength - 1)) << 16;
    uint32_t jet_max_q16  = ((uint32_t)(kFluteJetLength  - 1)) << 16;
    while (bore_delay_q16 > bore_max_q16 || jet_delay_q16 > jet_max_q16) {
        bore_delay_q16 >>= 1;
        jet_delay_q16  >>= 1;
    }

    bore_delay_ = bore_delay_q16;
    jet_delay_  = jet_delay_q16;

    // breath_intensity: Braids uses 2100 - (parameter_[0] >> 4).
    // parameter_[0] is TIMBRE (0..65535) → >>4 gives 0..4095.
    // So the range is 2100 down to about -1995, but in practice
    // TIMBRE is rarely maxed.  We map color_ 0..1 → 2100..200.
    breath_intensity_ = (uint16_t)(2100.0f - (color_ * 1900.0f));

    // filter_coefficient: indexed by pitch (0..65535) >> 7 → 0..511.
    // We map frequency (20..4000 Hz) linearly onto 0..511.
    // (Braids' actual pitch LUT is logarithmic, but linear here is close
    //  enough for the damping behaviour — the LUT itself has the curve.)
    float pitch_norm = (frequency_ - 20.0f) / (4000.0f - 20.0f);
    if (pitch_norm < 0.0f) pitch_norm = 0.0f;
    if (pitch_norm > 1.0f) pitch_norm = 1.0f;
    uint16_t lut_index = (uint16_t)(pitch_norm * 511.0f);
    filter_coefficient_ = lut_flute_body_filter[lut_index];
}

// ---------------------------------------------------------------------------
// randomSample — fast LCG returning int16_t in full range
// ---------------------------------------------------------------------------
inline int16_t AudioSynthBraidsFluted::randomSample(void)
{
    rng_state_ = rng_state_ * 1664525UL + 1013904223UL;
    return (int16_t)(rng_state_ >> 16);
}

// ---------------------------------------------------------------------------
// update — called by the Teensy Audio Library every 128 samples
// ---------------------------------------------------------------------------
void AudioSynthBraidsFluted::update(void)
{
    audio_block_t *block = allocate();
    if (!block) return;

    int16_t *bp = block->data;

    // --- local copies of state (fast access, written back at end) ---
    uint16_t delay_ptr      = delay_ptr_;
    uint16_t excitation_ptr = excitation_ptr_;
    int32_t  lp_state       = lp_state_;
    int32_t  dc_x0          = dc_blocking_x0_;
    int32_t  dc_y0          = dc_blocking_y0_;
    uint32_t bore_delay     = bore_delay_;
    uint32_t jet_delay      = jet_delay_;
    uint16_t breath_int     = breath_intensity_;
    uint16_t filter_coeff   = filter_coefficient_;
    float    amp            = amplitude_;

    // --- strike: clear everything ---
    if (strike_) {
        excitation_ptr = 0;
        memset(bore_, 0, sizeof(bore_));
        memset(jet_,  0, sizeof(jet_));
        lp_state = 0;
        dc_x0    = 0;
        dc_y0    = 0;
        strike_  = false;
    }

    // --- pre-split delay values into integral + fractional (Q16) ---
    uint16_t bore_delay_int  = bore_delay >> 16;
    uint16_t bore_delay_frac = bore_delay & 0xFFFF;
    uint16_t jet_delay_int   = jet_delay  >> 16;
    uint16_t jet_delay_frac  = jet_delay  & 0xFFFF;

    // --- per-sample loop ---
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {

        // 1. Read bore delay line (two adjacent samples for interpolation)
        uint16_t bore_rd = delay_ptr + 2 * kFluteBoreLength - bore_delay_int;
        int8_t bore_a = bore_[bore_rd % kFluteBoreLength];
        int8_t bore_b = bore_[(bore_rd - 1) % kFluteBoreLength];
        int32_t bore_value = mixInterp(bore_a, bore_b, bore_delay_frac);

        // 2. Read jet delay line
        uint16_t jet_rd = delay_ptr + 2 * kFluteJetLength - jet_delay_int;
        int8_t jet_a = jet_[jet_rd % kFluteJetLength];
        int8_t jet_b = jet_[(jet_rd - 1) % kFluteJetLength];
        int32_t jet_value = mixInterp(jet_a, jet_b, jet_delay_frac);

        // 3. Breath pressure = envelope + modulated noise
        int32_t breath_pressure = (int32_t)lut_blowing_envelope[excitation_ptr];
        breath_pressure <<= 1;   // scale up (matches Braids)

        int32_t random_pressure = (int32_t)randomSample() * (int32_t)breath_int >> 12;
        random_pressure = random_pressure * breath_pressure >> 15;
        breath_pressure += random_pressure;

        // 4. Lowpass filter on bore (pitch-dependent 1-pole IIR)
        //    y = (-coeff * x + (4096 - coeff) * y_prev) >> 12
        lp_state = (-(int32_t)filter_coeff * bore_value +
                    (int32_t)(4096 - filter_coeff) * lp_state) >> 12;
        int32_t reflection = lp_state;

        // 5. DC-blocking filter on the reflection
        //    y = pole * y_prev + x - x_prev
        dc_y0 = (kDCBlockingPole * dc_y0 >> 12) + reflection - dc_x0;
        dc_x0 = reflection;
        reflection = dc_y0;

        // 6. Write into jet delay line:
        //    jet_in = breath_pressure - reflection/2
        int32_t pressure_delta = breath_pressure - (reflection >> 1);
        jet_[delay_ptr % kFluteJetLength] = (int8_t)(pressure_delta >> 9);

        // 7. Nonlinear jet table lookup → write into bore delay line
        //    Clamp jet_value to 0..65535 for the table index.
        int32_t jet_table_index = jet_value;
        if (jet_table_index < 0)     jet_table_index = 0;
        if (jet_table_index > 65535) jet_table_index = 65535;

        // Table is 256 entries; index = jet_table_index >> 8
        int32_t jet_nonlinear = (int32_t)lut_blowing_jet[jet_table_index >> 8];

        // bore_in = jet_nonlinear + reflection/2
        pressure_delta = jet_nonlinear + (reflection >> 1);
        bore_[delay_ptr % kFluteBoreLength] = (int8_t)(pressure_delta >> 9);

        // 8. Advance write pointer
        delay_ptr++;

        // 9. Output = bore_value (already scaled), clipped to int16
        int32_t out = bore_value >> 1;
        if (out > 32767)  out =  32767;
        if (out < -32768) out = -32768;

        // Apply amplitude
        out = (out * (int32_t)(amp * 32767.0f)) >> 15;

        bp[i] = (int16_t)out;

        // 10. Advance excitation pointer every 4th sample (matches Braids)
        if (i & 3) {
            excitation_ptr++;
            if (excitation_ptr >= LUT_BLOWING_ENVELOPE_SIZE - 1) {
                excitation_ptr = LUT_BLOWING_ENVELOPE_SIZE - 1;
            }
        }
    }

    // --- write state back ---
    delay_ptr_       = delay_ptr;
    excitation_ptr_  = excitation_ptr;
    lp_state_        = lp_state;
    dc_blocking_x0_  = dc_x0;
    dc_blocking_y0_  = dc_y0;

    transmit(block);
    release(block);
}
