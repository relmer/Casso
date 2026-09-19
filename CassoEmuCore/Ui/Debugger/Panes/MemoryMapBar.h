#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar
//
//  All 256 pages of the address space in two strips, what each page reads
//  from over what it writes to, colored by source, with a key below naming
//  the colors.
//
//  Bounds are pixels, like every Dxui control; the bar scales only its own
//  metrics.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryMapBar : public IDxuiControl
{
public:
    static constexpr int  kStripDip     = 12;
    static constexpr int  kLabelDip     = 16;
    static constexpr int  kGapDip       = 2;
    static constexpr int  kKeyDip       = 16;
    static constexpr int  kSwatchDip    = 10;
    static constexpr int  kKeyItemDip   = 64;
    static constexpr int  kKeyIndentDip = 14;

    void                          SetMap (const DiagnosticsMemoryMap & map) { m_map = map; }
    const DiagnosticsMemoryMap &  GetMap () const                           { return m_map; }

    int                           GetPreferredHeightPx (const DxuiDpiScaler & scaler) const;

    //  One color per source; None is drawn in the theme's divider color, so it
    //  has no entry of its own.
    static uint32_t               GetSourceColor (MemorySource source);
    static const wchar_t *        GetSourceName  (MemorySource source);

    //  Where a page's strip segment starts, in pixels from the bar's left.
    float                         GetPageX       (int page) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Memory map"; }

private:
    void  PaintStrip (IDxuiPainter & painter, const IDxuiTheme & theme, float y, bool isWrite) const;
    void  PaintKey   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float y) const;

    DiagnosticsMemoryMap  m_map;
    DxuiDpiScaler         m_scaler;
};
