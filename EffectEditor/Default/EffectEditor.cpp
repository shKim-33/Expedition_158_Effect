#include <crtdbg.h>
#include <cstdio>
#include <locale.h>
#include <tchar.h>
#include <Windows.h>
#include "EffectEditorApp.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Profile_View.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

FILE* g_debugConsole = nullptr;
HWND g_hWnd = nullptr;
HINSTANCE g_hInst = nullptr;

static auto g_windowClassName = L"Expedition158EffectEditor";
static auto g_windowTitle = L"Expedition_158 EffectEditor";
static constexpr DWORD kFullscreenWindowStyle = WS_POPUP | WS_VISIBLE;
static constexpr DWORD kWindowedWindowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
static constexpr LONG kDefaultWindowedClientWidth = 1600;
static constexpr LONG kDefaultWindowedClientHeight = 900;
static bool g_runtimeInitialized = false;
static bool g_isEditorFullscreen = false;
static RECT g_windowedRect = { 0, 0, 1600, 900 };

static ATOM Register_EffectEditorClass(HINSTANCE hInstance);
static bool Create_EffectEditorWindow(HINSTANCE hInstance, int nCmdShow);
static RECT Make_CenteredWindowRect(const RECT& areaRect, LONG clientWidth, LONG clientHeight);
static RECT Make_DefaultWindowedRect(HWND hWnd);
static void Sync_EffectEditorBackBufferToClient(HWND hWnd);
static void Toggle_EffectEditorFullscreen(HWND hWnd);
static LRESULT CALLBACK EffectEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    Register_EffectEditorClass(hInstance);

    if (!Create_EffectEditorWindow(hInstance, nCmdShow))
        return FALSE;

    g_hInst = hInstance;

    Unique<EffectEditorApp> mainApp = EffectEditorApp::Create();
    CHECK_NULL(mainApp, FALSE);

    CHECK_FAILED(GAME->Add_Timer(TEXT("Timer_Default")), FALSE);

    CHECK_FAILED(GAME->Add_Timer(TEXT("Timer_60FPS")), FALSE);

    g_runtimeInitialized = true;

    MSG msg{};
    float timeAcc = 0.f;
    auto previousFrameTime = Profile_View::clock::now();

    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        timeAcc += GAME->Compute_TimeDelta(TEXT("Timer_Default"));
        if (timeAcc >= 1.f / 60.f)
        {
            const float dt = GAME->Compute_TimeDelta(TEXT("Timer_60FPS"));

            Profile_View::Frame_Begin();

            {
                ENGINE_PROFILE_SCOPE_BASIC("Frame");
                mainApp->Priority_Update(dt);
                mainApp->Update(dt);
                mainApp->Late_Update(dt);
                mainApp->Render();
            }

            const auto currentFrameTime = Profile_View::clock::now();
            const double frameMs = chrono::duration<double, milli>(currentFrameTime - previousFrameTime).count();
            previousFrameTime = currentFrameTime;

            Profile_View::Set_Frame_Time_External(frameMs);
            Profile_View::Set_RenderCallCount_External(GAME->Get_RenderCall());
            Profile_View::Frame_End();

            timeAcc = 0.f;
        }
    }

    g_runtimeInitialized = false;
    mainApp->Free();
    mainApp.reset();

    if (nullptr != g_hWnd && IsWindow(g_hWnd))
    {
        DestroyWindow(g_hWnd);
        g_hWnd = nullptr;
    }

    FreeConsole();

    return static_cast<int>(msg.wParam);
}

static ATOM Register_EffectEditorClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = EffectEditor_WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_windowClassName;

    return RegisterClassExW(&wcex);
}

static bool Create_EffectEditorWindow(HINSTANCE hInstance, int nCmdShow)
{
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);

    const HMONITOR hMonitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return false;

    g_windowedRect = Make_CenteredWindowRect(
        monitorInfo.rcWork,
        kDefaultWindowedClientWidth,
        kDefaultWindowedClientHeight
    );

    g_hWnd = CreateWindowW(
        g_windowClassName,
        g_windowTitle,
        kWindowedWindowStyle,
        g_windowedRect.left,
        g_windowedRect.top,
        g_windowedRect.right - g_windowedRect.left,
        g_windowedRect.bottom - g_windowedRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (nullptr == g_hWnd)
        return false;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return true;
}

