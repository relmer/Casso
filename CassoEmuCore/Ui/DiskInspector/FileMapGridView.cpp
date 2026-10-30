#include "Pch.h"

#include "Ui/DiskInspector/FileMapGridView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/ZoomStops.h"





static constexpr float     s_kLabelDip      = 34.0f;
static constexpr float     s_kHeaderDip     = 16.0f;
static constexpr float     s_kAxisDip       = 16.0f;
static constexpr float     s_kNoteDip       = 34.0f;
//  Rows no shorter than a label's text, so every cell has room for its
//  number or role label.
static constexpr float     s_kMinRowDip     = InspectorView::kSmallDip;
static constexpr float     s_kMaxRowDip     = 16.0f;
static constexpr float     s_kLetterMinDip  = 12.0f;
static constexpr float     s_kWordPadDip    = 2.0f;
static constexpr float     s_kSmallerWordScale = 0.85f;
static constexpr float     s_kOutlineDip    = 2.0f;
static constexpr float     s_kDiffBandDip   = 3.0f;
static constexpr uint32_t  s_kMarkAlpha     = 0xE0000000u;
static constexpr float     s_kWheelStepDip  = 48.0f;
static constexpr float     s_kLineEm        = 1.4f;
static constexpr Byte      s_kHighBit       = 0x80;
static constexpr int       s_kBytesPerRow   = 16;
static constexpr float     s_kMinSquareDip  = 2.0f;
static constexpr float     s_kMinBitDip     = 3.0f;
static constexpr float     s_kBytePadDip    = 2.0f;
static constexpr uint32_t  s_kLowByteAlpha  = 0x60000000u;
static constexpr uint32_t  s_kZeroBitAlpha  = 0x30000000u;
static constexpr float     s_kEstDigitEm    = 0.6f;
static constexpr double    s_kScanRatio     = 1.02;
static constexpr double    s_kPastStepRatio = 1.25;
static constexpr double    s_kCoarseRatio   = 2.0;
static constexpr double    s_kMinStepRatio  = 1.2;





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::FileMapGridView
//
//  The scrollbars of a zoomed grid set its pan.
//
////////////////////////////////////////////////////////////////////////////////

