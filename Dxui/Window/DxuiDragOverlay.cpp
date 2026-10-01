#include "Pch.h"

#include "DxuiDragOverlay.h"





static constexpr wchar_t  s_kOverlayClass[] = L"DxuiDragOverlay";





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay::~DxuiDragOverlay
//
////////////////////////////////////////////////////////////////////////////////

DxuiDragOverlay::~DxuiDragOverlay()
{
    if (m_hwnd != nullptr)
    {
        DestroyWindow (m_hwnd);
        m_hwnd = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay::CreateHwnd
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragOverlay::CreateHwnd (HWND owner)
{
    HRESULT     hr        = S_OK;
    HINSTANCE   instance  = GetModuleHandleW (nullptr);
    WNDCLASSEXW wc        = {};
    DWORD       exStyle   = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    ATOM        atom      = 0;
    BOOL        known     = FALSE;



    known = GetClassInfoExW (instance, s_kOverlayClass, &wc);

    if (!known)
    {
        wc               = {};
        wc.cbSize        = sizeof (wc);
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = instance;
        wc.lpszClassName = s_kOverlayClass;

        atom = RegisterClassExW (&wc);
        CWRA (atom);
    }

    m_hwnd = CreateWindowExW (exStyle, s_kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr, instance, nullptr);
    CWRA (m_hwnd);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay::Show
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragOverlay::Show (HWND owner, const RECT & screenRect, const std::vector<DxuiDockDragMark> & marks)
{
    HRESULT        hr      = S_OK;
    int            width   = screenRect.right  - screenRect.left;
    int            height  = screenRect.bottom - screenRect.top;
    HDC            screen  = nullptr;
    HDC            memory  = nullptr;
    HBITMAP        bitmap  = nullptr;
    HGDIOBJ        old     = nullptr;
    void         * bits    = nullptr;
    BITMAPINFO     bmi     = {};
    POINT          dst     = { screenRect.left, screenRect.top };
    POINT          src     = {};
    SIZE           size    = { width, height };
    BLENDFUNCTION  blend   = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    BOOL           updated = FALSE;



    BAIL_OUT_IF (width <= 0 || height <= 0, S_OK);

    if (m_hwnd == nullptr)
    {
        hr = CreateHwnd (owner);
        CHRA (hr);
    }

    bmi.bmiHeader.biSize        = sizeof (bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    screen = GetDC (nullptr);
    CWRA (screen);

    memory = CreateCompatibleDC (screen);
    CWRA (memory);

    bitmap = CreateDIBSection (memory, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    CWRA (bitmap);

    RenderMarks (marks, width, height, (uint32_t *) bits);

    old     = SelectObject (memory, bitmap);
    updated = UpdateLayeredWindow (m_hwnd, screen, &dst, &size, memory, &src, 0, &blend, ULW_ALPHA);
    CWRA (updated);

    if (!m_showing)
    {
        SetWindowPos (m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        m_showing = true;
    }

Error:
    if (old != nullptr)
    {
        SelectObject (memory, old);
    }

    if (bitmap != nullptr)
    {
        DeleteObject (bitmap);
    }

    if (memory != nullptr)
    {
        DeleteDC (memory);
    }

    if (screen != nullptr)
    {
        ReleaseDC (nullptr, screen);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay::Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragOverlay::Hide()
{
    if (m_hwnd != nullptr && m_showing)
    {
        ShowWindow (m_hwnd, SW_HIDE);
    }

    m_showing = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay::RenderMarks
//
//  Clears the image to fully transparent, then lays each mark over it. An
//  outlined mark is the strips DxuiDockSite::GetOutlineStrips gives.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragOverlay::RenderMarks (const std::vector<DxuiDockDragMark> & marks, int width, int height, uint32_t * pixels)
{
    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    std::fill (pixels, pixels + (size_t) width * (size_t) height, 0u);

    for (const DxuiDockDragMark & mark : marks)
    {
        const RECT &  r = mark.rect;
        int           t = mark.outlinePx;

        if (t == 0)
        {
            BlendRect (r, mark.argb, width, height, pixels);
            continue;
        }

        for (const RECT & strip : DxuiDockSite::GetOutlineStrips (mark))
        {
            BlendRect (strip, mark.argb, width, height, pixels);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragOverlay::BlendRect
//
//  Source-over of one straight-alpha color across a rectangle, clipped to
//  the image, into premultiplied pixels.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragOverlay::BlendRect (const RECT & rect, uint32_t argb, int width, int height, uint32_t * pixels)
{
    uint32_t  a     = argb >> 24;
    uint32_t  r     = ((argb >> 16) & 0xFFu) * a / 255u;
    uint32_t  g     = ((argb >>  8) & 0xFFu) * a / 255u;
    uint32_t  b     = ( argb        & 0xFFu) * a / 255u;
    uint32_t  keep  = 255u - a;
    int       left  = std::max (0L, rect.left);
    int       top   = std::max (0L, rect.top);
    int       right = std::min ((LONG) width,  rect.right);
    int       bot   = std::min ((LONG) height, rect.bottom);



    for (int y = top; y < bot; y++)
    {
        uint32_t * row = pixels + (size_t) y * (size_t) width;

        for (int x = left; x < right; x++)
        {
            uint32_t  d = row[x];

            row[x] = ((a + ((d >> 24)         ) * keep / 255u) << 24) |
                     ((r + ((d >> 16) & 0xFFu) * keep / 255u) << 16) |
                     ((g + ((d >>  8) & 0xFFu) * keep / 255u) <<  8) |
                      (b + ( d        & 0xFFu) * keep / 255u);
        }
    }
}
