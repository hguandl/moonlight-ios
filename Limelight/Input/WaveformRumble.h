#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ControllerHaptics.h"

// PCM-to-RMS approximation. Caller locks state; time is monotonic seconds.
typedef struct {
    bool ready, hasSequence, active;
    uint32_t sequence, frames;
    double lastNonzeroTime, leftPower, rightPower;
    uint16_t rumbleLeft, rumbleRight;
} WaveformRumble;

static inline void WaveformRumbleClearEnergy(WaveformRumble *state)
{
    state->frames = 0;
    state->leftPower = state->rightPower = 0;
}

static inline bool WaveformRumblePush(WaveformRumble *state, uint32_t sequence,
                                      const uint8_t *pcm, uint16_t frames, double now)
{
    if (!state->ready || !pcm || frames == 0 || frames > ML_HAPTICS_MAX_FRAMES) return false;
    uint32_t delta = sequence - state->sequence;
    if (state->hasSequence && (delta == 0 || delta >= 0x80000000u)) return false;
    // Keep at most 20 ms of contiguous energy.
    if (!state->hasSequence || delta != 1 || state->frames + frames > ML_HAPTICS_SAMPLE_RATE / 50 ||
            (state->active && now - state->lastNonzeroTime >= 0.1)) {
        WaveformRumbleClearEnergy(state);
    }
    state->hasSequence = true;
    state->sequence = sequence;
    double power[2] = {0};
    for (unsigned i = 0; i < frames; i++) {
        for (unsigned channel = 0; channel < 2; channel++) {
            const uint8_t *sample = pcm + i * ML_HAPTICS_FRAME_BYTES + channel * 2;
            int value = MlHapticsRead16(sample);
            if (value >= 32768) value -= 65536;
            double normalized = value / 32768.0;
            power[channel] += normalized * normalized;
        }
    }
    // Silence cannot acquire or extend PCM ownership.
    if (power[0] + power[1] > frames * 0.000025) {
        if (!state->active) WaveformRumbleClearEnergy(state);
        state->active = true;
        state->lastNonzeroTime = now;
    }
    state->leftPower += power[0];
    state->rightPower += power[1];
    state->frames += frames;
    return true;
}

static inline bool WaveformRumblePoll(WaveformRumble *state, double now, uint16_t *left, uint16_t *right)
{
    if (!state->ready || !state->active) return false;
    if (now - state->lastNonzeroTime >= 0.1) {
        state->active = false;
        WaveformRumbleClearEnergy(state);
        *left = state->rumbleLeft;
        *right = state->rumbleRight;
        return true;
    }
    if (!state->frames) return false;
    *left = (uint16_t)fmin(65535, sqrt(state->leftPower / state->frames) * 65535);
    *right = (uint16_t)fmin(65535, sqrt(state->rightPower / state->frames) * 65535);
    WaveformRumbleClearEnergy(state);
    return true;
}
