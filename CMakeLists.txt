cmake_minimum_required(VERSION 3.20)
project(mf95 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

set(MF95_SOURCES
    src/mf_ppu.c
    src/mf_audio.c
    src/mf_assets.c
    src/mf_intro.c
    src/mf_system.c
    src/mf_game.c
)

set(MF95_HEADERS
    include/mf_types.h
    include/mf_ppu.h
    include/mf_audio.h
    include/mf_assets.h
    include/mf_intro.h
    include/mf_system.h
    include/mf_game.h
)

# Core library
add_library(mf95_core STATIC ${MF95_SOURCES} ${MF95_HEADERS})

if(WIN32)
    target_link_libraries(mf95_core winmm)
endif()

# Test runner
set(MF95_TEST_SOURCES
    tests/test_common.h
    tests/test_ppu.c
    tests/test_audio.c
    tests/test_assets.c
    tests/test_intro.c
    tests/test_boot.c
    tests/test_scenes.c
    tests/test_main.c
)
add_executable(mf95_tests ${MF95_TEST_SOURCES})
target_include_directories(mf95_tests PRIVATE tests)
target_link_libraries(mf95_tests mf95_core)

# Interactive application executable with CLI console
add_executable(mf95 src/main.c)
target_link_libraries(mf95 mf95_core)

if(WIN32)
    target_link_libraries(mf95 user32 gdi32 winmm)
endif()

if(MSVC)
    target_compile_options(mf95_core PRIVATE /W4 /utf-8)
    target_compile_options(mf95_tests PRIVATE /W4 /utf-8)
    target_compile_options(mf95 PRIVATE /W4 /utf-8)
else()
    target_compile_options(mf95_core PRIVATE -Wall -Wextra)
    target_compile_options(mf95_tests PRIVATE -Wall -Wextra)
    target_compile_options(mf95 PRIVATE -Wall -Wextra)
    target_link_libraries(mf95_tests m)
    target_link_libraries(mf95 m)
endif()
