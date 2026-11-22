#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeTextGeometry
//
//  A text view's bounds at the origin for the text view's components: the
//  text 6 pixels in from the left and top, 8-pixel columns, and 10-pixel
//  scrollbars, at 96 DPI.
//
////////////////////////////////////////////////////////////////////////////////

class FakeTextGeometry : public IDxuiTextGeometry
{
public:
    FakeTextGeometry (LONG width, LONG height) : m_bounds { 0, 0, width, height }
    {
        m_scaler.SetDpi (96);
    }

    const RECT           & GetViewBounds  () const override               { return m_bounds; }
    POINT                  GetTextOrigin  () const override               { return POINT { kPadPx, kPadPx }; }
    int                    GetTextColumns (bool scrollbar) const override { return (std::max) (1, ((int) m_bounds.right - kPadPx * 2 - (scrollbar ? kBarPx : 0)) / kColumnPx); }
    int                    GetPadPx       () const override               { return kPadPx; }
    int                    GetBarWidthPx  () const override               { return kBarPx; }
    const DxuiDpiScaler  & GetScaler      () const override               { return m_scaler; }

    static constexpr int  kPadPx    = 6;
    static constexpr int  kBarPx    = 10;
    static constexpr int  kColumnPx = 8;

private:
    RECT           m_bounds;
    DxuiDpiScaler  m_scaler;
};
