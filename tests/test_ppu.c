#include "test_common.h"

int run_ppu_tests(void) {
    int failures = 0;
    printf("[*] Running PPU Mode-1 Compositor & Scanline Self-Test...\n");
    if (mf_ppu_self_test()) {
        printf("    [PASS] PPU Self-Test: Mode-1 backgrounds, tile extraction,\n");
        printf("           CGRAM BGR555 conversion, sprite OAM priorities verified.\n");
    } else {
        printf("    [FAIL] PPU Self-Test: Color composition or priority failure.\n");
        failures++;
    }
    return failures;
}
