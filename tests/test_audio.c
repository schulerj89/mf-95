#include "test_common.h"

int run_audio_tests(void) {
    int failures = 0;
    printf("\n[*] Running Multi-Sound Audio Mixer Self-Test...\n");
    if (mf_audio_self_test()) {
        printf("    [PASS] Audio Self-Test: Multi-voice simultaneous playback,\n");
        printf("           additive mixing, 16.16 resampling, voice lifecycle verified.\n");
    } else {
        printf("    [FAIL] Audio Self-Test: Multi-sound mixer or voice control failure.\n");
        failures++;
    }
    return failures;
}
