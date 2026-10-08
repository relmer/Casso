#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/ColorLegend.h"






//  Mid-tone colors that read on a light and a dark background alike, one per
//  source in MemorySource order. None has no color here: it is drawn in the
//  theme's divider color, as an absence rather than a source.
static constexpr uint32_t  s_kSourceColors[] =
{
    0x00000000,     // None
    0xFF4A90D9,     // Main
    0xFFD98A4A,     // Aux
    0xFF5BB36A,     // LcBank1
    0xFF2E8B57,     // LcBank2
    0xFF9B6FD0,     // Rom
    0xFFC9A227,     // SlotRom
    0xFFD0505A,     // Io
};

static constexpr const wchar_t * s_kSourceNames[] =
{
    L"none",
    L"main",
    L"aux",
    L"LC bank 1",
    L"LC bank 2",
    L"ROM",
    L"slot ROM",
    L"I/O",
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetSourceColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t MemoryMapBar::GetSourceColor (MemorySource source)
{
    size_t  index = (size_t) source;



    return (index < std::size (s_kSourceColors)) ? s_kSourceColors[index] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetColorOf
//
////////////////////////////////////////////////////////////////////////////////

uint32_t MemoryMapBar::GetColorOf (MemorySource source) const
{
    size_t  index = (size_t) source;



    if (index < m_colors.size() && m_colors[index] != 0)
    {
        return m_colors[index];
    }

    return GetSourceColor (source);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetSourceName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * MemoryMapBar::GetSourceName (MemorySource source)
{
    size_t  index = (size_t) source;



    return (index < std::size (s_kSourceNames)) ? s_kSourceNames[index] : L"?";
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetPreferredHeightPx
//
//  The read strip and the write strip. What each color is, each run's tip
//  says (TryGetTipAt).
//
////////////////////////////////////////////////////////////////////////////////

int MemoryMapBar::GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    constexpr int  kStrips = 2;



    UNREFERENCED_PARAMETER (widthPx);

    return scaler.ToPx ((kStripDip + kGapDip) * kStrips);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::TryGetTipAt
//
//  Over a run of pages from one source, in either strip: the source's color
//  and what it is, which way the strip goes, and the run's addresses, "Blue:
//  reads come from Main RAM ($0000-$BFFF)".
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryMapBar::TryGetTipAt (POINT point, std::wstring & text) const
{
    float         strip   = m_scaler.ToPxf ((float) kStripDip);
    float         gap     = m_scaler.ToPxf ((float) kGapDip);
    float         top     = (float) m_boundsDip.top;
    float         y       = (float) point.y;
    float         x       = (float) point.x;
    float         left    = GetPageX (0);
    float         right   = GetPageX ((int) DiagnosticsMemoryMap::kPageCount);
    bool          isWrite = false;
    int           page    = 0;
    int           first   = 0;
    int           end     = 0;
    MemorySource  source  = MemorySource::None;
    auto          sourceOf = [this, &isWrite] (int at) { return isWrite ? m_map.pages[(size_t) at].write : m_map.pages[(size_t) at].read; };



    if (!m_visible || x < left || x >= right || right <= left)
    {
        return false;
    }

    if (y >= top && y < top + strip)
    {
        isWrite = false;
    }
    else if (y >= top + strip + gap && y < top + strip + gap + strip)
    {
        isWrite = true;
    }
    else
    {
        return false;
    }

    page   = std::clamp ((int) ((x - left) / (right - left) * (float) DiagnosticsMemoryMap::kPageCount), 0, (int) DiagnosticsMemoryMap::kPageCount - 1);
    source = sourceOf (page);
    first  = page;
    end    = page + 1;

    while (first > 0 && sourceOf (first - 1) == source)
    {
        first--;
    }

    while (end < (int) DiagnosticsMemoryMap::kPageCount && sourceOf (end) == source)
    {
        end++;
    }

    if (source == MemorySource::None)
    {
        text = std::format (L"Nothing is {} here (${:04X}-${:04X})", isWrite ? L"written" : L"read", first * 0x100, end * 0x100 - 1);
        return true;
    }

    text = std::format (L"{}: {} {} (${:04X}-${:04X})", ColorLegend::GetColorName (GetColorOf (source)),
                        isWrite ? L"writes go to" : L"reads come from", GetSourceDescription (source), first * 0x100, end * 0x100 - 1);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetSourceDescription
//
//  The memory map key's own words for the source.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * MemoryMapBar::GetSourceDescription (MemorySource source)
{
    switch (source)
    {
    case MemorySource::Main:    return ColorLegend::GetText (ColorLegend::Meaning::MapMain);
    case MemorySource::Aux:     return ColorLegend::GetText (ColorLegend::Meaning::MapAux);
    case MemorySource::LcBank1: return ColorLegend::GetText (ColorLegend::Meaning::MapLcBank1);
    case MemorySource::LcBank2: return ColorLegend::GetText (ColorLegend::Meaning::MapLcBank2);
    case MemorySource::Rom:     return ColorLegend::GetText (ColorLegend::Meaning::MapRom);
    case MemorySource::SlotRom: return ColorLegend::GetText (ColorLegend::Meaning::MapSlotRom);
    case MemorySource::Io:      return ColorLegend::GetText (ColorLegend::Meaning::MapIo);
    default:                    return GetSourceName (source);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetPageX
//
////////////////////////////////////////////////////////////////////////////////

float MemoryMapBar::GetPageX (int page) const
{
    float  label = m_scaler.ToPxf ((float) kLabelDip);
    float  width = std::max (0.0f, (float) (m_boundsDip.right - m_boundsDip.left) - label);



    return (float) m_boundsDip.left + label + width * (float) page / (float) DiagnosticsMemoryMap::kPageCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::Paint
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font  = theme.MonospaceFont();
    float           strip = m_scaler.ToPxf ((float) kStripDip);
    float           gap   = m_scaler.ToPxf ((float) kGapDip);
    float           label = m_scaler.ToPxf ((float) kLabelDip);
    float           left  = (float) m_boundsDip.left;
    float           top   = (float) m_boundsDip.top;
    HRESULT         hr    = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    PaintStrip (painter, theme, top,               false);
    PaintStrip (painter, theme, top + strip + gap, true);

    hr = text.DrawString (L"R", left, top, label, strip, theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (L"W", left, top + strip + gap, label, strip, theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::PaintStrip
//
//  A run of pages with the same source is one fill, so the usual map of a
//  few large regions is a handful of rectangles rather than 256.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::PaintStrip (IDxuiPainter & painter, const IDxuiTheme & theme, float y, bool isWrite) const
{
    float  strip = m_scaler.ToPxf ((float) kStripDip);
    int    first = 0;



    while (first < (int) DiagnosticsMemoryMap::kPageCount)
    {
        MemorySource  source = isWrite ? m_map.pages[(size_t) first].write : m_map.pages[(size_t) first].read;
        int           end    = first + 1;
        uint32_t      color  = (source == MemorySource::None) ? theme.Divider() : GetColorOf (source);

        while (end < (int) DiagnosticsMemoryMap::kPageCount &&
               (isWrite ? m_map.pages[(size_t) end].write : m_map.pages[(size_t) end].read) == source)
        {
            end++;
        }

        painter.FillRect (GetPageX (first), y, GetPageX (end) - GetPageX (first), strip, color);
        first = end;
    }
}
