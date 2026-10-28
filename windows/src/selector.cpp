// DrawingsGone for Windows — drag a rectangle to pick the area to filter out.
#include "selector.h"

#include <algorithm>

namespace {

const wchar_t* kSelectClass = L"DG_Select";

struct Selector {
    HWND hwnd = nullptr;
    RECT origin = {0, 0, 0, 0};  // virtual screen origin
    int width = 0;
    int height = 0;
    HDC bright = nullptr;   // the desktop as it was when selection started
    HDC dark = nullptr;     // ...and the same picture under a 38% black wash
    HBITMAP brightBitmap = nullptr;
    HBITMAP darkBitmap = nullptr;
    HBITMAP brightOld = nullptr;
    HBITMAP darkOld = nullptr;
    POINT start = {0, 0};
    POINT current = {0, 0};
    bool dragging = false;
    bool finished = false;
    std::function<void(bool, RECT)> done;

    void release() {
        if (bright && brightOld) {
            SelectObject(bright, brightOld);
        }
        if (dark && darkOld) {
            SelectObject(dark, darkOld);
        }
        brightOld = darkOld = nullptr;
        if (brightBitmap) {
            DeleteObject(brightBitmap);
            brightBitmap = nullptr;
        }
        if (darkBitmap) {
            DeleteObject(darkBitmap);
            darkBitmap = nullptr;
        }
        if (bright) {
            DeleteDC(bright);
            bright = nullptr;
        }
        if (dark) {
            DeleteDC(dark);
            dark = nullptr;
        }
    }
};

HFONT uiFont(int size, int weight) {
    return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void paint(Selector* state) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(state->hwnd, &ps);

    RECT client;
    GetClientRect(state->hwnd, &client);

    RECT selection = {0, 0, 0, 0};
    bool haveSelection = false;
    if (state->dragging) {
        selection.left = std::min(state->start.x, state->current.x);
        selection.top = std::min(state->start.y, state->current.y);
        selection.right = std::max(state->start.x, state->current.x);
        selection.bottom = std::max(state->start.y, state->current.y);
        haveSelection = selection.right - selection.left > 1 && selection.bottom - selection.top > 1;
    }

    if (state->dark) {
        BitBlt(dc, 0, 0, client.right, client.bottom, state->dark, 0, 0, SRCCOPY);
    }
    if (haveSelection && state->bright) {
        BitBlt(dc, selection.left, selection.top, selection.right - selection.left,
               selection.bottom - selection.top, state->bright, selection.left, selection.top,
               SRCCOPY);

        HPEN pen = CreatePen(PS_SOLID, 2, RGB(236, 55, 80));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, selection.left, selection.top, selection.right, selection.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);

        wchar_t size[64];
        wsprintfW(size, L"%d \u00d7 %d", static_cast<int>(selection.right - selection.left),
                  static_cast<int>(selection.bottom - selection.top));
        HFONT font = uiFont(14, FW_SEMIBOLD);
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SIZE extent = {0, 0};
        GetTextExtentPoint32W(dc, size, static_cast<int>(wcslen(size)), &extent);
        RECT badge = {selection.left + 8, selection.top + 8, selection.left + 8 + extent.cx + 18,
                      selection.top + 8 + extent.cy + 10};
        HBRUSH badgeBrush = CreateSolidBrush(RGB(236, 55, 80));
        FillRect(dc, &badge, badgeBrush);
        DeleteObject(badgeBrush);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT text = badge;
        text.left += 9;
        DrawTextW(dc, size, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }

    const wchar_t* hint = L"Drag to select the area to filter out     Esc to cancel";
    HFONT hintFont = uiFont(16, FW_SEMIBOLD);
    HGDIOBJ oldFont = SelectObject(dc, hintFont);
    SetBkMode(dc, TRANSPARENT);
    SIZE hintSize = {0, 0};
    GetTextExtentPoint32W(dc, hint, static_cast<int>(wcslen(hint)), &hintSize);
    int pillWidth = hintSize.cx + 40;
    int pillHeight = hintSize.cy + 24;
    RECT pill = {client.right / 2 - pillWidth / 2, 56, client.right / 2 + pillWidth / 2,
                 56 + pillHeight};
    HBRUSH pillBrush = CreateSolidBrush(RGB(8, 9, 12));
    FillRect(dc, &pill, pillBrush);
    DeleteObject(pillBrush);
    SetTextColor(dc, RGB(240, 240, 244));
    RECT hintRect = pill;
    DrawTextW(dc, hint, -1, &hintRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
    DeleteObject(hintFont);

    EndPaint(state->hwnd, &ps);
}

void finish(Selector* state, bool chosen, const RECT& rect) {
    if (!state || state->finished) {
        return;
    }
    state->finished = true;
    auto done = state->done;
    HWND hwnd = state->hwnd;
    ReleaseCapture();
    DestroyWindow(hwnd);
    if (done) {
        done(chosen, rect);
    }
}

LRESULT CALLBACK SelectProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<Selector*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            if (state) {
                paint(state);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (state) {
                state->dragging = true;
                state->start.x = GET_X_LPARAM(lparam);
                state->start.y = GET_Y_LPARAM(lparam);
                state->current = state->start;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (state && state->dragging) {
                state->current.x = GET_X_LPARAM(lparam);
                state->current.y = GET_Y_LPARAM(lparam);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (state && state->dragging) {
                state->dragging = false;
                POINT end = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                RECT rect;
                rect.left = std::min(state->start.x, end.x);
                rect.top = std::min(state->start.y, end.y);
                rect.right = std::max(state->start.x, end.x);
                rect.bottom = std::max(state->start.y, end.y);
                bool big = rect.right - rect.left >= 24 && rect.bottom - rect.top >= 24;
                if (big) {
                    OffsetRect(&rect, state->origin.left, state->origin.top);
                }
                finish(state, big, rect);
            }
            return 0;
        case WM_RBUTTONDOWN:
            finish(state, false, RECT{0, 0, 0, 0});
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                finish(state, false, RECT{0, 0, 0, 0});
            }
            return 0;
        case WM_DESTROY:
            if (state) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                state->release();
                delete state;
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

void beginRegionSelection(HINSTANCE instance, std::function<void(bool, RECT)> finished) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW cls = {};
        cls.cbSize = sizeof(cls);
        cls.lpfnWndProc = SelectProc;
        cls.hInstance = instance;
        cls.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        cls.lpszClassName = kSelectClass;
        RegisterClassExW(&cls);
        registered = true;
    }

    auto* state = new Selector();
    state->done = std::move(finished);
    state->origin.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    state->origin.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    state->width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    state->height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screen = GetDC(nullptr);
    state->bright = CreateCompatibleDC(screen);
    state->brightBitmap = CreateCompatibleBitmap(screen, state->width, state->height);
    state->brightOld = static_cast<HBITMAP>(SelectObject(state->bright, state->brightBitmap));
    BitBlt(state->bright, 0, 0, state->width, state->height, screen, state->origin.left,
           state->origin.top, SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, screen);

    // The 38% black wash is drawn once; the picker then only has to blit.
    state->dark = CreateCompatibleDC(state->bright);
    state->darkBitmap = CreateCompatibleBitmap(state->bright, state->width, state->height);
    state->darkOld = static_cast<HBITMAP>(SelectObject(state->dark, state->darkBitmap));
    BitBlt(state->dark, 0, 0, state->width, state->height, state->bright, 0, 0, SRCCOPY);
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 97, 0};
    HDC wash = CreateCompatibleDC(state->dark);
    HBITMAP washBitmap = CreateCompatibleBitmap(state->dark, state->width, state->height);
    HGDIOBJ washOld = SelectObject(wash, washBitmap);
    RECT full = {0, 0, state->width, state->height};
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(wash, &full, brush);
    DeleteObject(brush);
    AlphaBlend(state->dark, 0, 0, state->width, state->height, wash, 0, 0, state->width,
               state->height, blend);
    SelectObject(wash, washOld);
    DeleteObject(washBitmap);
    DeleteDC(wash);

    state->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kSelectClass,
                                  L"DrawingsGone select area", WS_POPUP, state->origin.left,
                                  state->origin.top, state->width, state->height, nullptr, nullptr,
                                  instance, nullptr);
    if (!state->hwnd) {
        state->release();
        delete state;
        return;
    }

    SetWindowLongPtrW(state->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    ShowWindow(state->hwnd, SW_SHOW);
    SetForegroundWindow(state->hwnd);
    SetFocus(state->hwnd);
    SetCapture(state->hwnd);
}
