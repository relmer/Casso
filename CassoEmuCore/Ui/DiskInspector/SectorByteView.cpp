#include "Pch.h"

#include "Ui/DiskInspector/SectorByteView.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr Byte   s_kHighBit         = 0x80;
static constexpr Byte   s_kFirstPrintable  = 0x20;
static constexpr Byte   s_kLastPrintable   = 0x7E;
static constexpr float  s_kOffsetCharsDip  = 30.0f;
static constexpr float  s_kByteDip         = 22.0f;
static constexpr float  s_kTextGapDip      = 12.0f;
static constexpr float  s_kCharDip         = 8.0f;
static constexpr float  s_kHeaderGapDip    = 20.0f;
static constexpr uint32_t  s_kSelectionAlpha = 0x50000000u;
static constexpr uint32_t  s_kDiffAlpha      = 0x50000000u;
static constexpr uint32_t  s_kHitAlpha       = 0x60000000u;
static constexpr float     s_kDiffLineDip    = 2.0f;
static constexpr float     s_kMinRowDip      = 11.0f;
static constexpr float     s_kMarkDip        = 5.0f;
static constexpr float     s_kCursorDip      = 1.5f;
static constexpr uint32_t  s_kPanelArgb      = 0xFF0E121Au;
static constexpr float     s_kPanelRadiusDip = 6.0f;
static constexpr float     s_kPanelPadDip    = 8.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *   track  = m_context.GetTrack();
    const AnalyzedSector *  sector = (track != nullptr) ? m_context.model->GetSector() : nullptr;
    float                   y      = static_cast<float> (m_boundsDip.top);
    std::wstring            empty;



    m_grid.isShown = false;

    if (m_context.hasDisk && m_context.analysis != nullptr)
    {
        if (sector != nullptr)
        {
            y = PaintHeader (text, theme, *sector, *track, y);
        }

        //  The bytes on a near-black panel with a blue tint (FR-150).
        if (sector != nullptr && sector->dataField >= 0)
        {
            painter.FillRoundedRect (m_boundsDip.left - m_scaler.ToPxf (s_kPanelPadDip), y, GetWidth() + m_scaler.ToPxf (s_kPanelPadDip), m_boundsDip.bottom - y,
                                     m_scaler.ToPxf (s_kPanelRadiusDip), s_kPanelArgb);
            PaintBytes (painter, text, GetPanelTheme (theme), *sector, *track, y + m_scaler.ToPxf (s_kPanelRadiusDip));
        }
        else
        {
            empty = (sector == nullptr && track != nullptr && !track->sectors.empty())
                        ? std::wstring (L"No sector is selected. Click a sector on the platter, in the strip or in the row of sectors to see its bytes.")
                        : InspectorText::FormatNoSectorData (m_context.analysis->entries[m_context.model->GetQuarterTrack()], track);
            text.DrawString (empty.c_str(), static_cast<float> (m_boundsDip.left), y, GetWidth(), m_scaler.ToPxf (static_cast<float> (kRowDip * 3)),
                             theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetTextChar
//
////////////////////////////////////////////////////////////////////////////////

wchar_t SectorByteView::GetTextChar (Byte value)
{
    Byte  masked = static_cast<Byte> (value & ~s_kHighBit);



    return (masked >= s_kFirstPrintable && masked <= s_kLastPrintable) ? static_cast<wchar_t> (masked) : L'.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintHeader
//
//  Labeled values, wrapping onto as many lines as the width needs; returns
//  where the bytes start.
//
////////////////////////////////////////////////////////////////////////////////

float SectorByteView::PaintHeader (IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y)
{
    vector<HeaderItem>  items  = InspectorText::BuildSectorHeader (sector, track);
    vector<HeaderItem>  owners;
    float               gap    = m_scaler.ToPxf (s_kHeaderGapDip);
    float               rowH   = m_scaler.ToPxf (static_cast<float> (kHeaderDip));
    float               textPx = m_scaler.ToPxf (kSmallDip);
    float               x      = static_cast<float> (m_boundsDip.left);
    float               w      = 0;
    float               h      = 0;
    float               top    = y;
    std::wstring        line;



    //  With a pending edit, the data checksum result once applied (FR-098).
    if (int edit = GetEdit(); edit >= 0)
    {
        items.push_back ({ L"Checksum after apply", EditController::IsChecksumGoodAfter (m_context.edits->GetEdit (edit), track) ? L"Good" : L"Bad" });
    }

    //  On a whole track of a mapped volume, the sector's role and owners.
    if (m_context.fileMap != nullptr && m_context.model->GetQuarterTrack() % DiskImage::kQuarterTracksPerWholeTrack == 0)
    {
        owners = FileMapText::BuildSectorItems (*m_context.fileMap, m_context.model->GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack, sector.sector);
        items.insert (items.end(), owners.begin(), owners.end());
    }

    for (const HeaderItem & item : items)
    {
        line = item.label + L": " + item.value;
        text.MeasureString (line.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

        if (x + w > m_boundsDip.right - ((y == top) ? m_headerReservePx : 0.0f) && x > m_boundsDip.left)
        {
            x  = static_cast<float> (m_boundsDip.left);
            y += rowH;
        }

        text.DrawString (line.c_str(), x, y, w + 1.0f, rowH, (item.value == L"Bad") ? theme.ErrorForeground() : theme.Foreground(),
                         textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        x += w + gap;
    }

    return y + rowH * 1.5f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintBytes
//
//  The sector's bytes; while comparing, B's paired sector below them, both
//  grids sized to fit, with each byte that differs marked in both (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintBytes (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y)
{
    const DataFieldDecode &  data    = track.fields[sector.dataField].data;
    std::span<const Byte>    bytesB  = GetPairedBytes (track);
    bool                     isB     = m_context.trackB != nullptr;
    float                    needed  = s_kOffsetCharsDip + kBytesPerRow * (s_kByteDip + s_kCharDip) + s_kTextGapDip;
    float                    fit     = std::min (1.0f, GetWidth() / m_scaler.ToPxf (needed));
    float                    rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip)) * std::max (fit, 0.75f);
    float                    textPx  = 0;
    float                    left    = static_cast<float> (m_boundsDip.left);
    bool                     isBad   = sector.isDataCheck && !sector.isDataGood;
    int                      rows    = static_cast<int> (data.bytes.size() + kBytesPerRow - 1) / kBytesPerRow;
    int                      index   = static_cast<int> (&sector - track.sectors.data());
    int                      edit    = GetEdit();
    std::span<const Byte>    shown   = (edit >= 0) ? std::span<const Byte> (m_context.edits->GetEdit (edit).bytes) : std::span<const Byte> (data.bytes);
    vector<Byte>             hits    (data.bytes.size(), 0);
    vector<Byte>             changed (data.bytes.size(), 0);
    ByteGrid                 grid;



    //  The rows shrink to the height there is, both grids and B's title
    //  sharing it while comparing, down to the smallest legible row.
    rows = rows * (isB ? 2 : 1);
    rowH = std::min (rowH, std::max (m_scaler.ToPxf (s_kMinRowDip), (static_cast<float> (m_boundsDip.bottom) - y) / (rows + (isB ? 2.5f : 0.0f) + (isBad ? 1.25f : 0.0f))));
    fit  = std::min (fit, rowH / m_scaler.ToPxf (static_cast<float> (kRowDip)));
    rows = rows / (isB ? 2 : 1);

    textPx = m_scaler.ToPxf (kTextDip) * fit;
    grid   = { y, rowH, left + m_scaler.ToPxf (s_kOffsetCharsDip) * fit, 0.0f, m_scaler.ToPxf (s_kByteDip) * fit, m_scaler.ToPxf (s_kCharDip) * fit, true };
    grid.textLeft = grid.hexLeft + kBytesPerRow * grid.byteW + m_scaler.ToPxf (s_kTextGapDip) * fit;

    if (isBad)
    {
        painter.FillRect (left, y, GetWidth(), rowH, (m_context.palette.colors.failedChecksum & 0x00FFFFFF) | 0x40000000);
        text.DrawString (L"Data field checksum failed; these bytes are as decoded", left, y, GetWidth(), rowH, theme.ErrorForeground(),
                         m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        grid.top += rowH * 1.25f;
    }

    for (size_t i = 0; m_context.findMarks != nullptr && i < hits.size(); i++)
    {
        hits[i] = m_context.findMarks->IsByteMarked (&track, index, static_cast<int> (i)) ? 1 : 0;
    }

    for (size_t i = 0; edit >= 0 && i < changed.size(); i++)
    {
        changed[i] = m_context.edits->GetEdit (edit).IsChanged (static_cast<int> (i)) ? 1 : 0;
    }

    m_grid = grid;
    PaintSelection (painter, theme);
    PaintGrid      (painter, text, theme, grid, shown, bytesB, hits, changed, textPx);
    PaintCursor    (painter, theme);

    if (isB)
    {
        grid.top += rows * rowH + rowH * 0.5f;
        text.DrawString (!bytesB.empty() ? L"B" : L"B has no sector to pair with this one", left, grid.top, GetWidth(), rowH, theme.HeadingForeground(),
                         m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        grid.top += rowH;

        if (!bytesB.empty())
        {
            PaintGrid (painter, text, theme, grid, bytesB, data.bytes, {}, {}, textPx);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintGrid
//
//  Rows of 16 under their offsets, hex then text. A byte that differs from
//  the other side's is tinted and underlined, so it shows without color;
//  with no other side, nothing is marked. A byte of a find hit is tinted
//  and outlined in both columns. A byte a pending edit changed is drawn in
//  the edit color with a corner mark in both columns (FR-097).
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintGrid (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const ByteGrid & grid, std::span<const Byte> bytes,
                                std::span<const Byte> other, std::span<const Byte> hits, std::span<const Byte> changed, float textPx)
{
    float     left   = static_cast<float> (m_boundsDip.left);
    float     line   = m_scaler.ToPxf (s_kDiffLineDip);
    size_t    i      = 0;
    int       row    = 0;
    int       col    = 0;
    float     rowTop = 0;
    uint32_t  color  = 0;
    float     mark   = m_scaler.ToPxf (s_kMarkDip);
    float     hx     = 0;
    float     tx     = 0;
    wchar_t   ch[2]  = {};



    for (i = 0; i < bytes.size(); i++)
    {
        row    = static_cast<int> (i) / kBytesPerRow;
        col    = static_cast<int> (i) % kBytesPerRow;
        rowTop = grid.top + row * grid.rowH;
        color  = (bytes[i] == 0) ? theme.ForegroundMuted() : theme.Foreground();
        hx     = grid.hexLeft + col * grid.byteW;
        tx     = grid.textLeft + col * grid.charW;

        //  A row that does not fit in the view is left out rather than drawn
        //  over what lies below it.
        if (rowTop + grid.rowH > static_cast<float> (m_boundsDip.bottom))
        {
            continue;
        }

        if (col == 0)
        {
            text.DrawString (std::format (L"{:02X}", i).c_str(), left, rowTop, grid.hexLeft - left, grid.rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        if (i < hits.size() && hits[i] != 0)
        {
            painter.FillRect    (grid.hexLeft + col * grid.byteW, rowTop, grid.byteW * 0.8f, grid.rowH, (m_context.palette.colors.findHit & 0x00FFFFFFu) | s_kHitAlpha);
            painter.OutlineRect (grid.hexLeft + col * grid.byteW, rowTop, grid.byteW * 0.8f, grid.rowH, 1.0f, m_context.palette.colors.findHit);
            painter.FillRect    (grid.textLeft + col * grid.charW, rowTop, grid.charW, grid.rowH, (m_context.palette.colors.findHit & 0x00FFFFFFu) | s_kHitAlpha);
        }

        if (!other.empty() && (i >= other.size() || other[i] != bytes[i]))
        {
            painter.FillRect (grid.hexLeft + col * grid.byteW, rowTop, grid.byteW * 0.8f, grid.rowH, (m_context.palette.colors.difference & 0x00FFFFFFu) | s_kDiffAlpha);
            painter.FillRect (grid.hexLeft + col * grid.byteW, rowTop + grid.rowH - line, grid.byteW * 0.8f, line, m_context.palette.colors.difference);
            painter.FillRect (grid.textLeft + col * grid.charW, rowTop + grid.rowH - line, grid.charW, line, m_context.palette.colors.difference);
        }

        if (i < changed.size() && changed[i] != 0)
        {
            color = m_context.palette.colors.pendingEdit;
            painter.FillConvexQuad (hx, rowTop, hx + mark, rowTop, hx, rowTop + mark, hx, rowTop + mark, color);
            painter.FillConvexQuad (tx, rowTop, tx + mark, rowTop, tx, rowTop + mark, tx, rowTop + mark, color);
        }

        text.DrawString (std::format (L"{:02X}", bytes[i]).c_str(), grid.hexLeft + col * grid.byteW, rowTop, grid.byteW, grid.rowH, color, textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        ch[0] = GetTextChar (bytes[i]);
        text.DrawString (ch, grid.textLeft + col * grid.charW, rowTop, grid.charW, grid.rowH, color, textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetPairedBytes
//
//  B's bytes for the selected sector while comparing: the sector B pairs
//  with it on the same quarter track, when that sector has data.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> SectorByteView::GetPairedBytes (const TrackAnalysis & track) const
{
    const TrackAnalysis *  trackB = m_context.trackB;
    int                    paired = (trackB != nullptr) ? DiskComparer::FindPairedSector (track, m_context.model->GetSectorIndex(), *trackB) : -1;
    int                    field  = (paired >= 0) ? trackB->sectors[paired].dataField : -1;



    return (field >= 0) ? std::span<const Byte> (trackB->fields[field].data.bytes) : std::span<const Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintSelection
//
//  The selected bytes shaded in both columns, a run per row.
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintSelection (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    int       first = m_context.model->GetFirstByte();
    int       end   = first + m_context.model->GetByteCount();
    int       row   = 0;
    int       a     = 0;
    int       b     = 0;
    float     y     = 0;
    uint32_t  shade = (theme.Accent() & 0x00FFFFFFu) | s_kSelectionAlpha;



    for (row = (first >= 0) ? first / kBytesPerRow : kBytesPerRow; row * kBytesPerRow < end; row++)
    {
        a = std::max (first, row * kBytesPerRow) % kBytesPerRow;
        b = (std::min (end, (row + 1) * kBytesPerRow) - 1) % kBytesPerRow + 1;
        y = m_grid.top + row * m_grid.rowH;

        painter.FillRect (m_grid.hexLeft + a * m_grid.byteW, y, (b - a) * m_grid.byteW, m_grid.rowH, shade);
        painter.FillRect (m_grid.textLeft + a * m_grid.charW, y, (b - a) * m_grid.charW, m_grid.rowH, shade);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::OnMouse
//
//  A press on a byte in either column selects it, Shift extends the
//  selection to it, and a drag extends it as it goes (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

bool SectorByteView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isText    = false;
    int   index     = HitTest (ev.positionDip, isText);
    bool  isHandled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && index >= 0)
    {
        if (ev.shift && m_context.model->GetByteCount() > 0)
        {
            m_context.model->ExtendBytes (index);
        }
        else
        {
            m_context.model->SelectBytes (index, 1, isText);
            m_isLowNibble = !isText && ev.positionDip.x >= m_grid.hexLeft + (index % kBytesPerRow) * m_grid.byteW + m_grid.byteW * 0.4f;
        }

        m_isDragging = true;
        isHandled    = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isDragging)
    {
        if (index >= 0)
        {
            m_context.model->ExtendBytes (index);
        }

        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_isDragging)
    {
        m_isDragging = false;
        isHandled    = true;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetEdit
//
//  The selected sector's pending edit, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SectorByteView::GetEdit() const
{
    const TrackAnalysis *  track = m_context.GetTrack();



    return (m_context.edits != nullptr && track != nullptr) ? m_context.edits->Find (*track, m_context.model->GetSectorIndex()) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintCursor
//
//  While editing, a box around the hex digit or the character typing goes
//  to.
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintCursor (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    int    offset = m_context.model->GetFirstByte();
    float  row    = 0;
    float  col    = 0;
    float  digitW = m_grid.byteW * 0.4f;



    if (m_isEditing && m_grid.isShown && offset >= 0 && offset < DiskFieldFormat::kSectorBytes && GetEdit() >= 0)
    {
        row = m_grid.top + (offset / kBytesPerRow) * m_grid.rowH;
        col = static_cast<float> (offset % kBytesPerRow);

        if (m_context.model->IsTextColumn())
        {
            painter.OutlineRect (m_grid.textLeft + col * m_grid.charW, row, m_grid.charW, m_grid.rowH, m_scaler.ToPxf (s_kCursorDip), theme.Accent());
        }
        else
        {
            painter.OutlineRect (m_grid.hexLeft + col * m_grid.byteW + (m_isLowNibble ? digitW : 0.0f), row, digitW, m_grid.rowH, m_scaler.ToPxf (s_kCursorDip), theme.Accent());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::MoveCursor
//
//  The cursor is the selection's first byte, so moving it selects that
//  byte alone; it stops at the sector's first and last bytes.
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::MoveCursor (int offset, bool isTextColumn, bool isLowNibble)
{
    if (offset < 0)
    {
        offset      = 0;
        isLowNibble = false;
    }
    else if (offset >= DiskFieldFormat::kSectorBytes)
    {
        offset      = DiskFieldFormat::kSectorBytes - 1;
        isLowNibble = !isTextColumn;
    }

    m_isLowNibble = isLowNibble && !isTextColumn;
    m_context.model->SelectBytes (offset, 1, isTextColumn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::OnEditKey
//
//  In the hex column a digit sets the digit under the cursor and moves to
//  the next one; in the text column a character sets the byte and moves on.
//  The arrows move by a digit or a character and by a row, Home and End to
//  the row's ends, and Tab between the columns.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorByteView::OnEditKey (const DxuiKeyEvent & ev)
{
    int   edit      = GetEdit();
    int   offset    = m_context.model->GetFirstByte();
    bool  isText    = m_context.model->IsTextColumn();
    bool  isHandled = false;



    if (!m_isEditing || edit < 0 || offset < 0)
    {
        return false;
    }

    if (ev.kind == DxuiKeyEventKind::Char && !ev.ctrl && !ev.alt)
    {
        if (isText)
        {
            isHandled = m_context.edits->TypeCharacter (edit, offset, static_cast<wchar_t> (ev.vk));

            if (isHandled)
            {
                MoveCursor (offset + 1, true, false);
            }
        }
        else
        {
            isHandled = m_context.edits->TypeHexDigit (edit, offset, !m_isLowNibble, static_cast<wchar_t> (ev.vk));

            if (isHandled)
            {
                MoveCursor (m_isLowNibble ? offset + 1 : offset, false, !m_isLowNibble);
            }
        }
    }
    else if (ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt && !ev.shift)
    {
        isHandled = true;

        switch (ev.vk)
        {
            case VK_LEFT:  MoveCursor ((isText || !m_isLowNibble) ? offset - 1 : offset, isText, !isText && !m_isLowNibble); break;
            case VK_RIGHT: MoveCursor ((isText || m_isLowNibble) ? offset + 1 : offset, isText, !isText && !m_isLowNibble);  break;
            case VK_UP:    MoveCursor (offset - kBytesPerRow, isText, m_isLowNibble);                                          break;
            case VK_DOWN:  MoveCursor (offset + kBytesPerRow, isText, m_isLowNibble);                                          break;
            case VK_HOME:  MoveCursor (offset - offset % kBytesPerRow, isText, false);                                         break;
            case VK_END:   MoveCursor (offset - offset % kBytesPerRow + kBytesPerRow - 1, isText, !isText);                    break;
            case VK_TAB:   MoveCursor (offset, !isText, false);                                                                break;
            default:       isHandled = false;                                                                                  break;
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetTooltip
//
//  Only a byte of a find hit has a tip (FR-056): its offset, and the hit.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorByteView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    sector = (m_context.model != nullptr) ? m_context.model->GetSectorIndex() : -1;
    bool                   isText = false;
    int                    index  = HitTest (pointPx, isText);
    bool                   hasTip = index >= 0 && sector >= 0 && m_context.findMarks != nullptr && m_context.findMarks->IsByteMarked (track, sector, index);



    if (hasTip)
    {
        outText     = std::format (L"Byte ${:02X}\n{}", index, m_context.findMarks->FormatHitLine (1));
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetTooltipSwatches
//
////////////////////////////////////////////////////////////////////////////////

bool SectorByteView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    std::wstring  text;
    RECT          anchor = {};
    bool          hasTip = GetTooltip (pointPx, text, anchor);



    if (hasTip)
    {
        outSwatches = { { 1, m_context.palette.colors.findHit } };
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::HitTest
//
//  The byte under a point and which column it is in, or -1. A drag past
//  either column's edge takes the byte at that edge.
//
////////////////////////////////////////////////////////////////////////////////

int SectorByteView::HitTest (POINT pointPx, bool & outIsText) const
{
    int    index    = -1;
    int    row      = 0;
    int    col      = 0;
    float  hexRight = m_grid.hexLeft + kBytesPerRow * m_grid.byteW;
    float  txtRight = m_grid.textLeft + kBytesPerRow * m_grid.charW;



    outIsText = pointPx.x >= m_grid.textLeft - (m_grid.textLeft - hexRight) / 2;

    if (m_grid.isShown && pointPx.y >= m_grid.top && pointPx.y < m_grid.top + kBytesPerRow * m_grid.rowH && pointPx.x < txtRight + m_grid.charW &&
        pointPx.x >= m_grid.hexLeft - m_grid.byteW)
    {
        row   = std::clamp (static_cast<int> ((pointPx.y - m_grid.top) / m_grid.rowH), 0, kBytesPerRow - 1);
        col   = outIsText ? static_cast<int> ((pointPx.x - m_grid.textLeft) / m_grid.charW) : static_cast<int> ((pointPx.x - m_grid.hexLeft) / m_grid.byteW);
        col   = std::clamp (col, 0, kBytesPerRow - 1);
        index = row * kBytesPerRow + col;
    }

    return index;
}
