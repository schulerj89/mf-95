#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "mf_types.h"
#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_assets.h"
#include "mf_game.h"
#include "mf_system.h"

int run_ppu_tests(void);
int run_audio_tests(void);
int run_asset_tests(void);
int run_intro_tests(void);
int run_boot_tests(void);
int run_scene_tests(void);

#endif /* TEST_COMMON_H */
