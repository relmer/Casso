#include "Pch.h"

#include "Ui/DiskInspector/Views/SectorsView.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/SectorAccess.h"
#include "Shell/InspectorStatusPublisher.h"
#include "Core/NumberFormat.h"





static constexpr float     s_kHeaderGapDip   = 20.0f;
static constexpr float     s_kByteCells      = 74.0f;   // the hex view's character cells across a row of 16 bytes
static constexpr float     s_kCellDip        = 8.0f;
static constexpr float     s_kMinZoom        = 0.6f;
static constexpr float     s_kZoomStep       = 0.05f;
static constexpr float     s_kHitShare       = 0.45f;
static constexpr float     s_kConflictShare  = 0.25f;
static constexpr uint32_t  s_kPanelArgb      = 0xFF0E121Au;
static constexpr float     s_kPanelRadiusDip = 6.0f;
static constexpr float     s_kPanelPadDip    = 8.0f;
static constexpr int       s_kSectorRows     = DiskFieldFormat::kSectorBytes / SectorsView::kBytesPerRow;

//  Each byte has one mark, the first of these that applies.
static constexpr uint8_t  s_kMarkNone     = 0;
static constexpr uint8_t  s_kMarkDiffers  = 1;
static constexpr uint8_t  s_kMarkHit      = 2;
static constexpr uint8_t  s_kMarkChanged  = 3;
static constexpr uint8_t  s_kMarkConflict = 4;
static constexpr uint8_t  s_kMarkMatches  = 5;

//  While comparing, a byte that matches the other image's is drawn this much
//  of the way from the panel's color to the text's; one that differs is
//  drawn at full strength, as the Nibbles tab's nibbles are.
static constexpr float    s_kMatchInkShare = 0.35f;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::SectorsView
//
//  Both hex views reserve the edit frame, so their columns line up; only
//  the main one is edited. Neither is in the window's tab order: the view
//  passes them its input.
//
////////////////////////////////////////////////////////////////////////////////

