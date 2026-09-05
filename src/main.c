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
static bool g_show_diagnostics = false;
static char g_last_input[64] = "None (Awaiting Input)";

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

static void draw_hud(HDC hdc, int width, int height) {
    (void)height;
    SetBkMode(hdc, TRANSPARENT);

    /* Background banner for HUD */
    HBRUSH bgBrush = CreateSolidBrush(RGB(16, 20, 32));
    HBRUSH borderBrush = CreateSolidBrush(RGB(48, 80, 140));
    RECT bannerRect = { 10, 10, width - 10, 195 };
    FillRect(hdc, &bannerRect, bgBrush);
    FrameRect(hdc, &bannerRect, borderBrush);

    HFONT hFontTitle = CreateFontA(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Segoe UI");
    HFONT hFontText = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    HGDIOBJ oldFont = SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, RGB(255, 215, 0)); /* Gold */
    TextOutA(hdc, 24, 18, "MADDEN NFL '95 - NATIVE C ENGINE DISPLAY", 40);

    SelectObject(hdc, hFontText);
    SetTextColor(hdc, RGB(220, 220, 240));

    char buf[256];
    snprintf(buf, sizeof(buf), "State: %s (PC: $0x%06X)", get_state_name(g_game.state), (unsigned int)g_game.current_pc);
    TextOutA(hdc, 24, 48, buf, (int)strlen(buf));

    snprintf(buf, sizeof(buf), "Video: SNES Mode 2 (256x224 -> 3x Scale) | Brightness: %u/15 | Blank: %s",
             g_game.ppu.brightness, g_game.ppu.forced_blank ? "YES" : "NO");
    TextOutA(hdc, 24, 70, buf, (int)strlen(buf));

    snprintf(buf, sizeof(buf), "Audio: Ports [$2140: 0x%02X, $2141: 0x%02X] (Menu Track $4A37 Queued)",
             g_game.apu_ports[0], g_game.apu_ports[1]);
    TextOutA(hdc, 24, 92, buf, (int)strlen(buf));

    snprintf(buf, sizeof(buf), "Assets: %s (%u entries, %zu bytes)",
             g_game.assets.is_loaded ? "assets/madden95.pak Loaded" : "Cleanroom Fallbacks",
             g_game.assets.entry_count, g_game.assets.raw_data_size);
    TextOutA(hdc, 24, 114, buf, (int)strlen(buf));

    snprintf(buf, sizeof(buf), "Live Controller Input: %s | Target: sub_c1579e_menu_render", g_last_input);
    SetTextColor(hdc, RGB(100, 255, 120)); /* Bright green */
    TextOutA(hdc, 24, 136, buf, (int)strlen(buf));

    SetTextColor(hdc, RGB(180, 180, 180));
    TextOutA(hdc, 24, 162, "Controls: [Arrows] D-Pad  [Enter] Start  [Space] A  [Z] B  [R] Reset  [ESC] Exit", 76);

    /* Palette Swatches bar */
    RECT swatchBox = { 10, height - 60, width - 10, height - 10 };
    FillRect(hdc, &swatchBox, bgBrush);
    FrameRect(hdc, &swatchBox, borderBrush);

    SetTextColor(hdc, RGB(200, 200, 200));
    TextOutA(hdc, 24, height - 52, "Active CGRAM Palette Swatches (UI / Gradient / Highlight):", 58);

    /* Draw small color blocks for the first 32 CGRAM colors */
    int swatch_x = 24;
    int swatch_y = height - 32;
    for (int i = 0; i < 32 && (swatch_x + 18) < (width - 24); i++) {
        uint32_t argb = mf_ppu_cgram_to_argb(g_game.ppu.cgram, (uint8_t)i, 15);
        COLORREF color = RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
        HBRUSH cBrush = CreateSolidBrush(color);
        RECT r = { swatch_x, swatch_y, swatch_x + 16, swatch_y + 16 };
        FillRect(hdc, &r, cBrush);
        FrameRect(hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
        DeleteObject(cBrush);
        swatch_x += 20;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(hFontTitle);
    DeleteObject(hFontText);
    DeleteObject(bgBrush);
    DeleteObject(borderBrush);
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

        if (g_show_diagnostics) {
            draw_hud(hdc, client.right - client.left, client.bottom - client.top);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            snprintf(g_last_input, sizeof(g_last_input), "ESC (Quitting)");
            printf("[INPUT] ESC pressed -> Exiting...\n");
            g_running = false;
            DestroyWindow(hwnd);
        } else if (wParam == 'R') {
            snprintf(g_last_input, sizeof(g_last_input), "R (Soft Reset)");
            printf("[INPUT] 'R' pressed -> Triggering soft reset...\n");
            mf_game_init(&g_game);
        } else if (wParam == VK_RETURN) {
            snprintf(g_last_input, sizeof(g_last_input), "START / RETURN");
            printf("[INPUT] START / RETURN pressed\n");
            g_game.wram[0x00EC] |= 0x10;
        } else if (wParam == VK_F1) {
            g_show_diagnostics = !g_show_diagnostics;
            snprintf(g_last_input, sizeof(g_last_input), "F1 (Diagnostics %s)",
                     g_show_diagnostics ? "On" : "Off");
            printf("[DISPLAY] Diagnostic overlay %s\n",
                   g_show_diagnostics ? "enabled" : "disabled");
        } else if (wParam == VK_SPACE) {
            snprintf(g_last_input, sizeof(g_last_input), "BUTTON A / SPACE");
            printf("[INPUT] BUTTON A / SPACE pressed\n");
        } else if (wParam == 'Z') {
            snprintf(g_last_input, sizeof(g_last_input), "BUTTON B / 'Z'");
            printf("[INPUT] BUTTON B / 'Z' pressed\n");
        } else if (wParam == VK_UP) {
            snprintf(g_last_input, sizeof(g_last_input), "D-PAD UP");
            printf("[INPUT] D-PAD UP pressed\n");
        } else if (wParam == VK_DOWN) {
            snprintf(g_last_input, sizeof(g_last_input), "D-PAD DOWN");
            printf("[INPUT] D-PAD DOWN pressed\n");
        } else if (wParam == VK_LEFT) {
            snprintf(g_last_input, sizeof(g_last_input), "D-PAD LEFT");
            printf("[INPUT] D-PAD LEFT pressed\n");
        } else if (wParam == VK_RIGHT) {
            snprintf(g_last_input, sizeof(g_last_input), "D-PAD RIGHT");
            printf("[INPUT] D-PAD RIGHT pressed\n");
        }
        InvalidateRect(hwnd, NULL, FALSE);
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
    int headless_frames = 0;
    const char *screenshot_path = "screen_capture.bmp";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            headless_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--headless") == 0) {
            if (headless_frames <= 0) headless_frames = 60;
        }
    }

    if (headless_frames > 0) {
        printf("==========================================================\n");
        printf("  Madden NFL '95 (mf-95) - Headless Frame Capture Runner  \n");
        printf("==========================================================\n\n");
        printf("[HEADLESS] Stepping %d frames...\n", headless_frames);
        mf_game_init(&g_game);
        for (int f = 1; f <= headless_frames; f++) {
            mf_game_step(&g_game);
        }
        printf("[HEADLESS] Completed %d frames. Final state: %s (PC: $0x%06X)\n",
               headless_frames, get_state_name(g_game.state), (unsigned int)g_game.current_pc);
        if (mf_ppu_save_bmp(&g_game.ppu, screenshot_path)) {
            printf("[HEADLESS] Framebuffer capture saved successfully to: %s\n", screenshot_path);
        } else {
            printf("[HEADLESS] Error: Failed to write screenshot to: %s\n", screenshot_path);
        }
        return 0;
    }

    /* Ensure dedicated CLI console window exists and is visible */
    HWND hConsole = GetConsoleWindow();
    if (!hConsole) {
        AllocConsole();
        hConsole = GetConsoleWindow();
        FILE *fpCon;
        freopen_s(&fpCon, "CONOUT$", "w", stdout);
        freopen_s(&fpCon, "CONOUT$", "w", stderr);
        freopen_s(&fpCon, "CONIN$", "r", stdin);
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (hConsole) {
        SetConsoleTitleA("Madden NFL '95 - Engine CLI Diagnostics");
        ShowWindow(hConsole, SW_SHOW);
        SetWindowPos(hConsole, HWND_TOP, 30, 30, 840, 720, SWP_SHOWWINDOW);
        SetForegroundWindow(hConsole);
    }

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
        "Madden NFL '95 - Native C Port (Display Output)",
        (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX),
        890, 30,
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

    /* Step through hardware cold boot to Title / Intro scene entry */
    for (int step_count = 0; step_count < 20; step_count++) {
        if (g_game.state == MF_GAME_STATE_TITLE) {
            break;
        }
        mf_game_step(&g_game);
        if (g_game.state != last_logged_state) {
            printf("      -> State: %s (PC: $0x%06X, Next: $0x%06X)\n",
                   get_state_name(g_game.state),
                   (unsigned int)g_game.current_pc,
                   (unsigned int)g_game.next_pc);
            last_logged_state = g_game.state;
        }
    }

    printf("\n[5/5] Entering interactive game loop (60 FPS)...\n");
    printf("----------------------------------------------------------\n");
    printf("Controls (the game image is unobstructed by default):\n");
    printf("  [Arrow Keys] D-Pad Navigation\n");
    printf("  [Enter]      Start Button\n");
    printf("  [Space]      Button A\n");
    printf("  [Z]          Button B\n");
    printf("  [R]          Soft Reset\n");
    printf("  [F1]         Toggle Diagnostic Overlay\n");
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
