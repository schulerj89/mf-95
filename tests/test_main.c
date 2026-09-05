#include "test_common.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("====================================================\n");
    printf("  Madden NFL '95 C Port (mf-95) Modular Unit Tests   \n");
    printf("====================================================\n\n");

    int failures = 0;

    /* 1. PPU Subsystem Tests */
    failures += run_ppu_tests();

    /* 2. Audio Subsystem Tests */
    failures += run_audio_tests();

    /* 3. Boot Sequence Subsystem Tests ($00:CB63 .. $C0:CB9C) */
    failures += run_boot_tests();

    /* 4. Scene & Game Mode Tests ($C1:4DE2, $C1:5467) */
    failures += run_scene_tests();

    /* 5. Asset Container & Packaging Tests */
    failures += run_asset_tests();

    printf("\n----------------------------------------------------\n");
    if (failures == 0) {
        printf("Result: ALL TESTS PASSED (PPU + Audio + Boot Sequence + Scenes + Assets)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("Result: %d TEST(S) FAILED\n", failures);
        printf("====================================================\n");
        return 1;
    }
}
