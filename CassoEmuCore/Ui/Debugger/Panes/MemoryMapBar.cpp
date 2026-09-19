#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryMapBar.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





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
//  The read strip, the write strip, and the key.
//
////////////////////////////////////////////////////////////////////////////////

int MemoryMapBar::GetPreferredHeightPx (const DxuiDpiScaler & scaler) const
{
    constexpr int  kStrips = 2;



    return scaler.ToPx ((kStripDip + kGapDip) * kStrips + kKeyDip);
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
    PaintKey   (painter, text, theme, top + (strip + gap) + (strip + gap));

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
        uint32_t      color  = (source == MemorySource::None) ? theme.Divider() : GetSourceColor (source);

        while (end < (int) DiagnosticsMemoryMap::kPageCount &&
               (isWrite ? m_map.pages[(size_t) end].write : m_map.pages[(size_t) end].read) == source)
        {
            end++;
        }

        painter.FillRect (GetPageX (first), y, GetPageX (end) - GetPageX (first), strip, color);
        first = end;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::PaintKey
//
//  Only the sources the map uses, so the key names what is on screen.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::PaintKey (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float y) const
{
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           key    = m_scaler.ToPxf ((float) kKeyDip);
    float           swatch = m_scaler.ToPxf ((float) kSwatchDip);
    float           item   = m_scaler.ToPxf ((float) kKeyItemDip);
    float           indent = m_scaler.ToPxf ((float) kKeyIndentDip);
    float           x      = (float) m_boundsDip.left;
    HRESULT         hr     = S_OK;



    for (int index = (int) MemorySource::Main; index < (int) MemorySource::Count; index++)
    {
        MemorySource  source = (MemorySource) index;
        bool          used   = std::any_of (m_map.pages.begin(), m_map.pages.end(), [source] (const DiagnosticsMemoryMap::Page & page)
        {
            return page.read == source || page.write == source;
        });

        if (!used || x + item > (float) m_boundsDip.right)
        {
            continue;
        }

        painter.FillRect (x, y + (key - swatch) / 2, swatch, swatch, GetSourceColor (source));

        hr = text.DrawString (GetSourceName (source), x + indent, y, item - indent, key,
                              theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += item;
    }
}