SectorsView::SectorsView (InspectorViewContext & context) :
    InspectorView (context)
{
    for (DxuiHexView * hex : { &m_hexA, &m_hexB })
    {
        hex->SetParent       (this);
        hex->SetFocusable    (false);
        hex->GetHostEdit().SetEditable (true);
        hex->SetBytesPerRow  (kBytesPerRow);
        hex->SetTextEncoding (DxuiHexViewTypes::TextEncoding::AppleHighBit);
        hex->SetMarkColor    ([] (uint8_t, uint32_t &) { return false; });
    }

    m_hexA.SetSource (&m_sourceA);
    m_hexB.SetSource (&m_sourceB);

    m_hexA.SetByteStyle ([this] (uint64_t offset, uint8_t mark, bool isSelected, DxuiHexViewTypes::Column, DxuiHexViewTypes::ByteStyle & outStyle)
    {
        return PickStyle (m_sourceA, offset, mark, isSelected, outStyle);
    });

    m_hexB.SetByteStyle ([this] (uint64_t offset, uint8_t mark, bool isSelected, DxuiHexViewTypes::Column, DxuiHexViewTypes::ByteStyle & outStyle)
    {
        return PickStyle (m_sourceB, offset, mark, isSelected, outStyle);
    });

    m_hexA.GetHostEdit().SetIsChanged ([] (uint64_t, uint8_t mark)
    {
        return mark == s_kMarkChanged || mark == s_kMarkConflict;
    });

    m_hexA.GetHostEdit().SetOnType ([this] (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)
    {
        int  edit = GetEdit();



        if (edit < 0)
        {
            return false;
        }

        return isHexColumn ? m_context.edits->TypeHexDigit (edit, static_cast<int> (offset), isHighNibble, ch)
                           : m_context.edits->TypeCharacter (edit, static_cast<int> (offset), ch);
    });

    m_hexA.SetOnSelectionChanged ([this] () { TakeSelection(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetFullWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::GetFullWidthPx (const DxuiDpiScaler & scaler)
{
    return static_cast<int> (std::ceil (scaler.ToPxf (s_kByteCells * s_kCellDip + 2.0f * DxuiHexHostEdit::kEditFrameDip)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::Layout
//
//  The header has a fixed number of lines and the edit bar's row is there
//  whether or not a sector is edited, so the bytes don't move when a
//  sector is edited or another is selected.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  y = 0;



    InspectorView::Layout (boundsDip, scaler);

    y = boundsDip.top + static_cast<int> (std::ceil (scaler.ToPxf (static_cast<float> (kHeaderDip)) * (kHeaderLines + 0.25f)));

    if (m_editRow)
    {
        y += m_editRow ({ boundsDip.left, y, boundsDip.right, y });
    }

    m_bytesTopPx = y;
    PlaceBytes (m_isBad, m_isB);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PlaceBytes
//
//  The hex views below the bad-data band, when there is one. Their rows
//  shrink to the room there is, both views and B's title sharing it while
//  comparing, down to the smallest legible row.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PlaceBytes (bool isBad, bool isB)
{
    float  pad    = m_scaler.ToPxf (s_kPanelRadiusDip);
    float  rowDip = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float  top    = static_cast<float> (m_bytesTopPx) + pad + (isBad ? rowDip * 1.25f : 0.0f);
    float  room   = static_cast<float> (m_boundsDip.bottom) - top - pad;
    float  frame  = 2.0f * std::max (1.0f, m_scaler.ToPxf (static_cast<float> (DxuiHexHostEdit::kEditFrameDip)));
    float  rows   = isB ? 2.0f * s_kSectorRows + 1.5f : static_cast<float> (s_kSectorRows);
    float  rowPx  = (m_rowPx > 0.0f) ? m_rowPx : rowDip;
    float  zoom   = std::min ({ 1.0f, (room - (isB ? 2 : 1) * frame) / (rows * rowPx), GetWidth() / GetFullWidthPx (m_scaler) });
    float  row    = 0;
    LONG   height = 0;



    m_isBad = isBad;
    m_isB   = isB;

    zoom = std::max (s_kMinZoom, std::floor (zoom / s_kZoomStep) * s_kZoomStep);
    m_hexA.SetZoom (zoom);
    m_hexB.SetZoom (zoom);

    row    = rowPx * zoom;
    height = static_cast<LONG> (std::ceil (row * s_kSectorRows + frame)) + 1;

    //  Below the smallest row, each view takes its share of the room and
    //  scrolls, rather than running past the bottom of the panel.
    height = std::min (height, std::max (static_cast<LONG> (row + frame), static_cast<LONG> ((room - (isB ? row * 1.5f : 0.0f)) / (isB ? 2.0f : 1.0f))));

    m_hexA.Layout ({ m_boundsDip.left, static_cast<LONG> (top), m_boundsDip.right, static_cast<LONG> (top) + height }, m_scaler);

    top += height + row * 1.5f;
    m_hexB.Layout ({ m_boundsDip.left, static_cast<LONG> (top), m_boundsDip.right, static_cast<LONG> (top) + height }, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *   track   = m_context.GetTrack();
    const AnalyzedSector *  sector  = (track != nullptr) ? m_context.model->GetSector() : nullptr;
    const DxuiTheme *       surface = dynamic_cast<const DxuiTheme *> (&GetPanelTheme (theme));
    float                   pad     = m_scaler.ToPxf (s_kPanelPadDip);
    float                   top     = static_cast<float> (m_bytesTopPx);
    float                   rowDip  = m_scaler.ToPxf (static_cast<float> (kRowDip));
    bool                    isBad   = false;
    bool                    isB     = false;
    RECT                    cell    = {};
    std::wstring            empty;



    m_hexA.SetVisible (false);
    m_hexB.SetVisible (false);

    if (!m_context.hasDisk || m_context.analysis == nullptr)
    {
        return;
    }

    if (sector != nullptr)
    {
        PaintHeader (text, theme, *sector, *track);
    }

    if (sector == nullptr || sector->dataField < 0)
    {
        empty = (sector == nullptr && track != nullptr && !track->sectors.empty())
                    ? std::wstring (L"No sector is selected. Click a sector on the platter, in the strip or in the row of sectors to see its bytes.")
                    : InspectorText::FormatNoSectorData (m_context.analysis->entries[m_context.model->GetQuarterTrack()], track);
        text.DrawString (empty.c_str(), static_cast<float> (m_boundsDip.left), (sector == nullptr) ? static_cast<float> (m_boundsDip.top) : top, GetWidth(), rowDip * 3,
                         theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        return;
    }

    isBad = sector->isDataCheck && !sector->isDataGood;
    isB   = m_context.trackB != nullptr;

    if (isBad != m_isBad || isB != m_isB)
    {
        PlaceBytes (isBad, isB);
    }

    //  The bytes on a near-black panel with a blue tint (FR-150).
    m_panelTheme           = (surface != nullptr) ? *surface : DxuiTheme::Dark();
    m_panelTheme.contentBg = s_kPanelArgb;

    painter.FillRoundedRect (m_boundsDip.left - pad, top, GetWidth() + pad, m_boundsDip.bottom - top, m_scaler.ToPxf (s_kPanelRadiusDip), s_kPanelArgb);

    if (isBad)
    {
        painter.FillRect (static_cast<float> (m_boundsDip.left), top + m_scaler.ToPxf (s_kPanelRadiusDip), GetWidth(), rowDip,
                          (m_context.palette.colors.failedChecksum & 0x00FFFFFF) | 0x40000000);
        text.DrawString (L"Data field checksum failed; these bytes are as decoded", static_cast<float> (m_boundsDip.left), top + m_scaler.ToPxf (s_kPanelRadiusDip),
                         GetWidth(), rowDip, m_panelTheme.ErrorForeground(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                         DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    }

    ShowBytes (*sector, *track);
    FollowSelection();

    m_hexA.GetHostEdit().SetEditing (m_isEditing && GetEdit() >= 0);
    m_hexA.OnFocusChanged (m_isEditing && GetEdit() >= 0 && m_hasKeys);
    m_hexA.SetVisible     (true);
    m_hexA.Paint          (painter, text, m_panelTheme);

    if (isB)
    {
        SideNames     names = (m_context.comparison != nullptr) ? m_context.comparison->names : SideNames();
        std::wstring  title = (m_sourceB.GetByteCount() > 0) ? names.b : names.Get (1) + L" has no sector to pair with this one";

        cell = m_hexB.GetBounds();
        text.DrawString (title.c_str(), static_cast<float> (m_boundsDip.left),
                         cell.top - rowDip * m_hexB.GetZoom(), GetWidth(), rowDip * m_hexB.GetZoom(), m_panelTheme.HeadingForeground(), m_scaler.ToPxf (kSmallDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

        if (m_sourceB.GetByteCount() > 0)
        {
            m_hexB.SetVisible (true);
            m_hexB.Paint      (painter, text, m_panelTheme);
        }
    }

    //  The hex view measures its rows as it paints. Once a row's height is
    //  known, the views are placed again for it.
    cell = m_hexA.GetByteRect (0, DxuiHexViewTypes::Column::Hex);

    if (cell.bottom > cell.top && std::abs ((cell.bottom - cell.top) / m_hexA.GetZoom() - m_rowPx) >= 1.0f)
    {
        m_rowPx = (cell.bottom - cell.top) / m_hexA.GetZoom();
        PlaceBytes (isBad, isB);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PaintHeader
//
//  The header's lines (FR-040): where the sector is, its checks, where the
//  file system puts it and its role, then its owners and how many times
//  the guest read, wrote and scanned it (FR-161). Each line is at a fixed
//  place, whether or not it has items, and is cut off at the view's edge.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PaintHeader (IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track)
{
    using Lines = std::array<vector<HeaderItem>, kHeaderLines>;

    MapFileSystem       fileSystem = (m_context.fileMap != nullptr) ? m_context.fileMap->fileSystem : MapFileSystem::None;
    SectorHeader        header     = InspectorText::BuildSectorHeader (sector, track, fileSystem);
    Lines               lines;
    vector<HeaderItem>  mapped;
    float               gap        = m_scaler.ToPxf (s_kHeaderGapDip);
    float               rowH       = m_scaler.ToPxf (static_cast<float> (kHeaderDip));
    float               textPx     = m_scaler.ToPxf (kSmallDip);
    float               x          = 0;
    float               y          = 0;
    float               w          = 0;
    float               h          = 0;
    float               right      = 0;
    HRESULT             hr         = S_OK;



    std::copy (header.begin(), header.end(), lines.begin());

    //  On a whole track of a mapped volume, the sector's role, and its owners
    //  on the last line.
    if (m_context.fileMap != nullptr && m_context.model->GetQuarterTrack() % DiskImage::kQuarterTracksPerWholeTrack == 0)
    {
        mapped = FileMapText::BuildSectorItems (*m_context.fileMap, m_context.model->GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack, sector.sector);

        for (const HeaderItem & item : mapped)
        {
            lines[(item.label == L"Owner" || item.label == L"Owners") ? 3 : 2].push_back (item);
        }
    }

    if (m_context.drive != nullptr && m_context.drive->access != nullptr && &sector >= track.sectors.data() && &sector < track.sectors.data() + track.sectors.size())
    {
        size_t    qt     = static_cast<size_t> (m_context.model->GetQuarterTrack());
        size_t    s      = static_cast<size_t> (&sector - track.sectors.data());
        uint32_t  reads  = SectorAccess::CountReads  (track, m_context.drive->access->reads[qt])[s];
        uint32_t  writes = SectorAccess::CountWrites (track, m_context.drive->access->writes[qt])[s];
        uint32_t  scans  = SectorAccess::CountScans  (track, m_context.drive->access->scans[qt])[s];

        if (reads > 0)
        {
            lines[3].push_back ({ L"Reads", NumberFormat::FormatInteger (reads) });
        }

        if (writes > 0)
        {
            lines[3].push_back ({ L"Writes", NumberFormat::FormatInteger (writes) });
        }

        if (scans > 0)
        {
            lines[3].push_back ({ L"Scans", NumberFormat::FormatInteger (scans) });
        }
    }

    for (size_t line = 0; line < lines.size(); line++)
    {
        x     = static_cast<float> (m_boundsDip.left);
        y     = m_boundsDip.top + line * rowH;
        right = m_boundsDip.right - ((line == 0) ? m_headerReservePx : 0.0f);

        hr = text.PushClipRect (x, y, std::max (0.0f, right - x), rowH);
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (const HeaderItem & item : lines[line])
        {
            text.MeasureString ((item.label.empty() ? item.value : item.label + L": " + item.value).c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            PaintHeaderItem (text, theme, item, x, y, rowH);
            x += w + gap;
        }

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PaintHeaderItem
//
//  The label and value in the text color, with the value's unexpected part
//  in the error color. Each part starts where the text before it ends, its
//  trailing space included: a bar after that text is measured and then
//  taken off again.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PaintHeaderItem (IDxuiTextRenderer & text, const IDxuiTheme & theme, const HeaderItem & item, float x, float y, float rowH) const
{
    float         textPx  = m_scaler.ToPxf (kSmallDip);
    std::wstring  prefix  = item.label.empty() ? std::wstring() : item.label + L": ";
    std::wstring  line    = prefix + item.value;
    size_t        badAt   = std::min (prefix.size() + std::min (item.badStart, item.value.size()), line.size());
    size_t        badEnd  = std::min (badAt + item.badLength, line.size());
    size_t        ends[]  = { badAt, badEnd, line.size() };
    size_t        start   = 0;
    float         bar     = 0;
    float         before  = 0;
    float         w       = 0;
    float         h       = 0;



    text.MeasureString (L"|", textPx, DxuiTheme::kBodyFace, bar, h);

    for (size_t i = 0; i < std::size (ends); i++)
    {
        if (ends[i] > start)
        {
            text.MeasureString ((line.substr (0, start) + L"|").c_str(), textPx, DxuiTheme::kBodyFace, before, h);
            text.MeasureString (line.substr (start, ends[i] - start).c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            text.DrawString    (line.substr (start, ends[i] - start).c_str(), x + before - bar, y, w + 1.0f, rowH, (i == 1) ? theme.ErrorForeground() : theme.Foreground(),
                                textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        start = std::max (start, ends[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::ShowBytes
//
//  The sector's bytes, its pending edit's when it has one, with their marks:
//  a byte the guest also wrote, a changed byte, a byte of a find hit, and
//  while comparing, a byte that differs from B's paired sector (FR-121).
//  B's bytes are marked where they differ from the sector as decoded.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::ShowBytes (const AnalyzedSector & sector, const TrackAnalysis & track)
{
    const DataFieldDecode &  data   = track.fields[sector.dataField].data;
    std::span<const Byte>    bytesB = GetPairedBytes (track);
    int                      index  = static_cast<int> (&sector - track.sectors.data());
    int                      edit   = GetEdit();
    const PendingEdit *      change = (edit >= 0) ? &m_context.edits->GetEdit (edit) : nullptr;
    std::span<const Byte>    shown  = (change != nullptr) ? std::span<const Byte> (change->bytes) : std::span<const Byte> (data.bytes);
    uint64_t                 countB = m_sourceB.GetByteCount();



    m_sourceA.SetBytes (shown);
    m_sourceB.SetBytes (bytesB);

    if (m_sourceB.GetByteCount() != countB)
    {
        m_hexB.SetSource (&m_sourceB);
    }

    for (size_t i = 0; i < shown.size(); i++)
    {
        int  at = static_cast<int> (i);

        if (change != nullptr && change->IsConflict (at))
        {
            m_sourceA.SetMark (i, s_kMarkConflict);
        }
        else if (change != nullptr && change->IsChanged (at))
        {
            m_sourceA.SetMark (i, s_kMarkChanged);
        }
        else if (m_context.findMarks != nullptr && m_context.findMarks->IsByteMarked (&track, index, at))
        {
            m_sourceA.SetMark (i, s_kMarkHit);
        }
        else if (!bytesB.empty() && (i >= bytesB.size() || bytesB[i] != shown[i]))
        {
            m_sourceA.SetMark (i, s_kMarkDiffers);
        }
        else if (!bytesB.empty())
        {
            m_sourceA.SetMark (i, s_kMarkMatches);
        }
    }

    for (size_t i = 0; i < bytesB.size(); i++)
    {
        m_sourceB.SetMark (i, (i >= data.bytes.size() || data.bytes[i] != bytesB[i]) ? s_kMarkDiffers : s_kMarkMatches);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PickStyle
//
//  While comparing, a byte that matches the other side's is dimmed and one
//  that differs is drawn at full strength. A byte of a find hit has the
//  find color with an outline, and a byte the guest also wrote an outline
//  in the failure color. Otherwise a zero byte is muted. A selected byte is
//  the hex view's selection.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::PickStyle (const BlockHexSource & source, uint64_t offset, uint8_t mark, bool isSelected, DxuiHexViewTypes::ByteStyle & outStyle) const
{
    const DiskInspectorColors &  colors = m_context.palette.colors;



    outStyle = DxuiHexViewTypes::ByteStyle();

    if (isSelected)
    {
        return false;
    }

    switch (mark)
    {
        case s_kMarkDiffers:
            outStyle.ink = m_panelTheme.Foreground();
            break;

        case s_kMarkMatches:
            outStyle.ink = DxuiColor::Mix (s_kPanelArgb, m_panelTheme.Foreground(), s_kMatchInkShare);
            break;

        case s_kMarkHit:
            outStyle.fill    = DxuiColor::Mix (s_kPanelArgb, colors.findHit | 0xFF000000u, s_kHitShare);
            outStyle.outline = colors.findHit | 0xFF000000u;
            break;

        case s_kMarkConflict:
            outStyle.fill    = DxuiColor::Mix (s_kPanelArgb, colors.failedChecksum | 0xFF000000u, s_kConflictShare);
            outStyle.outline = colors.failedChecksum | 0xFF000000u;
            break;

        default:
            break;
    }

    if (mark != s_kMarkDiffers && mark != s_kMarkMatches && offset < source.GetBytes().size() && source.GetBytes()[offset] == 0)
    {
        outStyle.ink = m_panelTheme.ForegroundMuted();
    }

    return outStyle.fill != 0 || outStyle.ink != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetPairedBytes
//
//  B's bytes for the selected sector while comparing: the sector B pairs
//  with it on the same quarter track, when that sector has data.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> SectorsView::GetPairedBytes (const TrackAnalysis & track) const
{
    const TrackAnalysis *  trackB = m_context.trackB;
    int                    paired = (trackB != nullptr) ? DiskComparer::FindPairedSector (track, m_context.model->GetSectorIndex(), *trackB) : -1;
    int                    field  = (paired >= 0) ? trackB->sectors[paired].dataField : -1;



    return (field >= 0) ? std::span<const Byte> (trackB->fields[field].data.bytes) : std::span<const Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::FollowSelection
//
//  The main hex view's selection and column from the view model's. A
//  selection of nibbles, as made in the strip, selects the bytes whose high
//  bits are in them.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::FollowSelection()
{
    int                       first  = m_context.model->GetFirstByte();
    int                       count  = m_context.model->GetByteCount();
    int                       anchor = m_context.model->GetByteAnchor();
    DxuiHexViewTypes::Column  column = m_context.model->IsTextColumn() ? DxuiHexViewTypes::Column::Text : DxuiHexViewTypes::Column::Hex;
    uint64_t                  from   = 0;
    uint64_t                  to     = 0;



    m_isFollowing = true;

    if ((first < 0 || count <= 0) && m_context.model->GetBytesOfNibbles (first, count))
    {
        anchor = first;
    }

    if (first < 0 || count <= 0)
    {
        if (m_hexA.HasSelection())
        {
            m_hexA.ClearSelection();
        }
    }
    else
    {
        from = (anchor >= first && anchor < first + count) ? static_cast<uint64_t> (anchor) : static_cast<uint64_t> (first);
        to   = (from == static_cast<uint64_t> (first)) ? static_cast<uint64_t> (first + count - 1) : static_cast<uint64_t> (first);

        if (!m_hexA.HasSelection() || m_hexA.GetSelectionAnchor() != from || m_hexA.GetCaret() != to)
        {
            m_hexA.SelectByte (from, column);

            if (to != from)
            {
                m_hexA.ExtendSelectionTo (to);
            }
        }
    }

    if (m_hexA.GetActiveColumn() != column)
    {
        m_hexA.SetActiveColumn (column);
    }

    m_isFollowing = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::TakeSelection
//
//  The view model's byte selection from the main hex view's, after a click,
//  a drag, a key or typing changes it there.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::TakeSelection()
{
    uint64_t  anchor = m_hexA.GetSelectionAnchor();
    uint64_t  caret  = m_hexA.GetCaret();



    if (m_isFollowing || !m_hexA.HasSelection())
    {
        return;
    }

    m_context.model->SelectBytes (static_cast<int> (anchor), 1, m_hexA.GetActiveColumn() == DxuiHexViewTypes::Column::Text);

    if (caret != anchor)
    {
        m_context.model->ExtendBytes (static_cast<int> (caret));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::OnMouse
//
//  A press on a byte in either column selects it, Shift extends the
//  selection to it, and a drag extends it as it goes (FR-043). The main hex
//  view takes the press, and the drag until the button is released.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  bounds    = m_hexA.GetBounds();
    bool  isOver    = m_hexA.IsVisible() && PtInRect (&bounds, ev.positionDip);
    bool  isHandled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && isOver)
    {
        int      at       = HitTest (ev.positionDip);
        int64_t  nowMs    = static_cast<int64_t> (GetTickCount64());
        bool     isDouble = at >= 0 && at == m_pressAt && m_pressMs != 0 && nowMs - m_pressMs <= static_cast<int64_t> (GetDoubleClickTime());

        m_isDragging = !isDouble;
        isHandled    = true;
        m_pressAt    = at;
        m_pressMs    = isDouble ? 0 : nowMs;

        (void) m_hexA.OnMouse (ev);

        if (isDouble && m_onOpen)
        {
            m_onOpen (at);
        }
    }
    else if ((ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up) && m_isDragging)
    {
        m_isDragging = ev.kind == DxuiMouseEventKind::Move;
        isHandled    = true;

        (void) m_hexA.OnMouse (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::OnFocusEntered
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::OnFocusEntered (bool forward)
{
    int  first = m_context.model->GetFirstByte();



    m_context.model->SelectBytes ((std::max) (0, first), (first >= 0) ? m_context.model->GetByteCount() : 1, !forward);
    FollowSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GoToByte
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::GoToByte (int offset)
{
    m_context.model->SelectBytes (offset, 1, false);
    FollowSelection();
    m_hexA.GoToOffset (static_cast<uint64_t> (offset));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::SetEditing
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::SetEditing (bool isEditing)
{
    m_isEditing = isEditing;
    m_hexA.GetHostEdit().ResetNibble();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetEdit
//
//  The selected sector's pending edit, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::GetEdit() const
{
    const TrackAnalysis *  track = m_context.GetTrack();



    return (m_context.edits != nullptr && track != nullptr) ? m_context.edits->Find (*track, m_context.model->GetSectorIndex()) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::OnEditKey
//
//  While the sector is edited, typing and the keys that move the caret go
//  to the main hex view: the arrows by a byte and by a row, Home and End to
//  the row's ends, and Tab between the columns.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::OnEditKey (const DxuiKeyEvent & ev)
{
    bool  isCaretKey = ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_LEFT || ev.vk == VK_RIGHT || ev.vk == VK_UP || ev.vk == VK_DOWN ||
                                                              ev.vk == VK_HOME || ev.vk == VK_END || ev.vk == VK_TAB);



    if (!m_isEditing || GetEdit() < 0 || m_context.model->GetFirstByte() < 0 || ev.ctrl || ev.alt)
    {
        return false;
    }

    if (ev.kind != DxuiKeyEventKind::Char && !isCaretKey)
    {
        return false;
    }

    FollowSelection();

    return m_hexA.OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::HitTest
//
//  The byte of the main hex view under a point, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::HitTest (POINT pointPx) const
{
    DxuiHexViewTypes::HitResult  hit = m_hexA.IsVisible() ? m_hexA.HitTestPoint (pointPx) : DxuiHexViewTypes::HitResult();



    return hit.hit ? static_cast<int> (hit.offset) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetTooltip
//
//  Only a byte of a find hit has a tip (FR-056): its offset, and the hit.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    sector = (m_context.model != nullptr) ? m_context.model->GetSectorIndex() : -1;
    int                    index  = HitTest (pointPx);
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
//  SectorsView::GetTooltipSwatches
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
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
