#pragma once
#import <GameController/GameController.h>

// Wire layouts: https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
static inline BOOL ApplyDualSenseTriggerEffect(GCDualSenseAdaptiveTrigger *trigger, uint8_t type,
                                               const uint8_t *data)
    API_AVAILABLE(ios(14.5), tvos(14.5), macos(11.3))
{
    switch (type) {
        case 0x00:
        case 0x05:
            [trigger setModeOff];
            return YES;

        case 0x01:
            [trigger setModeFeedbackWithStartPosition:data[0] / 255.0f resistiveStrength:data[1] / 255.0f];
            return YES;

        case 0x11:
            if (data[1] > 10) break;
            [trigger setModeFeedbackWithStartPosition:data[0] / 255.0f resistiveStrength:data[1] / 10.0f];
            return YES;

        case 0x02:
            if (data[0] >= data[1]) break;
            [trigger setModeWeaponWithStartPosition:data[0] / 255.0f endPosition:data[1] / 255.0f
                                 resistiveStrength:data[2] / 255.0f];
            return YES;

        case 0x12:
            if (data[0] < 16 || data[0] >= data[1] || data[1] > data[0] + 100 || data[2] > 10) break;
            [trigger setModeWeaponWithStartPosition:data[0] / 255.0f endPosition:data[1] / 255.0f
                                 resistiveStrength:data[2] / 10.0f];
            return YES;

        case 0x06:
            if (!data[0] || !data[1]) {
                [trigger setModeOff];
                return YES;
            }
            [trigger setModeVibrationWithStartPosition:data[2] / 255.0f amplitude:data[1] / 255.0f
                                            frequency:data[0] / 255.0f];
            return YES;

        case 0x21:
        case 0x26:
            if (@available(iOS 15.4, tvOS 15.4, macOS 12.3, *)) {
                uint16_t zones = data[0] | ((uint16_t)data[1] << 8);
                uint32_t packed = data[2] | ((uint32_t)data[3] << 8) |
                                  ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24);
                if (!(zones & 0x3ff) || (type == 0x26 && !data[8])) {
                    [trigger setModeOff];
                    return YES;
                }
                GCDualSenseAdaptiveTriggerPositionalResistiveStrengths strengths = {0};
                GCDualSenseAdaptiveTriggerPositionalAmplitudes amplitudes = {0};
                for (unsigned i = 0; i < 10; i++) {
                    float value = (zones & (1u << i)) ? (((packed >> (3 * i)) & 7) + 1) / 8.0f : 0;
                    strengths.values[i] = amplitudes.values[i] = value;
                }
                if (type == 0x21) {
                    [trigger setModeFeedbackWithResistiveStrengths:strengths];
                } else {
                    [trigger setModeVibrationWithAmplitudes:amplitudes frequency:data[8] / 255.0f];
                }
                return YES;
            }
            break;

        case 0x22: // Approximate bow with weapon.
        case 0x23: // Approximate gallop with vibration.
        case 0x27: // Average alternating amplitudes.
        case 0x25: {
            uint16_t zones = data[0] | ((uint16_t)data[1] << 8);
            unsigned start = 10, end = 0, count = 0;
            for (unsigned i = 0; i < 10; i++) {
                if (zones & (1u << i)) {
                    if (!count) start = i;
                    end = i;
                    count++;
                }
            }
            if (count != 2 || start > 8 || end > 9) break;
            if (type == 0x23 || type == 0x27) {
                unsigned a = data[2] & 7, b = (data[2] >> 3) & 7;
                if (type == 0x23 && b >= a) break;
                float amplitude = type == 0x23 ? 0.5f : (a + b) / 14.0f;
                if (!data[3] || amplitude == 0) {
                    [trigger setModeOff];
                    return YES;
                }
                if (@available(iOS 15.4, tvOS 15.4, macOS 12.3, *)) {
                    GCDualSenseAdaptiveTriggerPositionalAmplitudes amplitudes = {0};
                    for (unsigned i = start; i <= end; i++) amplitudes.values[i] = amplitude;
                    [trigger setModeVibrationWithAmplitudes:amplitudes frequency:data[3] / 255.0f];
                } else {
                    // Approximate range with onset.
                    [trigger setModeVibrationWithStartPosition:start / 9.0f amplitude:amplitude
                                                    frequency:data[3] / 255.0f];
                }
                return YES;
            }
            if (end > 8 || (type == 0x25 && start < 2)) break;
            [trigger setModeWeaponWithStartPosition:start / 9.0f endPosition:end / 9.0f
                                 resistiveStrength:((data[2] & 7) + 1) / 8.0f];
            return YES;
        }
    }

    [trigger setModeOff];
    return NO;
}
