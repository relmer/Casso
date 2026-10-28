#pragma once

#include "Pch.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar
//
//  All 256 pages of the address space in two wide vertical strips, page $00
//  at the top: what each page reads from beside what it writes to, colored
//  by source. Every run of pages of one source is at least kMinRunDip tall,
//  the height taken from the larger runs, so a single I/O page is as easy
//  to see and to point at as main RAM; a run with room has its source's
//  name inside it. Each run's first address is a label outside its strip,
//  the read strip's on the left and the write strip's on the right, left
//  out where it would overlap the one above. A run's tip has its source and
//  the addresses it covers.
//
//  Bounds are pixels, like every Dxui control; the bar scales only its own
//  metrics.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryMapBar : public IDxuiControl
{
public:
    static constexpr int  kStripDip    = 96;
    static constexpr int  kStripGapDip = 4;
    static constexpr int  kBoundsDip   = 44;
    static constexpr int  kHeadingDip  = 18;
    static constexpr int  kNamePadDip  = 2;
    static constexpr int  kMinRunDip   = 20;

    void                          SetMap (const DiagnosticsMemoryMap & map);
    const DiagnosticsMemoryMap &  GetMap () const { return m_map; }

    //  The width of the two strips and their bounds' labels.
    static int                    GetPreferredWidthPx (const DxuiDpiScaler & scaler);

    //  The tip over a run of pages from one source in either strip, or false
    //  off the strips.
    bool                          TryGetTipAt (POINT point, std::wstring & text) const;

    //  One color per source; None is drawn in the theme's divider color, so it
    //  has no entry of its own.
    static uint32_t               GetSourceColor (MemorySource source);
    static const wchar_t *        GetSourceName  (MemorySource source);
    static const wchar_t *        GetSourceLabel (MemorySource source);
    static const wchar_t *        GetSourceDescription (MemorySource source);

    //  The theme's colors in place of the mid-tones, one per source in
    //  MemorySource order; a zero keeps that source's mid-tone.
    using SourceColors = std::array<uint32_t, (size_t) MemorySource::Count>;

    void                          SetSourceColors (const SourceColors & colors) { m_colors = colors; }
    uint32_t                      GetColorOf      (MemorySource source) const;

    //  Where a strip starts, in pixels from the bar's left; and where a page's
    //  segment starts, in pixels from the bar's top, the same in both strips.
    //  Page 256 is the strips' foot.
    float                         GetStripX (bool isWrite) const;
    float                         GetPageY  (int page, bool isWrite) const;

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Memory map"; }

private:
    //  Pages first to end - 1 of one source, from top to bottom in pixels.
    struct Run
    {
        int    first  = 0;
        int    end    = 0;
        float  top    = 0.0f;
        float  bottom = 0.0f;
    };

    void          PlaceRuns         ();
    float         GetAxisY          (int page) const;
    static void   ShareHeight       (std::vector<Run> & runs, float top, float height, float minimum);
    const Run *   FindRunAt         (float y, bool isWrite) const;
    MemorySource  GetSourceOf       (int page, bool isWrite) const;
    bool          IsInExpansionRom  (int page, bool isWrite) const;
    bool          IsSameRun         (int page, int other, bool isWrite) const;
    std::wstring  GetRunLabel       (int first, bool isWrite) const;
    std::wstring  GetRunDescription (int first, bool isWrite) const;
    void          PaintStrip        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, bool isWrite) const;
    void          PaintBounds       (IDxuiTextRenderer & text, const IDxuiTheme & theme, bool isWrite) const;

    DiagnosticsMemoryMap               m_map;
    DxuiDpiScaler                      m_scaler;
    SourceColors                       m_colors  = {};
    std::array<std::vector<Run>, 2>    m_runs;           // read, then write
    std::vector<Run>                   m_axis;           // the pieces between every page where either strip changes source
};