static RECT Make_CenteredWindowRect(const RECT& areaRect, LONG clientWidth, LONG clientHeight)
{
    RECT windowRect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&windowRect, kWindowedWindowStyle, FALSE);

    const LONG windowWidth = windowRect.right - windowRect.left;
    const LONG windowHeight = windowRect.bottom - windowRect.top;
    const LONG areaWidth = areaRect.right - areaRect.left;
    const LONG areaHeight = areaRect.bottom - areaRect.top;

    const LONG left = areaRect.left + max(0L, (areaWidth - windowWidth) / 2);
    const LONG top = areaRect.top + max(0L, (areaHeight - windowHeight) / 2);

    return RECT{ left, top, left + windowWidth, top + windowHeight };
}

static RECT Make_DefaultWindowedRect(HWND hWnd)
{
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);

    const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return g_windowedRect;

    return Make_CenteredWindowRect(
        monitorInfo.rcWork,
        kDefaultWindowedClientWidth,
        kDefaultWindowedClientHeight
    );
}

static void Sync_EffectEditorBackBufferToClient(HWND hWnd)
{
    if (!g_runtimeInitialized)
        return;

    RECT clientRect{};
    if (!GetClientRect(hWnd, &clientRect))
        return;

    const uint32 width = static_cast<uint32>(max(1L, clientRect.right - clientRect.left));
    const uint32 height = static_cast<uint32>(max(1L, clientRect.bottom - clientRect.top));

    if (GAME->Get_BackBufferWidth() != width || GAME->Get_BackBufferHeight() != height)
    {
        if (FAILED(GAME->Resize_BackBuffer(width, height)))
            return;
    }

    const uint32 logicalWidth = min(max(1u, GAME->Get_ViewportWidth()), width);
    const uint32 logicalHeight = min(max(1u, GAME->Get_ViewportHeight()), height);
    if (GAME->Get_ViewportWidth() != logicalWidth || GAME->Get_ViewportHeight() != logicalHeight)
        GAME->Apply_LogicalViewportSize(logicalWidth, logicalHeight);
}

static void Toggle_EffectEditorFullscreen(HWND hWnd)
{
    if (g_isEditorFullscreen)
    {
        g_windowedRect = Make_DefaultWindowedRect(hWnd);

        SetWindowLongPtrW(hWnd, GWL_STYLE, kWindowedWindowStyle);
        SetWindowPos(
            hWnd,
            nullptr,
            g_windowedRect.left,
            g_windowedRect.top,
            g_windowedRect.right - g_windowedRect.left,
            g_windowedRect.bottom - g_windowedRect.top,
            SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );
        Sync_EffectEditorBackBufferToClient(hWnd);
        UpdateWindow(hWnd);
        g_isEditorFullscreen = false;
        return;
    }

    GetWindowRect(hWnd, &g_windowedRect);

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(hMonitor, &monitorInfo))
        return;

    const RECT& monitorRect = monitorInfo.rcMonitor;
    SetWindowLongPtrW(hWnd, GWL_STYLE, kFullscreenWindowStyle);
    SetWindowPos(
        hWnd,
        HWND_TOP,
        monitorRect.left,
        monitorRect.top,
        monitorRect.right - monitorRect.left,
        monitorRect.bottom - monitorRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW
    );
    Sync_EffectEditorBackBufferToClient(hWnd);
    UpdateWindow(hWnd);
    g_isEditorFullscreen = true;
}

static LRESULT CALLBACK EffectEditor_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (WM_SYSKEYDOWN == message && VK_RETURN == wParam && 0 != (HIWORD(lParam) & KF_ALTDOWN))
    {
        Toggle_EffectEditorFullscreen(hWnd);
        return 0;
    }

    if (g_runtimeInitialized && ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_CREATE:
    {
        AllocConsole();
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        _tfreopen_s(&g_debugConsole, _T("CONOUT$"), _T("w"), stdout);
        _tfreopen_s(&g_debugConsole, _T("CONIN$"), _T("r"), stdin);
        _tfreopen_s(&g_debugConsole, _T("CONERR$"), _T("w"), stderr);
        _tsetlocale(LC_ALL, _T(""));
        break;
    }

    case WM_SIZE:
        if (g_runtimeInitialized && SIZE_MINIMIZED != wParam)
        {
            const uint32 width = LOWORD(lParam);
            const uint32 height = HIWORD(lParam);

            if (width > 0 && height > 0)
                Sync_EffectEditorBackBufferToClient(hWnd);
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_CLOSE:
        if (g_runtimeInitialized)
            EDITOR->Request_Exit();
        else
            PostQuitMessage(0);
        return 0;

    case WM_MOUSEWHEEL:
    {
        const float wheel = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam));
        GAME->Set_MouseWheel(wheel);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default: break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
