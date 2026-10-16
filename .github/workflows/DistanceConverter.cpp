// ============================================================================
//  Distance converter - converts kilometres to miles (native Win32 API)
//
//  Build in Visual Studio 2019/2022:
//    1. File > New > Project > "Windows Desktop Application" (C++).
//       Delete the generated .cpp/.h/.rc files.
//    2. Project > Add Existing Item... and pick this file.
//    3. Choose Release | x64 and Build > Build Solution.
//
//  Or from a "Developer Command Prompt for VS":
//    cl /EHsc /O2 /MT /DUNICODE /D_UNICODE DistanceConverter.cpp /link /SUBSYSTEM:WINDOWS
//
//  Usage: type a number of kilometres - the miles value updates as you type.
//    Enter = Convert, Esc = Clear, Tab = move between controls.
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>

#include <cmath>
#include <cstdlib>
#include <cwchar>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ----------------------------------------------------------------------------
//  Conversion logic (no Win32 calls in here)
// ----------------------------------------------------------------------------
// BEGIN LOGIC
static const double KM_TO_MILES = 0.621371192237334;

enum ParseResult { PR_EMPTY, PR_BAD, PR_OK };

// Accepts plain decimal numbers such as "12", "12.5", "-3", "1e3".
static ParseResult ParseKm(const wchar_t* in, double& out)
{
    wchar_t buf[64];

    while (*in == L' ' || *in == L'\t') ++in;
    size_t len = wcslen(in);
    while (len > 0 && (in[len - 1] == L' ' || in[len - 1] == L'\t')) --len;

    if (len == 0)  return PR_EMPTY;
    if (len >= 63) return PR_BAD;

    for (size_t i = 0; i < len; ++i) {
        wchar_t c = in[i];
        bool ok = (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' ||
                  c == L'+' || c == L'e' || c == L'E';
        if (!ok) return PR_BAD;
        buf[i] = c;
    }
    buf[len] = 0;

    wchar_t* end = nullptr;
    out = wcstod(buf, &end);
    if (end == buf || *end != 0) return PR_BAD;
    if (!std::isfinite(out))     return PR_BAD;
    return PR_OK;
}

// Formats e.g. 3.106855 -> "3.1069 miles" (up to 4 decimals, no trailing zeros).
static void FormatMiles(double mi, wchar_t* out, size_t cap)
{
    if (std::fabs(mi) >= 1e15) {
        swprintf(out, cap, L"%.6g miles", mi);
        return;
    }
    wchar_t num[64];
    swprintf(num, 64, L"%.4f", mi);

    size_t l = wcslen(num);
    if (wcschr(num, L'.')) {
        while (l > 0 && num[l - 1] == L'0') num[--l] = 0;
        if (l > 0 && num[l - 1] == L'.')    num[--l] = 0;
    }
    if (wcscmp(num, L"-0") == 0) { num[0] = L'0'; num[1] = 0; }

    swprintf(out, cap, L"%ls %ls", num, wcscmp(num, L"1") == 0 ? L"mile" : L"miles");
}
// END LOGIC

// ----------------------------------------------------------------------------
//  UI
// ----------------------------------------------------------------------------
static const wchar_t* APP_TITLE  = L"Distance converter";
static const wchar_t* MAIN_CLASS = L"Win32DistanceConverter";

enum { IDC_EDIT = 101, IDC_CONVERT = 102, IDC_CLEAR = 103, IDC_RESULT = 104 };

static HWND  g_hwnd = nullptr, g_edit = nullptr, g_result = nullptr;
static HFONT g_font = nullptr, g_bigFont = nullptr;
static int   g_dpi = 96;
static bool  g_error = false;

static int Sc(int v) { return MulDiv(v, g_dpi, 96); }

static void UpdateResult()
{
    if (!g_edit || !g_result) return;

    wchar_t text[64];
    GetWindowTextW(g_edit, text, 64);

    double km = 0;
    wchar_t out[96];
    g_error = false;

    switch (ParseKm(text, km)) {
    case PR_EMPTY:
        wcscpy_s(out, L"—");
        break;
    case PR_BAD:
        g_error = true;
        wcscpy_s(out, L"Enter a number");
        break;
    default:
        FormatMiles(km * KM_TO_MILES, out, 96);
        break;
    }
    SetWindowTextW(g_result, out);
    InvalidateRect(g_result, nullptr, TRUE);
}

static void CreateControls(HWND hwnd)
{
    auto make = [&](const wchar_t* cls, const wchar_t* txt, DWORD style, DWORD ex,
                    int x, int y, int w, int h, int id) -> HWND {
        HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | style,
                                 Sc(x), Sc(y), Sc(w), Sc(h), hwnd,
                                 (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
        return c;
    };

    make(L"STATIC", L"Kilometres (km)", SS_LEFT, 0, 20, 16, 320, 18, -1);
    g_edit = make(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                  20, 38, 320, 28, IDC_EDIT);
    SendMessageW(g_edit, EM_LIMITTEXT, 40, 0);
    SendMessageW(g_edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"e.g. 10");

    make(L"BUTTON", L"Convert", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 20, 76, 154, 30, IDC_CONVERT);
    make(L"BUTTON", L"Clear",   WS_TABSTOP | BS_PUSHBUTTON,    0, 186, 76, 154, 30, IDC_CLEAR);

    make(L"STATIC", L"Miles (mi)", SS_LEFT, 0, 20, 122, 320, 18, -1);
    g_result = make(L"STATIC", L"—", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0,
                    20, 142, 320, 44, IDC_RESULT);
    SendMessageW(g_result, WM_SETFONT, (WPARAM)g_bigFont, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        SetFocus(g_edit);
        return 0;

    case WM_SETFOCUS:
        if (g_edit) SetFocus(g_edit);
        return 0;

    case DM_GETDEFID:                       // Enter key -> Convert button
        return MAKELRESULT(IDC_CONVERT, DC_HASDEFID);

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_EDIT:
            if (HIWORD(wp) == EN_CHANGE) UpdateResult();
            break;
        case IDC_CONVERT:
            UpdateResult();
            SetFocus(g_edit);
            SendMessageW(g_edit, EM_SETSEL, 0, -1);
            break;
        case IDC_CLEAR:
        case IDCANCEL:                      // Esc
            SetWindowTextW(g_edit, L"");
            SetFocus(g_edit);
            break;
        }
        return 0;

    case WM_CTLCOLORSTATIC:
        if ((HWND)lp == g_result && g_error) {
            HDC dc = (HDC)wp;
            SetTextColor(dc, RGB(200, 0, 0));
            SetBkMode(dc, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;

    case WM_DESTROY:
        if (g_font)    { DeleteObject(g_font);    g_font = nullptr; }
        if (g_bigFont) { DeleteObject(g_bigFont); g_bigFont = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    SetProcessDPIAware();

    HDC dc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW big = ncm.lfMessageFont;
    big.lfHeight = big.lfHeight * 2;
    big.lfWeight = FW_SEMIBOLD;
    g_bigFont = CreateFontIndirectW(&big);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm       = wc.hIcon;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = MAIN_CLASS;
    if (!RegisterClassExW(&wc)) return 1;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = { 0, 0, Sc(360), Sc(200) };
    AdjustWindowRect(&rc, style, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.left + ((wa.right - wa.left) - w) / 2;
    int y = wa.top  + ((wa.bottom - wa.top) - h) / 2;

    g_hwnd = CreateWindowExW(0, MAIN_CLASS, APP_TITLE, style, x, y, w, h,
                             nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(g_hwnd, &msg)) {   // gives Tab / Enter / Esc handling
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
