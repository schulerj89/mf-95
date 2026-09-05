#include "mf_types.h"
#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_assets.h"
#include "mf_game.h"
#include "mf_system.h"
#include <stdio.h>
#include <stdlib.h>

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
    case MF_GAME_STATE_INIT_PHASE2: return "INIT PHASE 2";
    case MF_GAME_STATE_INIT_PHASE3: return "INIT PHASE 3";
    case MF_GAME_STATE_INIT_PHASE4: return "INIT PHASE 4";
    case MF_GAME_STATE_INIT_PHASE5: return "INIT PHASE 5";
    case MF_GAME_STATE_INIT_PHASE6: return "INIT PHASE 6";
    case MF_GAME_STATE_INIT_PHASE7: return "INIT PHASE 7";
    case MF_GAME_STATE_INIT_PHASE8: return "INIT PHASE 8";
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
            g_running = false;
            DestroyWindow(hwnd);
        } else if (wParam == 'R') {
            mf_game_init(&g_game);
        }
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    /* Initialize game engine */
    mf_game_init(&g_game);

    /* Register Window Class */
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "MF95_GameWindowClass";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Error", MB_ICONERROR);
        return 1;
    }

    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        "MF95_GameWindowClass",
        "Madden NFL '95 - Native C Port",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create application window.", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    timeBeginPeriod(1);

    DWORD frame_count = 0;
    DWORD last_title_update = GetTickCount();

    /* Primary Interactive Game Loop */
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

        /* Update Title Bar Diagnostic Information */
        DWORD now = GetTickCount();
        if (now - last_title_update >= 500) {
            char title_buf[256];
            const char *pak_status = g_game.assets.is_loaded ? "PAK OK" : "NO PAK";
            snprintf(title_buf, sizeof(title_buf),
                "Madden NFL '95 (C Port) | State: %s | PC: $C1:%04X | [%s: %u entries]",
                get_state_name(g_game.state),
                (unsigned int)(g_game.current_pc & 0xFFFF),
                pak_status,
                g_game.assets.entry_count);
            SetWindowTextA(hwnd, title_buf);
            last_title_update = now;
        }

        /* 60 FPS Frame Pacing (~16 ms) */
        Sleep(16);
    }

    timeEndPeriod(1);
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
