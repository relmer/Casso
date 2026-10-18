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

//  The words drawn inside a run tall enough to have them.
static constexpr const wchar_t * s_kSourceLabels[] =
{
    L"",
    L"Main RAM",
    L"Aux RAM",
    L"LC bank 1",
    L"LC bank 2",
    L"ROM",
    L"Slot ROM",
    L"I/O",
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
//  MemoryMapBar::GetSourceLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * MemoryMapBar::GetSourceLabel (MemorySource source)
{
    size_t  index = (size_t) source;



    return (index < std::size (s_kSourceLabels)) ? s_kSourceLabels[index] : L"";
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
//  MemoryMapBar::GetPreferredWidthPx
//
//  The read strip with its bounds on its left, and the write strip with
//  its bounds on its right.
//
////////////////////////////////////////////////////////////////////////////////

int MemoryMapBar::GetPreferredWidthPx (const DxuiDpiScaler & scaler)
{
    return scaler.ToPx (kBoundsDip * 2 + kStripDip * 2 + kStripGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetStripX
//
////////////////////////////////////////////////////////////////////////////////

float MemoryMapBar::GetStripX (bool isWrite) const
{
    return (float) m_boundsDip.left + m_scaler.ToPxf ((float) kBoundsDip) + (isWrite ? m_scaler.ToPxf ((float) (kStripDip + kStripGapDip)) : 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetPageY
//
//  Below the headings, page $00 at the top and page $FF at the bottom.
//
////////////////////////////////////////////////////////////////////////////////

float MemoryMapBar::GetPageY (int page) const
{
    float  top    = (float) m_boundsDip.top + m_scaler.ToPxf ((float) kHeadingDip);
    float  height = std::max (0.0f, (float) m_boundsDip.bottom - top);



    return top + height * (float) page / (float) DiagnosticsMemoryMap::kPageCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::GetSourceOf
//
////////////////////////////////////////////////////////////////////////////////

MemorySource MemoryMapBar::GetSourceOf (int page, bool isWrite) const
{
    return isWrite ? m_map.pages[(size_t) page].write : m_map.pages[(size_t) page].read;
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
    float         x       = (float) point.x;
    float         y       = (float) point.y;
    float         top     = GetPageY (0);
    float         bottom  = GetPageY ((int) DiagnosticsMemoryMap::kPageCount);
    bool          isWrite = false;
    int           page    = 0;
    int           first   = 0;
    int           end     = 0;
    MemorySource  source  = MemorySource::None;



    if (!m_visible || y < top || y >= bottom || bottom <= top)
    {
        return false;
    }

    if (x >= GetStripX (false) && x < GetStripX (false) + strip)
    {
        isWrite = false;
    }
    else if (x >= GetStripX (true) && x < GetStripX (true) + strip)
    {
        isWrite = true;
    }
    else
    {
        return false;
    }

    page   = std::clamp ((int) ((y - top) / (bottom - top) * (float) DiagnosticsMemoryMap::kPageCount), 0, (int) DiagnosticsMemoryMap::kPageCount - 1);
    source = GetSourceOf (page, isWrite);
    first  = page;
    end    = page + 1;

    while (first > 0 && GetSourceOf (first - 1, isWrite) == source)
    {
        first--;
    }

    while (end < (int) DiagnosticsMemoryMap::kPageCount && GetSourceOf (end, isWrite) == source)
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
//  Each strip under its heading, then each strip's bounds.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           strip = m_scaler.ToPxf ((float) kStripDip);
    float           top   = (float) m_boundsDip.top;
    HRESULT         hr    = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left || m_boundsDip.bottom <= m_boundsDip.top)
    {
        return;
    }

    for (bool isWrite : { false, true })
    {
        hr = text.DrawString (isWrite ? L"Write" : L"Read", GetStripX (isWrite), top, strip, m_scaler.ToPxf ((float) kHeadingDip),
                              theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        PaintStrip  (painter, text, theme, isWrite);
        PaintBounds (text, theme, isWrite);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::PaintStrip
//
//  A run of pages with the same source is one fill, so the usual map of a
//  few large regions is a handful of rectangles rather than 256. The source's
//  name is centered in a run that has room for it.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::PaintStrip (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, bool isWrite) const
{
    DxuiFontHandle  font  = theme.BodyFont();
    float           size  = m_scaler.ToPxf (font.sizeDip);
    float           strip = m_scaler.ToPxf ((float) kStripDip);
    float           pad   = m_scaler.ToPxf ((float) kNamePadDip);
    float           x     = GetStripX (isWrite);
    int             first = 0;
    HRESULT         hr    = S_OK;



    while (first < (int) DiagnosticsMemoryMap::kPageCount)
    {
        MemorySource     source = GetSourceOf (first, isWrite);
        int              end    = first + 1;
        uint32_t         color  = (source == MemorySource::None) ? theme.Divider() : GetColorOf (source);
        const wchar_t  * label  = GetSourceLabel (source);
        float            y      = GetPageY (first);
        float            height = 0.0f;
        float            w      = 0.0f;
        float            h      = 0.0f;
        HRESULT          fit    = E_FAIL;

        while (end < (int) DiagnosticsMemoryMap::kPageCount && GetSourceOf (end, isWrite) == source)
        {
            end++;
        }

        height = GetPageY (end) - y;
        painter.FillRect (x, y, strip, height, color);

        fit = (*label != L'\0') ? text.MeasureString (label, size, font.face, w, h) : E_FAIL;

        if (SUCCEEDED (fit) && h + pad * 2.0f <= height && w + pad * 2.0f <= strip)
        {
            hr = text.DrawString (label, x, y, strip, height, DxuiColor::ChooseInkFor (color, 0xFF000000u, 0xFFFFFFFFu), size, font.face,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        first = end;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::PaintBounds
//
//  Each run's first address beside the strip at its top edge, and $FFFF at
//  the strip's foot: the read strip's on its left, the write strip's on
//  its right. A label that would overlap the one above it is left out; the
//  run's tip has its addresses.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::PaintBounds (IDxuiTextRenderer & text, const IDxuiTheme & theme, bool isWrite) const
{
    DxuiFontHandle  font     = theme.MonospaceFont();
    float           size     = m_scaler.ToPxf (font.sizeDip);
    float           pad      = m_scaler.ToPxf ((float) kNamePadDip);
    float           width    = m_scaler.ToPxf ((float) kBoundsDip) - pad;
    float           x        = isWrite ? GetStripX (true) + m_scaler.ToPxf ((float) kStripDip) + pad : (float) m_boundsDip.left;
    float           top      = GetPageY (0);
    float           bottom   = GetPageY ((int) DiagnosticsMemoryMap::kPageCount);
    float           line     = size * 1.3f;
    float           last     = -FLT_MAX;
    DxuiTextHAlign  align    = isWrite ? DxuiTextHAlign::Left : DxuiTextHAlign::Right;
    auto            draw     = [&] (const std::wstring & label, float at)
    {
        HRESULT  hr = S_OK;

        if (at < last)
        {
            return;
        }

        hr = text.DrawString (label.c_str(), x, at, width, line, theme.ForegroundMuted(), size, font.face,
                              align, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        last = at + line;
    };



    for (int page = 0; page < (int) DiagnosticsMemoryMap::kPageCount; page++)
    {
        if (page == 0 || GetSourceOf (page, isWrite) != GetSourceOf (page - 1, isWrite))
        {
            draw (std::format (L"${:04X}", page * 0x100), std::clamp (GetPageY (page) - line / 2.0f, top, bottom - line));
        }
    }

    draw (L"$FFFF", bottom - line);
}