FileMapGridView::FileMapGridView (InspectorViewContext & context) : InspectorView (context)
{
    m_vBar.SetOnScroll ([this] (int, int pos) { m_panY = static_cast<float> (pos); ClampPan(); });
    m_hBar.SetOnScroll ([this] (int, int pos) { m_panX = static_cast<float> (pos); ClampPan(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_map != nullptr)
    {
        PaintMap (painter, text, theme);
    }
    else if (m_context.hasDisk)
    {
        text.DrawString (L"The map is built once every track has been analyzed.", static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top),
                         GetWidth(), m_scaler.ToPxf (s_kNoteDip), theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintMap
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintMap (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT                      hr        = S_OK;
    Grid                         g         = MakeGrid();
    float                        textPx    = m_scaler.ToPxf (kSmallDip);
    int                          cell      = 0;
    int                          number    = 0;
    bool                         isFile    = false;
    RECT                         prior     = {};
    RECT                         area      = {};
    RECT                         clip      = {};
    RECT                         cellPx    = {};
    bool                         hasPrior  = false;
    bool                         isClipped = false;
    vector<int>                  numbers;
    std::optional<SectorSource>  source;



    if (m_map->notMapped != NotMappedReason::None)
    {
        text.DrawString (FileMapText::FormatNotMapped (*m_map).c_str(), static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top), GetWidth(),
                         m_scaler.ToPxf (s_kNoteDip), theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                         DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
    }

    //  The selected file's sectors numbered in file order.
    numbers.assign (m_map->cells.size(), 0);

    for (int k = 0; m_selectedFile >= 0 && m_selectedFile < static_cast<int> (m_map->files.size()) && k < static_cast<int> (m_map->files[m_selectedFile].sectors.size()); k++)
    {
        int  at = m_map->files[m_selectedFile].sectors[k].cell;

        number += (at >= 0) ? 1 : 0;

        if (at >= 0 && numbers[at] == 0)
        {
            numbers[at] = number;
            isFile      = true;
        }
    }

    MakeRoleLabels (text, g, textPx);

    //  Zoomed, the cells read their bytes from the analysis as it is now.
    if (IsZoomed() && m_context.analysis != nullptr)
    {
        source.emplace (*m_context.analysis);
    }

    //  The cells clipped to their area, so a zoomed grid pans under the
    //  pinned labels; cells wholly outside it are skipped.
    hasPrior  = painter.GetClipRect (prior);
    area      = { static_cast<LONG> (g.cellsLeft), static_cast<LONG> (g.cellsTop), m_boundsDip.right, m_boundsDip.bottom };
    clip      = area;
    isClipped = !hasPrior || IntersectRect (&clip, &area, &prior) != FALSE;

    painter.SetClipRect (&clip);
    hr = text.PushClipRect (static_cast<float> (clip.left), static_cast<float> (clip.top), static_cast<float> (clip.right - clip.left), static_cast<float> (clip.bottom - clip.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (cell = 0; isClipped && cell < static_cast<int> (m_map->cells.size()); cell++)
    {
        if (GetCellRect (cell, cellPx) && cellPx.right > clip.left && cellPx.left < clip.right && cellPx.bottom > clip.top && cellPx.top < clip.bottom)
        {
            PaintCell (painter, text, theme, g, cell, numbers[cell], isFile && numbers[cell] == 0, source ? &*source : nullptr);
        }
    }

    PaintMarquee (painter, g, numbers);

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
    painter.SetClipRect (hasPrior ? &prior : nullptr);

    PaintLabels (text, theme, g, textPx);

    //  Zoomed, scrollbars along the right and bottom edges where the cells
    //  run past them.
    ConfigureBars (g);

    if (m_vBar.IsVisible())
    {
        m_vBar.Paint (painter, theme.Foreground());
    }

    if (m_hBar.IsVisible())
    {
        m_hBar.Paint (painter, theme.Foreground());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::ConfigureBars
//
//  A bar where the cells run past their area, its range the cells' extent
//  and its page the area's; a bar of no range is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::ConfigureBars (const Grid & g)
{
    static constexpr int  kMinThumbDip = 16;



    int             bar      = m_scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip);
    int             contentW = static_cast<int> (m_map->cellsPerTrack * g.cellW);
    int             contentH = static_cast<int> (m_map->tracks * g.rowH);
    int             viewW    = m_boundsDip.right  - static_cast<int> (g.cellsLeft);
    int             viewH    = m_boundsDip.bottom - static_cast<int> (g.cellsTop);
    bool            isV      = IsZoomed() && contentH > viewH;
    bool            isH      = IsZoomed() && contentW > viewW;
    DxuiScrollInfo  info;



    m_vBar.Configure (DxuiScrollbar::Orientation::Vertical,   bar, m_scaler.ToPx (kMinThumbDip), m_scaler.ToPx (static_cast<int> (s_kWheelStepDip)));
    m_hBar.Configure (DxuiScrollbar::Orientation::Horizontal, bar, m_scaler.ToPx (kMinThumbDip), m_scaler.ToPx (static_cast<int> (s_kWheelStepDip)));
    m_vBar.SetTrack ({ m_boundsDip.right - bar, static_cast<LONG> (g.cellsTop), m_boundsDip.right, m_boundsDip.bottom - (isH ? bar : 0) });
    m_hBar.SetTrack ({ static_cast<LONG> (g.cellsLeft), m_boundsDip.bottom - bar, m_boundsDip.right - (isV ? bar : 0), m_boundsDip.bottom });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = isV ? contentH : 0;
    info.nPage = static_cast<UINT> (viewH - (isH ? bar : 0));
    info.nPos  = static_cast<int> (m_panY);
    m_vBar.SetScrollInfo (info);

    info.nMax  = isH ? contentW : 0;
    info.nPage = static_cast<UINT> (viewW - (isV ? bar : 0));
    info.nPos  = static_cast<int> (m_panX);
    m_hBar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintLabels
//
//  The axis title, "Track", the column headers along the top and the track
//  numbers down the left, pinned there while a zoomed grid pans: the
//  headers follow the columns across and the numbers follow the rows down,
//  each centered on the part of its column or row in view.
//  Rows too thin for every number are labeled every few tracks, each label a
//  line of text tall, centered on its row, so a row thinner than the text
//  does not clip it.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, float textPx) const
{
    HRESULT  hr      = S_OK;
    float    header  = m_scaler.ToPxf (s_kHeaderDip);
    float    left    = static_cast<float> (m_boundsDip.left);
    float    labelH  = textPx * 1.4f;
    float    right   = static_cast<float> (m_boundsDip.right);
    float    bottom  = static_cast<float> (m_boundsDip.bottom);



    text.DrawString (FileMapText::FormatColumnAxis (*m_map).c_str(), g.cellsLeft, g.cellsTop - m_scaler.ToPxf (s_kHeaderDip + s_kAxisDip), right - g.cellsLeft,
                     m_scaler.ToPxf (s_kAxisDip), theme.Foreground(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                     DxuiFontWeight::SemiBold, false);
    text.DrawString (L"Track", left, g.cellsTop - header, g.labelW - m_scaler.ToPxf (4.0f), header, theme.Foreground(),
                     textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

    hr = text.PushClipRect (g.cellsLeft, g.cellsTop - header, right - g.cellsLeft, header);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int c = 0; c < m_map->cellsPerTrack; c++)
    {
        std::wstring  label = (m_map->cellsPerTrack == 8) ? std::to_wstring (c) : std::format (L"{:X}", c);
        float         x0    = std::max (g.left + g.labelW + c * g.cellW, g.cellsLeft);
        float         x1    = std::min (g.left + g.labelW + (c + 1) * g.cellW, right);

        if (x1 > x0)
        {
            text.DrawString (label.c_str(), x0, g.cellsTop - header, x1 - x0, header,
                             theme.ForegroundMuted(), textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.PushClipRect (left, g.cellsTop, g.labelW, bottom - g.cellsTop);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int t = 0; t < m_map->tracks; t += std::max (1, static_cast<int> (std::ceil (textPx / g.rowH))))
    {
        float  y0 = std::max (g.top + t * g.rowH, g.cellsTop);
        float  y1 = std::min (g.top + (t + 1) * g.rowH, bottom);

        if (y1 > y0)
        {
            text.DrawString (std::to_wstring (t).c_str(), left, (y0 + y1) / 2 - labelH / 2, g.labelW - m_scaler.ToPxf (4.0f), labelH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintBytes
//
//  A cell's bytes under its label's band, laid out by GetByteLayout, in its
//  ink: squares where a byte is too small for text, $00 left clear, a byte
//  of $80 or more solid and the rest faint; the bytes in hex once they fit;
//  with room for eight bits a byte, its bits, a 1 solid and a 0 faint, under
//  the hex when that fits too; and with room for a digit in each bit, each
//  bit as a 1 or a 0. A block's two halves follow one another. Returns
//  whether the cell had room for the bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::PaintBytes (IDxuiPainter & painter, IDxuiTextRenderer & text, const SectorSource & source, int cell, uint32_t ink) const
{
    RECT        cellPx  = {};
    int         halves  = (m_map->cellsPerTrack == SectorSource::kBlocksPerTrack) ? 2 : 1;
    float       monoPx  = m_scaler.ToPxf (kSmallDip);
    float       pad     = m_scaler.ToPxf (s_kBytePadDip);
    float       digitH  = 0;
    float       x0      = 0;
    float       y0      = 0;
    float       bitsTop = 0;
    float       bitsEnd = 0;
    float       inset   = m_scaler.ToPxf (s_kBytePadDip);
    float       bitW    = 0;
    bool        isCell  = GetCellRect (cell, cellPx);
    ByteLayout  layout;
    uint32_t    faint   = (ink & 0x00FFFFFFu) | s_kLowByteAlpha;
    uint32_t    zeroBit = (ink & 0x00FFFFFFu) | s_kZeroBitAlpha;



    text.MeasureString (L"FF", monoPx, DxuiTheme::kMonoFace, m_hexW, m_hexH);
    text.MeasureString (L"0",  monoPx, DxuiTheme::kMonoFace, m_digitW, digitH);

    layout = GetByteLayout (static_cast<float> (cellPx.right - cellPx.left), static_cast<float> (cellPx.bottom - cellPx.top));
    x0     = cellPx.left + pad;
    y0     = cellPx.top + monoPx * s_kLineEm;
    bitW   = (layout.slotW - 2 * inset) / 8;

    for (int half = 0; isCell && layout.isRoom && half < halves; half++)
    {
        const SectorSource::Sector &  sector = source.GetPhysical (m_map->GetTrack (cell), m_map->GetPhysical (cell, half));

        for (int i = 0; sector.bytes != nullptr && i < SectorSource::kBytes; i++)
        {
            Byte   b  = sector.bytes[i];
            int    at = half * SectorSource::kBytes + i;
            float  x  = x0 + (at % layout.perRow) * layout.slotW;
            float  y  = y0 + (at / layout.perRow) * layout.slotH;

            if (layout.isHex)
            {
                text.DrawString (std::format (L"{:02X}", b).c_str(), x, y, layout.slotW, layout.isBits ? m_hexH : layout.slotH, (b == 0) ? faint : ink, monoPx,
                                 DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            }
            else if (!layout.isBits && b != 0)
            {
                painter.FillRect (x, y, layout.slotW - 1.0f, layout.slotH - 1.0f, (b >= s_kHighBit) ? ink : faint);
            }

            bitsTop = layout.isHex ? y + m_hexH : y + inset;

            //  At the bit level, each byte in a box of its own around its hex
            //  and its eight bits; with room, each bit's 1 or 0 under its bar.
            if (layout.isBits)
            {
                painter.OutlineRect (x + 0.5f, y + 0.5f, layout.slotW - 2.0f, layout.slotH - 2.0f, 1.0f, faint);
            }

            bitsEnd = y + layout.slotH - inset - (layout.isDigits ? m_hexH : 0.0f);

            for (int k = 0; layout.isBits && k < 8; k++)
            {
                bool  isOne = ((b >> (7 - k)) & 1) != 0;

                painter.FillRect (x + inset + k * bitW, bitsTop, bitW - 1.0f, bitsEnd - bitsTop - 1.0f, isOne ? ink : zeroBit);

                if (layout.isDigits)
                {
                    text.DrawString (isOne ? L"1" : L"0", x + inset + k * bitW, bitsEnd, bitW, m_hexH, isOne ? ink : faint, monoPx, DxuiTheme::kMonoFace,
                                     DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                }
            }
        }
    }

    return isCell && layout.isRoom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetByteLayout
//
//  16, 32 or 64 bytes to a row, the count with the most room for a byte's
//  hex in a cell of a size; then whether there is room for squares, for hex,
//  for eight bits a byte, and for a digit in each bit. Before the first paint
//  measures the font, its sizes are estimated.
//
////////////////////////////////////////////////////////////////////////////////

FileMapGridView::ByteLayout FileMapGridView::GetByteLayout (float cellW, float cellH) const
{
    ByteLayout  layout;
    int         halves = (m_map != nullptr && m_map->cellsPerTrack == SectorSource::kBlocksPerTrack) ? 2 : 1;
    int         bytes  = halves * SectorSource::kBytes;
    float       monoPx = m_scaler.ToPxf (kSmallDip);
    float       pad    = m_scaler.ToPxf (s_kBytePadDip);
    float       hexW   = (m_hexW > 0) ? m_hexW : monoPx * s_kEstDigitEm * 2;
    float       hexH   = (m_hexH > 0) ? m_hexH : monoPx * s_kLineEm;
    float       digitW = (m_digitW > 0) ? m_digitW : monoPx * s_kEstDigitEm;
    float       areaW  = cellW - 2 * pad;
    float       areaH  = cellH - monoPx * s_kLineEm - pad;
    float       best   = 0;
    float       fit    = 0;
    float       bitW   = 0;



    for (int candidate = s_kBytesPerRow; candidate <= 4 * s_kBytesPerRow; candidate *= 2)
    {
        fit = std::min ((areaW / candidate) / (hexW + pad), (areaH / (bytes / candidate)) / hexH);

        if (fit > best)
        {
            best          = fit;
            layout.perRow = candidate;
        }
    }

    layout.slotW    = areaW / layout.perRow;
    layout.slotH    = areaH / (bytes / layout.perRow);
    bitW            = (layout.slotW - 2 * pad) / 8;
    layout.isRoom   = layout.slotW >= m_scaler.ToPxf (s_kMinSquareDip) && layout.slotH >= m_scaler.ToPxf (s_kMinSquareDip);
    layout.isHex    = layout.isRoom && layout.slotW >= hexW + pad && layout.slotH >= hexH;
    layout.isBits   = layout.isRoom && layout.slotW >= 8 * m_scaler.ToPxf (s_kMinBitDip) && layout.slotH >= m_scaler.ToPxf (s_kMinBitDip) + (layout.isHex ? hexH : 0.0f);
    layout.isDigits = layout.isBits && layout.isHex && bitW >= digitW + pad && layout.slotH >= 3 * hexH;

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetZoomStops
//
//  Each zoom at which more of a cell's bytes is drawn, found by trying
//  zooms a small ratio apart, the ones before them by halves, and a step past
//  the labeled bits.
//
////////////////////////////////////////////////////////////////////////////////

vector<double> FileMapGridView::GetZoomStops() const
{
    Grid            g       = MakeGrid();
    float           baseW   = IsZoomed() ? m_baseCellW : g.cellW;
    float           baseH   = IsZoomed() ? m_baseRowH  : g.rowH;
    int             level   = 0;
    int             deepest = 0;
    vector<double>  details;
    ByteLayout      layout;



    for (double zoom = 1.0; zoom <= kMaxZoom && deepest < 4; zoom *= s_kScanRatio)
    {
        layout = GetByteLayout (static_cast<float> (baseW * zoom), static_cast<float> (baseH * zoom));
        level  = layout.isDigits ? 4 : layout.isBits ? 3 : layout.isHex ? 2 : layout.isRoom ? 1 : 0;

        if (level > deepest)
        {
            details.push_back (zoom);
            deepest = level;
        }
    }

    if (!details.empty())
    {
        details.push_back (std::min (details.back() * s_kPastStepRatio, kMaxZoom));
    }

    return ZoomStops::Build (1.0, s_kCoarseRatio, details, s_kMinStepRatio);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetSteppedZoom
//
////////////////////////////////////////////////////////////////////////////////

double FileMapGridView::GetSteppedZoom (int steps) const
{
    return ZoomStops::Step (GetZoomStops(), m_zoom, steps);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::MakeRoleLabels
//
//  Each role's word where it fits a cell, in the small size or a step
//  smaller, else its letter where that fits, else nothing; measured once a
//  frame rather than once a cell.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::MakeRoleLabels (IDxuiTextRenderer & text, const Grid & g, float textPx)
{
    std::wstring  word;
    float         w        = 0;
    float         h        = 0;
    float         room     = g.cellW - m_scaler.ToPxf (s_kWordPadDip);
    bool          isPlaced = false;



    for (size_t role = 0; role < m_roleLabels.size(); role++)
    {
        word                = FileMapText::GetRoleWord (m_map->fileSystem, static_cast<SectorRole> (role));
        m_roleLabels[role]  = (g.cellW >= m_scaler.ToPxf (s_kLetterMinDip)) ? std::wstring (1, FileMapText::GetRoleLetter (static_cast<SectorRole> (role))) : std::wstring();
        m_roleSizesPx[role] = textPx;
        isPlaced            = false;

        for (float size : { textPx, textPx * s_kSmallerWordScale })
        {
            w = 0;
            text.MeasureString (word.c_str(), size, DxuiTheme::kBodyFace, w, h);

            if (!isPlaced && w <= room)
            {
                m_roleLabels[role]  = word;
                m_roleSizesPx[role] = size;
                isPlaced            = true;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintCell
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintCell (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, int cell, int number, bool isDimmed,
                                 const SectorSource * source)
{
    const MapCell &  at     = m_map->cells[cell];
    float            x      = g.left + g.labelW + m_map->GetColumn (cell) * g.cellW;
    float            y      = g.top + m_map->GetTrack (cell) * g.rowH;
    float            w      = g.cellW - 1.0f;
    float            h      = g.rowH - 1.0f;
    uint32_t         color  = GetCellColor (cell);
    uint32_t         ink    = (DiskInspectorPalette::GetTextColorOn (color) & 0x00FFFFFFu) | s_kMarkAlpha;
    float            textPx = m_scaler.ToPxf (kSmallDip);
    float            labelH = h;
    std::wstring     label;



    //  Every sector but the selected file's is dimmed, so the file's are the
    //  brightest in the grid wherever they lie.
    color = isDimmed ? InspectorMarquee::Dim (m_context.palette, color) : color;
    ink   = isDimmed ? InspectorMarquee::Dim (m_context.palette, ink)   : ink;

    painter.FillRect (x, y, w, h, color);

    if (at.result == SectorResult::Bad)
    {
        painter.DrawLine (x + 1, y + 1, x + w - 1, y + h - 1, 1.5f, ink);
        painter.DrawLine (x + w - 1, y + 1, x + 1, y + h - 1, 1.5f, ink);
    }
    else if (at.result == SectorResult::Missing)
    {
        painter.OutlineRect (x + 1, y + 1, w - 2, h - 2, 1.0f, theme.ForegroundMuted());
    }
    else if (at.result == SectorResult::NotChecked)
    {
        painter.FillCircle (x + w - 3.0f, y + 3.0f, 1.5f, ink);
    }

    //  Bytes under a band at the top for the label, when the cell has room.
    if (source != nullptr && PaintBytes (painter, text, *source, cell, ink))
    {
        labelH = textPx * s_kLineEm;
    }

    label = (number > 0) ? std::to_wstring (number)
          : (static_cast<size_t> (at.role) < m_roleLabels.size()) ? m_roleLabels[static_cast<size_t> (at.role)] : std::wstring();

    if (!label.empty() && at.result != SectorResult::Bad && g.rowH >= textPx)
    {
        text.DrawString (label.c_str(), x, y, w, labelH, number > 0 ? theme.Foreground() : ink, number > 0 ? textPx : m_roleSizesPx[static_cast<size_t> (at.role)],
                         number > 0 ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Center, DxuiTextVAlign::Center, number > 0 ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal, false);
    }

    //  A cell with pending edits (FR-097): a band along its left side.
    if (IsEditedCell (cell))
    {
        painter.FillRect (x, y, m_scaler.ToPxf (s_kDiffBandDip), h, m_context.palette.colors.pendingEdit);
    }

    //  A cell with find hits (FR-056): a band along its top.
    if (IsHitCell (cell))
    {
        painter.FillRect (x, y, w, m_scaler.ToPxf (s_kDiffBandDip + 1.0f), DiskInspectorPalette::GetTextColorOn (m_context.palette.colors.findHit));
        painter.FillRect (x, y, w, m_scaler.ToPxf (s_kDiffBandDip), m_context.palette.colors.findHit);
    }

    //  While comparing, a cell of the selected file pair that differs (FR-120).
    if (m_context.diffCells != nullptr && std::find (m_context.diffCells->begin(), m_context.diffCells->end(), cell) != m_context.diffCells->end())
    {
        painter.FillRect    (x, y + h - m_scaler.ToPxf (s_kDiffBandDip), w, m_scaler.ToPxf (s_kDiffBandDip), m_context.palette.colors.difference);
        painter.OutlineRect (x + 1, y + 1, w - 2, h - 2, m_scaler.ToPxf (s_kOutlineDip), m_context.palette.colors.difference);
    }

    //  The selected cell lit by the shimmer (FR-038), whose band crosses the
    //  cell's part in view.
    if (cell == m_selectedCell)
    {
        RECT  box = { static_cast<LONG> (std::max (x, g.cellsLeft)), static_cast<LONG> (std::max (y, g.cellsTop)),
                      static_cast<LONG> (std::min (x + w, static_cast<float> (m_boundsDip.right))), static_cast<LONG> (std::min (y + h, static_cast<float> (m_boundsDip.bottom))) };

        InspectorMarquee::PaintShimmer (painter, x, y, w, h, InspectorMarquee::GetClockMs(), box);
    }

    //  The cell under the pointer lit faintly and outlined, so the tooltip's
    //  subject is plain (FR-148).
    if (cell == HitTest (m_context.hoverPx))
    {
        painter.FillRect    (x, y, w, h, InspectorMarquee::GetHoverFill());
        painter.OutlineRect (x, y, w, h, 1.0f, InspectorMarquee::GetHoverLine());
    }

    if (cell == m_selectedCell)
    {
        painter.OutlineRect (x - 1, y - 1, w + 2, h + 2, m_scaler.ToPxf (s_kOutlineDip), theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintMarquee
//
//  A marching marquee around the selected file's sectors: an edge of a
//  numbered cell is drawn where its neighbor is not numbered, each edge in
//  the direction that runs clockwise around the file's sectors, its dashes
//  counted from the grid's origin so that edges in a line join up.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintMarquee (IDxuiPainter & painter, const Grid & g, const vector<int> & numbers) const
{
    InspectorMarquee  marquee (m_context.palette, m_scaler.ToPxf (InspectorMarquee::kDashDip), InspectorMarquee::kThicknessPx,
                               static_cast<int64_t> (GetTickCount64()));
    int               tracks  = m_map->tracks;
    int               columns = m_map->cellsPerTrack;
    float             x0      = 0;
    float             y0      = 0;
    float             x1      = 0;
    float             y1      = 0;
    auto              isMarked = [&] (int t, int c) { return t >= 0 && t < tracks && c >= 0 && c < columns && t * columns + c < static_cast<int> (numbers.size()) && numbers[t * columns + c] > 0; };



    for (int t = 0; t < tracks; t++)
    {
        for (int c = 0; c < columns; c++)
        {
            if (!isMarked (t, c))
            {
                continue;
            }

            //  On pixel centers, so each one-pixel line covers one pixel.
            x0 = std::floor (g.left + g.labelW + c * g.cellW) + 0.5f;
            y0 = std::floor (g.top + t * g.rowH) + 0.5f;
            x1 = std::floor (g.left + g.labelW + (c + 1) * g.cellW) - 0.5f;
            y1 = std::floor (g.top + (t + 1) * g.rowH) - 0.5f;

            if (!isMarked (t - 1, c))
            {
                marquee.DrawLine (painter, x0, y0, x1, y0, x0);
            }

            if (!isMarked (t, c + 1))
            {
                marquee.DrawLine (painter, x1, y0, x1, y1, y0);
            }

            if (!isMarked (t + 1, c))
            {
                marquee.DrawLine (painter, x1, y1, x0, y1, -x1);
            }

            if (!isMarked (t, c - 1))
            {
                marquee.DrawLine (painter, x0, y1, x0, y0, -y1);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT    p         = ev.positionDip;
    int      cell      = HitTest (p);
    bool     isInside  = PtInRect (&m_boundsDip, p) != FALSE;
    bool     isHandled = false;
    bool     isAcross  = false;
    int64_t  now       = static_cast<int64_t> (GetTickCount64());



    //  A scrollbar takes the press on it and the drag that follows.
    if (m_vBar.IsDragging() || m_hBar.IsDragging())
    {
        isHandled = (ev.kind == DxuiMouseEventKind::Move) ? (m_vBar.OnMouseMove (p.x, p.y) || m_hBar.OnMouseMove (p.x, p.y))
                  : (ev.kind == DxuiMouseEventKind::Up)   ? (m_vBar.OnMouseUp() || m_hBar.OnMouseUp())
                  : true;
        return isHandled;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && m_vBar.IsVisible() && m_vBar.HitTest (p.x, p.y))
    {
        return m_vBar.OnMouseDown (p.x, p.y);
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && m_hBar.IsVisible() && m_hBar.HitTest (p.x, p.y))
    {
        return m_hBar.OnMouseDown (p.x, p.y);
    }

    switch (ev.kind)
    {
        //  Zoomed, Ctrl and the wheel pan the grid down and up, and Shift and
        //  the wheel, or the wheel turned sideways, pan it across. The window
        //  zooms the grid with the wheel alone.
        case DxuiMouseEventKind::Wheel:
            if (IsZoomed() && isInside && (ev.ctrl || ev.shift || ev.wheelHorizontal))
            {
                isAcross  = ev.shift || ev.wheelHorizontal;
                ShiftPan (isAcross ? static_cast<int> (-ev.wheelDelta * m_scaler.ToPxf (s_kWheelStepDip)) : 0,
                          isAcross ? 0 : static_cast<int> (-ev.wheelDelta * m_scaler.ToPxf (s_kWheelStepDip)));
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left && cell >= 0)
            {
                m_isPressed = true;
                m_isDragged = false;
                m_pressAt   = p;
                m_lastAt    = p;
                isHandled   = true;
            }

            break;

        //  Zoomed, a drag pans the grid.
        case DxuiMouseEventKind::Move:
            if (m_isPressed && !m_isDragged && IsZoomed() &&
                (std::abs (p.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG) / 2 || std::abs (p.y - m_pressAt.y) > GetSystemMetrics (SM_CYDRAG) / 2))
            {
                m_isDragged = true;
            }

            if (m_isDragged)
            {
                ShiftPan (m_lastAt.x - p.x, m_lastAt.y - p.y);
                m_lastAt = p;
            }

            isHandled = m_isPressed;
            break;

        //  A click chooses a cell; a second click soon after it zooms the grid
        //  a stop in about the point, or with Shift a stop out.
        case DxuiMouseEventKind::Up:
            isHandled = m_isPressed;

            if (m_isPressed && !m_isDragged && now - m_lastClickMs <= static_cast<int64_t> (GetDoubleClickTime()))
            {
                m_lastClickMs = 0;

                if (m_onZoom)
                {
                    m_onZoom (ev.shift ? -1 : 1, p);
                }
            }
            else if (m_isPressed && !m_isDragged && cell >= 0)
            {
                m_lastClickMs = now;

                if (m_choose && m_map != nullptr && m_map->notMapped == NotMappedReason::None)
                {
                    m_choose (cell);
                }
            }

            m_isPressed = false;
            m_isDragged = false;
            break;

        default:
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::ZoomAbout
//
//  The point of the grid under the anchor stays under it. A cell's size at
//  a zoom of 1, as the zoom first moves past 1, is the base for every other
//  zoom.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::ZoomAbout (double factor, POINT anchorPx)
{
    Grid    g        = MakeGrid();
    double  next     = std::clamp (m_zoom * factor, 1.0, kMaxZoom);
    float   u        = 0;
    float   v        = 0;
    bool    isZoomed = false;



    if (!IsZoomed())
    {
        m_baseCellW = g.cellW;
        m_baseRowH  = g.rowH;
    }

    u         = (anchorPx.x - (g.left + g.labelW)) / g.cellW;
    v         = (anchorPx.y - g.top) / g.rowH;
    isZoomed  = IsZoomed();
    m_zoom    = next;

    if (IsZoomed())
    {
        m_panX = 0;
        m_panY = 0;
        g      = MakeGrid();
        m_panX = g.cellsLeft + u * g.cellW - anchorPx.x;
        m_panY = g.cellsTop  + v * g.rowH  - anchorPx.y;
    }

    //  Entering the zoom, the grid's bounds are about to change, and
    //  ShiftPan clamps the pan once they have.
    if (isZoomed || !IsZoomed())
    {
        ClampPan();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::ResetZoom
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::ResetZoom()
{
    m_zoom = 1.0;
    m_panX = 0;
    m_panY = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::ShiftPan
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::ShiftPan (int dxPx, int dyPx)
{
    m_panX += static_cast<float> (dxPx);
    m_panY += static_cast<float> (dyPx);

    ClampPan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::ClampPan
//
//  The cells' edges no farther in than their area's, less a scrollbar along
//  the other edge where there is one, so the grid never pans past its last
//  row or column; at a zoom of 1 there is no pan.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::ClampPan()
{
    Grid   g       = MakeGrid();
    int    columns = (m_map != nullptr) ? m_map->cellsPerTrack : 16;
    int    tracks  = (m_map != nullptr) ? m_map->tracks : 35;
    float  bar     = m_scaler.ToPxf (static_cast<float> (DxuiScrollPanel::kScrollbarWidthDip));
    float  viewW   = m_boundsDip.right  - g.cellsLeft;
    float  viewH   = m_boundsDip.bottom - g.cellsTop;
    bool   isH     = columns * g.cellW > viewW;
    bool   isV     = tracks  * g.rowH  > viewH;
    float  maxX    = std::max (0.0f, columns * g.cellW - (viewW - (isV ? bar : 0.0f)));
    float  maxY    = std::max (0.0f, tracks  * g.rowH  - (viewH - (isH ? bar : 0.0f)));



    m_panX = IsZoomed() ? std::clamp (m_panX, 0.0f, maxX) : 0.0f;
    m_panY = IsZoomed() ? std::clamp (m_panY, 0.0f, maxY) : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetTooltip
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    int  cell = HitTest (pointPx);



    if (cell >= 0)
    {
        outText     = (m_map->notMapped == NotMappedReason::None) ? FileMapText::FormatTooltip (*m_map, cell)
                                                                  : std::format (L"Track {}, physical sector {}", m_map->GetTrack (cell), InspectorFormat::FormatSector (m_map->GetColumn (cell)));
        outText    += (m_map->notMapped != NotMappedReason::None && m_map->cells[cell].result != SectorResult::NotChecked)
                      ? L"\n" + FileMapText::FormatResultPhrase (m_map->cells[cell].result) : L"";
        outText    += IsHitCell (cell) ? L"\n" + m_context.findMarks->FormatHitLine (1) : L"";
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return cell >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetTooltipSwatches
//
//  The cell's color beside the line with its role, or with its result when
//  the disk is not mapped, and the find hit color beside the last line for
//  a cell with hits.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    int  cell = HitTest (pointPx);



    if (cell >= 0)
    {
        std::wstring  text;
        RECT          anchor = {};

        outSwatches = { { 1, IsDimmed (cell) ? InspectorMarquee::Dim (m_context.palette, GetCellColor (cell)) : GetCellColor (cell) } };

        if (IsHitCell (cell) && GetTooltip (pointPx, text, anchor))
        {
            outSwatches.push_back ({ static_cast<int> (std::count (text.begin(), text.end(), L'\n')), m_context.palette.colors.findHit });
        }
    }

    return cell >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::IsHitCell
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::IsHitCell (int cell) const
{
    return m_context.findMarks != nullptr && m_context.findMarks->IsCellMarked (cell);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::IsEditedCell
//
//  A cell's sector has pending edits on its whole track's record.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::IsEditedCell (int cell) const
{
    int  half = 0;
    int  qt   = 0;



    if (m_context.edits == nullptr || m_context.analysis == nullptr || m_map == nullptr || m_context.edits->IsEmpty())
    {
        return false;
    }

    qt = m_map->GetTrack (cell) * DiskImage::kQuarterTracksPerWholeTrack;

    return qt >= 0 && qt < DiskImage::kQuarterTrackCount && m_context.edits->HasChanges (m_context.analysis->entries[qt].slot, static_cast<Byte> (m_map->GetPhysical (cell, half)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::IsDimmed
//
//  A cell outside the selected file, while that file has sectors to mark.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::IsDimmed (int cell) const
{
    bool  isFile = m_map != nullptr && m_selectedFile >= 0 && m_selectedFile < static_cast<int> (m_map->files.size()) &&
                   std::any_of (m_map->files[m_selectedFile].sectors.begin(), m_map->files[m_selectedFile].sectors.end(), [] (const FilePlace & p) { return p.cell >= 0; });



    return isFile && std::find (m_map->cells[cell].owners.begin(), m_map->cells[cell].owners.end(), m_selectedFile) == m_map->cells[cell].owners.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetCellColor
//
//  A cell's role color, or for a missing sector the platter's color for an
//  empty quarter track.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FileMapGridView::GetCellColor (int cell) const
{
    return (m_map->cells[cell].result == SectorResult::Missing) ? m_context.palette.colors.nothingRecorded
                                                                : m_context.palette.GetRoleColor (static_cast<MapRole> (m_map->cells[cell].role));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetHeightFor
//
//  Rows as tall as the room allows, between a minimum and a maximum.
//
////////////////////////////////////////////////////////////////////////////////

int FileMapGridView::GetHeightFor (int widthPx) const
{
    float  header = m_scaler.ToPxf (s_kHeaderDip + s_kAxisDip) + ((m_map != nullptr && m_map->notMapped != NotMappedReason::None) ? m_scaler.ToPxf (s_kNoteDip) : 0.0f);
    float  rowH   = std::clamp (static_cast<float> (widthPx) / 40.0f, m_scaler.ToPxf (s_kMinRowDip), m_scaler.ToPxf (s_kMaxRowDip));



    return static_cast<int> (header + 35.0f * rowH + 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::GetCellRect (int cell, RECT & outRectPx) const
{
    Grid  g      = MakeGrid();
    bool  isCell = m_map != nullptr && cell >= 0 && cell < static_cast<int> (m_map->cells.size());



    if (isCell)
    {
        outRectPx = { static_cast<LONG> (g.left + g.labelW + m_map->GetColumn (cell) * g.cellW), static_cast<LONG> (g.top + m_map->GetTrack (cell) * g.rowH),
                      static_cast<LONG> (g.left + g.labelW + (m_map->GetColumn (cell) + 1) * g.cellW), static_cast<LONG> (g.top + (m_map->GetTrack (cell) + 1) * g.rowH) };
    }

    return isCell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::MakeGrid
//
////////////////////////////////////////////////////////////////////////////////

FileMapGridView::Grid FileMapGridView::MakeGrid() const
{
    Grid   g;
    float  header  = m_scaler.ToPxf (s_kHeaderDip + s_kAxisDip);
    float  note    = (m_map != nullptr && m_map->notMapped != NotMappedReason::None) ? m_scaler.ToPxf (s_kNoteDip) : 0.0f;
    int    columns = (m_map != nullptr) ? m_map->cellsPerTrack : 16;
    int    tracks  = (m_map != nullptr) ? m_map->tracks : 35;



    g.labelW    = m_scaler.ToPxf (s_kLabelDip);
    g.cellsLeft = static_cast<float> (m_boundsDip.left) + g.labelW;
    g.cellsTop  = static_cast<float> (m_boundsDip.top) + note + header;
    g.rowH      = IsZoomed() ? static_cast<float> (m_baseRowH * m_zoom)  : std::max (1.0f, (GetHeight() - note - header) / tracks);
    g.cellW     = IsZoomed() ? static_cast<float> (m_baseCellW * m_zoom) : std::max (1.0f, (GetWidth() - g.labelW) / columns);
    g.left      = static_cast<float> (m_boundsDip.left) - m_panX;
    g.top       = g.cellsTop - m_panY;

    return g;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::HitTest
//
////////////////////////////////////////////////////////////////////////////////

int FileMapGridView::HitTest (POINT pointPx) const
{
    Grid  g      = MakeGrid();
    int   column = 0;
    int   row    = 0;
    int   cell   = -1;



    if (m_map != nullptr && PtInRect (&m_boundsDip, pointPx))
    {
        column = static_cast<int> ((pointPx.x - g.left - g.labelW) / g.cellW);
        row    = static_cast<int> ((pointPx.y - g.top) / g.rowH);

        if (pointPx.x >= g.cellsLeft && pointPx.y >= g.cellsTop && pointPx.x >= g.left + g.labelW && pointPx.y >= g.top &&
            column < m_map->cellsPerTrack && row < m_map->tracks)
        {
            cell = row * m_map->cellsPerTrack + column;
        }
    }

    return cell;
}
