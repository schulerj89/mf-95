#include "test_common.h"

int run_asset_tests(void) {
    int failures = 0;
    printf("\n[*] Running Asset Container & Extractor Scaffolding Self-Test...\n");
    if (mf_assets_self_test()) {
        printf("    [PASS] Asset Container: In-memory container parsing, TOC verification,\n");
        printf("           asset lookup by identifier, and CRC32 checks verified.\n");
    } else {
        printf("    [FAIL] Asset Container: Parsing or lookup verification failed.\n");
        failures++;
    }
    return failures;
}
