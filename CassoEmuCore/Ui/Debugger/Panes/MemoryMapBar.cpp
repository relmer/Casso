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
//  The read strip, the write strip, and the key, in as many rows as it wraps
//  to at this width.
//
////////////////////////////////////////////////////////////////////////////////

int MemoryMapBar::GetPreferredHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    constexpr int                                       kStrips = 2;
    std::vector<std::pair<MemorySource, POINT>>         key     = LayOutKey ((float) widthPx, scaler);
    int                                                 rows    = key.empty() ? 1 : (int) key.back().second.y + 1;



    return scaler.ToPx ((kStripDip + kGapDip) * kStrips + kKeyDip * rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapBar::LayOutKey
//
//  Each entry as wide as its swatch and name, the names measured by the
//  monospace advance, and a new row where the next would pass the right edge.
//  An entry wider than the whole bar still gets a row of its own.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<MemorySource, POINT>> MemoryMapBar::LayOutKey (float widthPx, const DxuiDpiScaler & scaler) const
{
    static constexpr float                        kAdvancePerDip = 0.6f;
    float                                         indent         = scaler.ToPxf ((float) kKeyIndentDip);
    float                                         gap            = scaler.ToPxf ((float) kKeyGapDip);
    float                                         advance        = scaler.ToPxf (m_fontDip) * kAdvancePerDip;
    float                                         x              = 0.0f;
    LONG                                          row            = 0;
    std::vector<std::pair<MemorySource, POINT>>   key;



    for (int index = (int) MemorySource::Main; index < (int) MemorySource::Count; index++)
    {
        MemorySource  source = (MemorySource) index;
        float         width  = indent + (float) wcslen (GetSourceName (source)) * advance;
        bool          used   = std::any_of (m_map.pages.begin(), m_map.pages.end(), [source] (const DiagnosticsMemoryMap::Page & page)
        {
            return page.read == source || page.write == source;
        });

        if (!used)
        {
            continue;
        }

        if (x > 0.0f && x + width > widthPx)
        {
            x = 0.0f;
            row++;
        }

        key.push_back ({ source, POINT { (LONG) x, row } });
        x += width + gap;
    }

    return key;
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

    //  The key's rows are counted at this size before the next layout.
    m_fontDip = font.sizeDip;

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
//  Only the sources the map uses, so the key names what is on screen, each
//  entry as wide as its name and the key wrapped to the bar's width.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMapBar::PaintKey (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float y) const
{
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           key    = m_scaler.ToPxf ((float) kKeyDip);
    float           swatch = m_scaler.ToPxf ((float) kSwatchDip);
    float           indent = m_scaler.ToPxf ((float) kKeyIndentDip);
    float           left   = (float) m_boundsDip.left;
    float           width  = (float) (m_boundsDip.right - m_boundsDip.left);
    HRESULT         hr     = S_OK;



    for (const auto & [source, at] : LayOutKey (width, m_scaler))
    {
        float  x   = left + (float) at.x;
        float  top = y + key * (float) at.y;

        painter.FillRect (x, top + (key - swatch) / 2, swatch, swatch, GetSourceColor (source));

        hr = text.DrawString (GetSourceName (source), x + indent, top, width - (float) at.x - indent, key,
                              theme.ForegroundMuted(), m_scaler.ToPxf (font.sizeDip), font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
