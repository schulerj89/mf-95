#include "mf_ppu.h"
#include "mf_audio.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("====================================================\n");
    printf("  Madden NFL '95 C Port (mf-95) Scaffolding Tests   \n");
    printf("====================================================\n\n");

    int failures = 0;

    /* 1. Test PPU Scaffolding */
    printf("[*] Running PPU Mode-1 Compositor & Scanline Self-Test...\n");
    if (mf_ppu_self_test()) {
        printf("    [PASS] PPU Self-Test: Mode-1 backgrounds, tile extraction,\n");
        printf("           CGRAM BGR555 conversion, sprite OAM priorities verified.\n");
    } else {
        printf("    [FAIL] PPU Self-Test: Color composition or priority failure.\n");
        failures++;
    }

    /* 2. Test Audio Scaffolding */
    printf("\n[*] Running Multi-Sound Audio Mixer Self-Test...\n");
    if (mf_audio_self_test()) {
        printf("    [PASS] Audio Self-Test: Multi-voice simultaneous playback,\n");
        printf("           additive mixing, 16.16 resampling, voice lifecycle verified.\n");
    } else {
        printf("    [FAIL] Audio Self-Test: Multi-sound mixer or voice control failure.\n");
        failures++;
    }

    printf("\n----------------------------------------------------\n");
    if (failures == 0) {
        printf("Result: ALL SCAFFOLDING TESTS PASSED (PPU + Multi-Voice Audio)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("Result: %d TEST(S) FAILED\n", failures);
        printf("====================================================\n");
        return 1;
    }
}
