#pragma once

#include "Pch.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar
//
//  All 256 pages of the address space in two strips, what each page reads
//  from over what it writes to, colored by source. A run's tip says what its
//  color is and the addresses it covers.
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

    void                          SetMap (const DiagnosticsMemoryMap & map) { m_map = map; }
    const DiagnosticsMemoryMap &  GetMap () const                           { return m_map; }

    int                           GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;

    //  The tip over a run of pages from one source in either strip, or false
    //  off the strips.
    bool                          TryGetTipAt (POINT point, std::wstring & text) const;

    //  One color per source; None is drawn in the theme's divider color, so it
    //  has no entry of its own.
    static uint32_t               GetSourceColor (MemorySource source);
    static const wchar_t *        GetSourceName  (MemorySource source);
    static const wchar_t *        GetSourceDescription (MemorySource source);

    //  The theme's colors in place of the mid-tones, one per source in
    //  MemorySource order; a zero keeps that source's mid-tone.
    using SourceColors = std::array<uint32_t, (size_t) MemorySource::Count>;

    void                          SetSourceColors (const SourceColors & colors) { m_colors = colors; }
    uint32_t                      GetColorOf      (MemorySource source) const;

    //  Where a page's strip segment starts, in pixels from the bar's left.
    float                         GetPageX       (int page) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Memory map"; }

private:
    void  PaintStrip (IDxuiPainter & painter, const IDxuiTheme & theme, float y, bool isWrite) const;

    DiagnosticsMemoryMap  m_map;
    DxuiDpiScaler         m_scaler;
    SourceColors          m_colors  = {};
};
