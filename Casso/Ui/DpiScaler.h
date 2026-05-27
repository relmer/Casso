#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DpiScaler
//
//  Per-window holder of the effective DPI (96 * scale%). Owned by
//  UiShell and updated on WM_DPICHANGED. Layout / Paint code receives
//  it by const-reference and asks for scaled values directly --
//  callers never do the MulDiv math themselves and never thread a
//  raw `dpi` parameter through every function.
//
//  Multiple windows can each own their own scaler with different
//  effective DPIs, so this is deliberately a non-singleton instance
//  member.
//
////////////////////////////////////////////////////////////////////////////////

class DpiScaler
{
public:
    static constexpr UINT  kBaseDpi = 96;

    void   SetDpi (UINT dpi) { m_dpi = (dpi == 0) ? kBaseDpi : dpi; }
    UINT   Dpi    () const   { return m_dpi; }

    int    Px     (int   dp) const { return MulDiv (dp, (int) m_dpi, (int) kBaseDpi); }
    float  Pxf    (float dp) const { return dp * (float) m_dpi / (float) kBaseDpi; }

private:
    UINT  m_dpi = kBaseDpi;
};
