#include "mf_types.h"
#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_assets.h"
#include "mf_game.h"
#include "mf_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>

#define WINDOW_WIDTH  768
#define WINDOW_HEIGHT 672
#define SCREEN_W      256
#define SCREEN_H      224

static mf_game_t g_game;
static bool g_running = true;

static const char *get_state_name(mf_game_state_t st) {
    switch (st) {
    case MF_GAME_STATE_RESET:       return "RESET";
    case MF_GAME_STATE_BOOT:        return "COLD BOOT";
    case MF_GAME_STATE_INIT_SYSTEM: return "INIT SYSTEM ($C1:0000)";
    case MF_GAME_STATE_INIT_PHASE2: return "INIT PHASE 2 ($C1:22C0)";
    case MF_GAME_STATE_INIT_PHASE3: return "INIT PHASE 3 ($C1:1823)";
    case MF_GAME_STATE_INIT_PHASE4: return "INIT PHASE 4 ($C1:0966)";
    case MF_GAME_STATE_INIT_PHASE5: return "INIT PHASE 5 ($C1:1F04)";
    case MF_GAME_STATE_INIT_PHASE6: return "INIT PHASE 6 ($C0:CE46)";
    case MF_GAME_STATE_INIT_PHASE7: return "INIT PHASE 7 ($C1:39F3)";
    case MF_GAME_STATE_INIT_PHASE8: return "INIT PHASE 8 ($C1:22C6)";
    case MF_GAME_STATE_BOOT_TABLES: return "BOOT TABLES ($C0:CB9C)";
    case MF_GAME_STATE_TITLE:       return "TITLE SCREEN ($C1:4DE2)";
    case MF_GAME_STATE_MENU:        return "MAIN MENU ($C1:5467 / $C1:55AE)";
    case MF_GAME_STATE_GAMEPLAY:    return "GAMEPLAY";
    default:                        return "UNKNOWN";
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = SCREEN_W;
        bmi.bmiHeader.biHeight = -SCREEN_H; /* Top-down DIB */
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        StretchDIBits(hdc,
            0, 0, client.right - client.left, client.bottom - client.top,
            0, 0, SCREEN_W, SCREEN_H,
            g_game.ppu.framebuffer,
            &bmi,
            DIB_RGB_COLORS,
            SRCCOPY);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            printf("[INPUT] ESC pressed -> Exiting...\n");
            g_running = false;
            DestroyWindow(hwnd);
        } else if (wParam == 'R') {
            printf("[INPUT] 'R' pressed -> Triggering soft reset...\n");
            mf_game_init(&g_game);
        } else if (wParam == VK_RETURN) {
            printf("[INPUT] START / RETURN pressed\n");
        } else if (wParam == VK_SPACE) {
            printf("[INPUT] BUTTON A / SPACE pressed\n");
        } else if (wParam == VK_UP) {
            printf("[INPUT] D-PAD UP pressed\n");
        } else if (wParam == VK_DOWN) {
            printf("[INPUT] D-PAD DOWN pressed\n");
        } else if (wParam == VK_LEFT) {
            printf("[INPUT] D-PAD LEFT pressed\n");
        } else if (wParam == VK_RIGHT) {
            printf("[INPUT] D-PAD RIGHT pressed\n");
        }
        return 0;
    case WM_DESTROY:
        printf("[SYSTEM] Window closed by user.\n");
        g_running = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("==========================================================\n");
    printf("  Madden NFL '95 (mf-95) - Native C Port Interactive CLI  \n");
    printf("==========================================================\n\n");

    /* Step 1: Initialize Engine */
    printf("[1/5] Initializing engine core and hardware subsystems...\n");
    mf_game_init(&g_game);

    /* Step 2: Check Asset Pack */
    printf("[2/5] Inspecting local asset pack...\n");
    if (g_game.assets.is_loaded) {
        printf("      [+] Asset Pack loaded successfully: %u entries, %zu bytes\n",
               g_game.assets.entry_count, g_game.assets.raw_data_size);
        for (uint32_t i = 0; i < g_game.assets.entry_count; i++) {
            printf("          - %-22s (size: %5u B, CRC: 0x%08X)\n",
                   g_game.assets.entries[i].name,
                   g_game.assets.entries[i].size,
                   g_game.assets.entries[i].crc32);
        }
    } else {
        printf("      [-] Asset pack 'assets/madden95.pak' not present.\n");
        printf("          (Using cleanroom built-in static tables and fallback palettes).\n");
    }

    /* Step 3: Register and Create Window */
    printf("\n[3/5] Spawning 3x scaled display window (768x672 -> 256x224)...\n");
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "MF95_GameWindowClass";

    RegisterClassExA(&wc);

    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        "MF95_GameWindowClass",
        "Madden NFL '95 - Native C Port",
        (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);
        printf("      [+] Graphical window created and visible.\n");
    } else {
        printf("      [!] Window creation skipped/unavailable. Running in pure CLI Console Mode.\n");
    }


    /* Step 4: Step through initial boot sequence */
    printf("\n[4/5] Executing initial boot sequence to scene entry...\n");
    mf_game_state_t last_logged_state = g_game.state;
    printf("      -> State: %s (PC: $0x%06X)\n", get_state_name(g_game.state), (unsigned int)g_game.current_pc);

    /* Step until we reach the interactive scene waiting point */
    for (int step_count = 0; step_count < 20; step_count++) {
        mf_game_step(&g_game);
        if (g_game.state != last_logged_state) {
            printf("      -> State: %s (PC: $0x%06X, Next: $0x%06X)\n",
                   get_state_name(g_game.state),
                   (unsigned int)g_game.current_pc,
                   (unsigned int)g_game.next_pc);
            last_logged_state = g_game.state;
        }
        if (g_game.state == MF_GAME_STATE_MENU && g_game.next_pc == MF_SNES_ADDR_MENU_POLL) {
            break;
        }
    }

    printf("\n[5/5] Entering interactive game loop (60 FPS)...\n");
    printf("----------------------------------------------------------\n");
    printf("Controls:\n");
    printf("  [Arrow Keys] D-Pad Navigation\n");
    printf("  [Enter]      Start Button\n");
    printf("  [Space]      Button A\n");
    printf("  [Z]          Button B\n");
    printf("  [R]          Soft Reset\n");
    printf("  [ESC]        Quit Application\n");
    printf("----------------------------------------------------------\n\n");

    timeBeginPeriod(1);

    DWORD frame_count = 0;
    DWORD last_second = GetTickCount();

    while (g_running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!g_running) break;

        /* Step game state machine and render scanlines */
        mf_game_step(&g_game);
        frame_count++;

        /* Request window redraw */
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);

        /* Diagnostic logging every second */
        DWORD now = GetTickCount();
        if (now - last_second >= 1000) {
            char title_buf[256];
            const char *pak_status = g_game.assets.is_loaded ? "PAK OK" : "NO PAK";
            snprintf(title_buf, sizeof(title_buf),
                "Madden NFL '95 (C Port) | %s | PC: $0x%06X | %s",
                get_state_name(g_game.state),
                (unsigned int)g_game.current_pc,
                pak_status);
            SetWindowTextA(hwnd, title_buf);

            printf("[HEARTBEAT] Frame %-6u | FPS: ~60 | State: %s | PPU Brightness: %u | ForcedBlank: %s\n",
                   frame_count,
                   get_state_name(g_game.state),
                   g_game.ppu.brightness,
                   g_game.ppu.forced_blank ? "YES" : "NO");
            last_second = now;
        }

        /* 60 FPS Frame Pacing (~16 ms) */
        Sleep(16);
    }

    timeEndPeriod(1);
    printf("\n[SHUTDOWN] Interactive session ended cleanly.\n");
    return 0;
}

#else

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    mf_game_t game;
    mf_game_init(&game);
    printf("Madden NFL '95 Native C Port Headless Console Runner\n");
    for (int i = 0; i < 60; i++) {
        mf_game_step(&game);
    }
    printf("Completed 60 frames. Current PC: $0x%06X\n", (unsigned int)game.current_pc);
    return 0;
}

#endif
