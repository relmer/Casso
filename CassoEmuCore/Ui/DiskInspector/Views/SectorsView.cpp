#include "Pch.h"

#include "Ui/DiskInspector/Views/SectorsView.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/SectorAccess.h"
#include "Shell/InspectorStatusPublisher.h"
#include "Core/NumberFormat.h"
#include "Ui/DiskInspector/EditBar.h"





static constexpr float     s_kHeaderGapDip   = 20.0f;
static constexpr float     s_kByteCells      = 4.0f;    // the hex view's character cells for each byte across a row
static constexpr float     s_kRowCells       = 10.0f;   // and for the address and the gaps between the columns
static constexpr float     s_kScrollbarDip   = 12.0f;
static constexpr float     s_kViewGapDip     = 16.0f;
static constexpr int       s_kMinSidePerRow  = 16;      // the fewest bytes across each view for side by side
static constexpr float     s_kRegionShare    = 0.45f;
static constexpr float     s_kTitleRows      = 1.4f;    // the row over the views while comparing, in rows of the bytes
static constexpr float     s_kToggleDip      = 26.0f;   // the button that moves B's view, as tall as Edit and Copy sector
static constexpr float     s_kToggleGapDip   = 6.0f;
static constexpr size_t    s_kHeaderGroups   = 4;       // the header's groups of items: where, checks, role, owners and counts
static constexpr float     s_kLabelEndsDip   = 24.0f;   // room at the ends of a box's top edge for its corners and the label's gaps
static constexpr float     s_kCellDip        = 8.0f;
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
static constexpr uint8_t  s_kMarkMissing  = 6;   // B has no sector paired with A's
static constexpr uint8_t  s_kMarkPad      = 7;   // past a sector's last byte, to the end of its row

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
        hex->SetColumns      (0);
        hex->SetTextEncoding (DxuiHexViewTypes::TextEncoding::AppleHighBit);
        hex->SetMarkColor    ([] (uint8_t, uint32_t &) { return false; });
        hex->SetShowRegions  (true);
        hex->SetTightRegionEdges (true);
    }

    m_sourceA.SetAbsentMark (s_kMarkPad);
    m_sourceB.SetAbsentMark (s_kMarkPad);
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



        if (edit < 0 || m_selectedSlot < 0 || offset / m_slotLen != static_cast<uint64_t> (m_selectedSlot) || offset % m_slotLen >= DiskFieldFormat::kSectorBytes)
        {
            return false;
        }

        offset %= m_slotLen;

        return isHexColumn ? m_context.edits->TypeHexDigit (edit, static_cast<int> (offset), isHighNibble, ch)
                           : m_context.edits->TypeCharacter (edit, static_cast<int> (offset), ch);
    });

    m_hexB.GetHostEdit().SetIsChanged ([] (uint64_t, uint8_t mark)
    {
        return mark == s_kMarkChanged || mark == s_kMarkConflict;
    });

    m_hexB.GetHostEdit().SetOnType ([this] (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)
    {
        int  edit = GetEditB();



        if (edit < 0 || m_selectedSlot < 0 || offset / m_slotLen != static_cast<uint64_t> (m_selectedSlot) || offset % m_slotLen >= DiskFieldFormat::kSectorBytes)
        {
            return false;
        }

        offset %= m_slotLen;

        return isHexColumn ? m_context.edits->TypeHexDigit (edit, static_cast<int> (offset), isHighNibble, ch)
                           : m_context.edits->TypeCharacter (edit, static_cast<int> (offset), ch);
    });

    m_hexA.SetOnSelectionChanged ([this] () { TakeSelection(); CopySelection (m_hexA, m_hexB); });
    m_hexB.SetOnSelectionChanged ([this] () { CopySelection (m_hexB, m_hexA); });

    m_link.Link (m_hexA, m_hexB);
    m_link.SetOnSettingsChanged ([this] () { PlaceBytes (m_isB); });

    m_hexA.SetOnBuildContextMenu ([this] (std::vector<DxuiPopupMenuItem> & items) { AddMenuItems (items, false); });
    m_hexB.SetOnBuildContextMenu ([this] (std::vector<DxuiPopupMenuItem> & items) { AddMenuItems (items, true); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::CopySelection
//
//  The other view's selection matches the one just changed, so the same
//  bytes are selected on both sides.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::CopySelection (DxuiHexView & from, DxuiHexView & to)
{
    if (m_isCopyingSel || !to.IsVisible())
    {
        return;
    }

    m_isCopyingSel = true;

    if (!from.HasSelection())
    {
        if (to.HasSelection())
        {
            to.ClearSelection();
        }
    }
    else if (!to.HasSelection() || to.GetSelectionAnchor() != from.GetSelectionAnchor() || to.GetCaret() != from.GetCaret())
    {
        to.SelectByte (from.GetSelectionAnchor(), from.GetActiveColumn());

        if (from.GetCaret() != from.GetSelectionAnchor())
        {
            to.ExtendSelectionTo (from.GetCaret());
        }
    }

    m_isCopyingSel = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetFullWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::GetFullWidthPx (const DxuiDpiScaler & scaler)
{
    return GetViewWidthPx (scaler, kBytesPerRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetViewWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::GetViewWidthPx (const DxuiDpiScaler & scaler, int bytesPerRow)
{
    float  cells = s_kByteCells * bytesPerRow + s_kRowCells;



    return static_cast<int> (std::ceil (scaler.ToPxf (cells * s_kCellDip + 2.0f * DxuiHexHostEdit::kEditFrameDip + s_kScrollbarDip)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::Layout
//
//  The header has a fixed number of lines, so the bytes don't move when
//  another sector is selected.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    InspectorView::Layout (boundsDip, scaler);

    m_bytesTopPx = -1;
    PlaceEditRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PlaceEditRow
//
//  The edit bar on the header's first line, left of the buttons there, when
//  its controls fit; otherwise on a row under the header. The bytes are
//  placed again when the row comes or goes.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PlaceEditRow()
{
    float  lineH   = m_scaler.ToPxf (static_cast<float> (kHeaderDip));
    int    y       = m_boundsDip.top + static_cast<int> (std::ceil (lineH * (kHeaderLines + 0.25f)));
    int    right   = static_cast<int> (GetButtonsLeftPx());
    int    inset   = m_scaler.ToPx (4);
    int    left    = right;
    RECT   inlined = { m_boundsDip.left + static_cast<LONG> (GetWidth() / 3.0f), m_boundsDip.top - inset, right, m_boundsDip.top - inset + m_scaler.ToPx (EditBar::kRowDip) };



    if (m_editRow)
    {
        y += m_editRow (inlined, { m_boundsDip.left, y, m_boundsDip.right, y }, left);
    }

    m_headerRightPx = static_cast<float> (left) - m_scaler.ToPxf (s_kToggleGapDip);

    if (y != m_bytesTopPx)
    {
        m_bytesTopPx = y;
        PlaceBytes (m_isB);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetButtonsLeftPx
//
//  Where the buttons at the right of the header's first line start: Edit and
//  Copy sector, and while comparing the button that moves B's view.
//
////////////////////////////////////////////////////////////////////////////////

float SectorsView::GetButtonsLeftPx() const
{
    float  toggle = (m_context.trackB != nullptr) ? m_scaler.ToPxf (s_kToggleDip + s_kToggleGapDip) : 0.0f;



    return m_boundsDip.right - m_headerReservePx - toggle - m_scaler.ToPxf (s_kToggleGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PlaceBytes
//
//  The hex views below the header, filling the room there is. While comparing, each image's title is over its view, and
//  B's view goes beside A's or below it, the two sharing the room.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PlaceBytes (bool isB)
{
    float  pad     = m_scaler.ToPxf (s_kPanelRadiusDip);
    float  rowDip  = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float  gap     = m_scaler.ToPxf (s_kViewGapDip);
    float  top     = static_cast<float> (m_bytesTopPx) + pad;
    float  title   = isB ? rowDip * s_kTitleRows : 0.0f;
    float  room    = std::max (0.0f, static_cast<float> (m_boundsDip.bottom) - top - pad);
    float  width   = GetWidth();
    bool   isSide  = isB && (m_arrangement == Arrangement::SideBySide ||
                             (m_arrangement == Arrangement::Auto && PickBytesPerRow ((width - gap) / 2.0f) >= s_kMinSidePerRow &&
                              CountShown (true, width, room, gap) >= CountShown (false, width, room, gap)));
    float  each    = isSide ? (width - gap) / 2.0f : width;
    float  height  = 0;
    LONG   left    = m_boundsDip.left;
    LONG   y       = 0;



    m_isB        = isB;
    m_isSide     = isSide;
    m_titleTopPx = top;

    //  Stacked, the gap over B's view is a title row, for its name.
    if (isB && !isSide)
    {
        gap = std::max (gap, title);
    }

    top   += title;
    room   = std::max (0.0f, room - title);
    height = (isB && !isSide) ? (room - gap) / 2.0f : room;
    height = std::max (height, 0.0f);
    y      = static_cast<LONG> (top);

    m_hexA.Layout ({ left, y, left + static_cast<LONG> (each), y + static_cast<LONG> (height) }, m_scaler);

    if (isSide)
    {
        m_hexB.Layout ({ m_boundsDip.right - static_cast<LONG> (each), y, m_boundsDip.right, y + static_cast<LONG> (height) }, m_scaler);
    }
    else
    {
        y += static_cast<LONG> (height + gap);
        m_hexB.Layout ({ left, y, left + static_cast<LONG> (each), y + static_cast<LONG> (height) }, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PickBytesPerRow
//
//  The bytes across a row in a view `widthPx` wide: the columns chosen in
//  the hex view's menu, or as many as fit.
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::PickBytesPerRow (float widthPx) const
{
    int  count = DiskFieldFormat::kSectorBytes;



    if (m_hexA.GetColumns() == DxuiHexViewTypes::s_kFixedRowWidth)
    {
        return m_hexA.GetBytesPerRow();
    }

    if (m_hexA.GetColumns() > 0)
    {
        return m_hexA.GetColumns() * m_hexA.GetGrouping();
    }

    while (count > 1 && GetViewWidthPx (m_scaler, count) > widthPx)
    {
        count--;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::CountShown
//
//  The bytes visible in each view at once with B's view beside A's or below it,
//  up to a sector's, for picking the arrangement with more of them visible.
//  A tie goes to side by side, so the views stay side by side as they
//  widen while a whole sector fits. Side by side also requires at least
//  s_kMinSidePerRow bytes across each view, so a narrow, tall area stacks
//  the views rather than splitting them into narrow rows.
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::CountShown (bool isSide, float widthPx, float roomPx, float gapPx) const
{
    float  rowPx  = (m_rowPx > 0.0f) ? m_rowPx : m_scaler.ToPxf (static_cast<float> (kRowDip));
    float  each   = isSide ? (widthPx - gapPx) / 2.0f : widthPx;
    float  tall   = isSide ? roomPx : (roomPx - gapPx) / 2.0f;
    int    perRow = PickBytesPerRow (each);
    int    rows   = std::max (0, static_cast<int> ((tall - 2.0f * m_scaler.ToPxf (static_cast<float> (kRowDip))) / rowPx));



    if (GetViewWidthPx (m_scaler, perRow) > each)
    {
        return 0;
    }

    return std::min (perRow * rows, static_cast<int> (DiskFieldFormat::kSectorBytes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::AddMenuItems
//
//  While comparing, "Copy sector from" the other image into the view's
//  image, and a "Second image" submenu, in the hex views' menu.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::AddMenuItems (std::vector<DxuiPopupMenuItem> & items, bool isViewB)
{
    std::vector<DxuiPopupMenuItem>  places;
    std::shared_ptr<DxuiCommand>    parent;



    auto  choice = [this] (const wchar_t * label, std::function<bool()> checked, std::function<void()> apply)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



        command->label     = label;
        command->radio     = true;
        command->isChecked = std::move (checked);
        command->dispatch  = [this, apply = std::move (apply)] ()
        {
            apply();
            PlaceBytes (m_isB);
        };

        return DxuiPopupMenuItem::ForCommand (command);
    };

    if (m_context.trackB != nullptr)
    {
        std::shared_ptr<DxuiCommand>  copy = std::make_shared<DxuiCommand>();



        copy->label     = L"Copy sector from \"" + m_context.GetSideName (isViewB ? 0 : 1) + L"\"";
        copy->isEnabled = [this, isViewB] () { return m_canCopySector && m_canCopySector (isViewB ? 1 : 0); };
        copy->dispatch  = [this, isViewB] () { if (m_copySector) { m_copySector (isViewB ? 1 : 0); } };

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        items.push_back (DxuiPopupMenuItem::ForCommand (copy));

        places.push_back (choice (L"&Auto",         [this] () { return m_arrangement == Arrangement::Auto; },       [this] () { m_arrangement = Arrangement::Auto; }));
        places.push_back (choice (L"&Side by side", [this] () { return m_arrangement == Arrangement::SideBySide; }, [this] () { m_arrangement = Arrangement::SideBySide; }));
        places.push_back (choice (L"S&tacked",      [this] () { return m_arrangement == Arrangement::Stacked; },    [this] () { m_arrangement = Arrangement::Stacked; }));

        parent        = std::make_shared<DxuiCommand>();
        parent->label = L"Second &image";
        items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (places)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::TryShowHexMenu
//
//  The hex view's menu for a right-click on either view's bytes. False
//  elsewhere, for the tab's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::TryShowHexMenu (POINT pointPx, DxuiHwndSource & host)
{
    std::vector<DxuiPopupMenuItem>  items;
    RECT                            boundsA = m_hexA.GetBounds();
    RECT                            boundsB = m_hexB.GetBounds();
    DxuiHexView                   * view    = (m_hexA.IsVisible() && PtInRect (&boundsA, pointPx)) ? &m_hexA :
                                              (m_hexB.IsVisible() && PtInRect (&boundsB, pointPx)) ? &m_hexB : nullptr;



    if (view == nullptr)
    {
        return false;
    }

    view->BuildContextMenu (items);
    DxuiContextMenu::Show (host, pointPx.x, pointPx.y, std::move (items));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PaintTitleRow
//
//  The row over the views while comparing: each image's title over its view
//  when the titles don't fit on the boxes, and at the right the button that
//  puts B's view below A's or beside it.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PaintTitleRow (IDxuiPainter & painter, IDxuiTextRenderer & text)
{
    SideNames  names  = (m_context.comparison != nullptr) ? m_context.comparison->names : SideNames();
    float      height = m_scaler.ToPxf (static_cast<float> (kRowDip)) * s_kTitleRows;
    float      textPx = m_scaler.ToPxf (kSmallDip);



    PaintToggle (painter);

    for (int side = 0; side < 2; side++)
    {
        //  Each image's name over its view; stacked, B's goes in the gap over
        //  its view.
        RECT   view = (side == 0) ? m_hexA.GetBounds() : m_hexB.GetBounds();
        float  y    = (side == 1 && !m_isSide) ? view.top - height : m_titleTopPx;



        text.DrawString (names.Get (side).c_str(), static_cast<float> (view.left), y, static_cast<float> (view.right - view.left), height,
                         m_panelTheme.HeadingForeground(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                         DxuiFontWeight::SemiBold, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::PaintToggle
//
//  The button in the header's first line, left of Edit and Copy sector,
//  that puts B's view below A's or beside it. Its glyph is the arrangement
//  it switches to: a square split across, or down the middle.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::PaintToggle (IDxuiPainter & painter)
{
    float     size   = m_scaler.ToPxf (s_kToggleDip);
    float     right  = m_boundsDip.right - m_headerReservePx - m_scaler.ToPxf (s_kToggleGapDip);
    float     inset  = size * 0.25f;
    float     line   = std::max (1.0f, m_scaler.ToPxf (1.0f));
    float     x      = right - size;
    float     y      = static_cast<float> (m_boundsDip.top);
    float     w      = size - 2.0f * inset;
    uint32_t  ink    = m_panelTheme.Foreground();



    m_toggleRect = { static_cast<LONG> (x), static_cast<LONG> (y), static_cast<LONG> (right), static_cast<LONG> (y + size) };

    painter.FillRoundedRect (x, y, size, size, m_scaler.ToPxf (4.0f), DxuiColor::Mix (m_panelTheme.Background(), ink, 0.12f));

    //  One square, split down the middle or across.
    painter.FillRect (x + inset,            y + inset,            w,    line, ink);
    painter.FillRect (x + inset,            y + inset + w - line, w,    line, ink);
    painter.FillRect (x + inset,            y + inset,            line, w,    ink);
    painter.FillRect (x + inset + w - line, y + inset,            line, w,    ink);

    if (m_isSide)
    {
        painter.FillRect (x + inset, y + inset + (w - line) / 2.0f, w, line, ink);
    }
    else
    {
        painter.FillRect (x + inset + (w - line) / 2.0f, y + inset, line, w, ink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::Paint
//
//  Every sector of the track with data, one after another, each starting a
//  row; the selected sector's header is above them.
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
    bool                    isB     = false;
    bool                    hasData = false;
    RECT                    cell    = {};
    std::wstring            empty;



    m_hexA.SetVisible (false);
    m_hexB.SetVisible (false);

    if (!m_context.hasDisk || m_context.analysis == nullptr)
    {
        return;
    }

    PlaceEditRow();

    if (sector != nullptr)
    {
        PaintHeader (text, theme, *sector, *track);
    }

    for (size_t i = 0; track != nullptr && i < track->sectors.size(); i++)
    {
        hasData = hasData || track->sectors[i].dataField >= 0;
    }

    if (!hasData)
    {
        empty = InspectorText::FormatNoSectorData (m_context.analysis->entries[m_context.model->GetQuarterTrack()], track);
        text.DrawString (empty.c_str(), static_cast<float> (m_boundsDip.left), top, GetWidth(), rowDip * 3,
                         theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        return;
    }

    isB = m_context.trackB != nullptr;

    if (isB != m_isB)
    {
        PlaceBytes (isB);
    }

    //  The bytes on a near-black panel with a blue tint (FR-150).
    m_panelTheme           = (surface != nullptr) ? *surface : DxuiTheme::Dark();
    m_panelTheme.contentBg = s_kPanelArgb;

    painter.FillRoundedRect (m_boundsDip.left - pad, top, GetWidth() + pad, m_boundsDip.bottom - top, m_scaler.ToPxf (s_kPanelRadiusDip), s_kPanelArgb);

    ShowBytes (*track);
    ScrollToSelectedIfMoved();
    FollowSelection();

    m_hexA.GetHostEdit().SetEditing (m_isEditing && GetActiveSide() == 0 && GetEdit() >= 0);
    m_hexA.OnFocusChanged (m_isEditing && GetActiveSide() == 0 && GetEdit() >= 0 && m_hasKeys);
    m_hexB.GetHostEdit().SetEditing (m_isEditing && GetActiveSide() == 1 && GetEditB() >= 0);
    m_hexB.OnFocusChanged (m_isEditing && GetActiveSide() == 1 && GetEditB() >= 0 && m_hasKeys);
    m_hexA.SetVisible     (true);
    m_hexA.Paint          (painter, text, m_panelTheme);

    if (isB)
    {
        m_hexB.SetVisible (true);
        m_hexB.Paint      (painter, text, m_panelTheme);
        PaintTitleRow     (painter, text);
    }

    //  The hex view measures its rows as it paints. Once a row's height is
    //  known, the views are placed again for it.
    cell = m_hexA.GetByteRect (m_hexA.GetTopRow() * std::max (m_hexA.GetBytesPerRow(), 1), DxuiHexViewTypes::Column::Hex);

    if (cell.bottom > cell.top && std::abs ((cell.bottom - cell.top) / m_hexA.GetZoom() - m_rowPx) >= 1.0f)
    {
        m_rowPx = (cell.bottom - cell.top) / m_hexA.GetZoom();
        PlaceBytes (isB);
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
    using Lines  = std::array<vector<HeaderItem>, s_kHeaderGroups>;
    using Flowed = std::array<vector<HeaderItem>, kHeaderLines>;

    MapFileSystem       fileSystem = (m_context.fileMap != nullptr) ? m_context.fileMap->fileSystem : MapFileSystem::None;
    SectorHeader        header     = InspectorText::BuildSectorHeader (sector, track, fileSystem);
    Lines               lines;
    Flowed              flowed;
    bool                isWrapped  = false;
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

    //  Two groups of items to a line. An item of the first line that would
    //  run into the buttons there starts the second line, with the items
    //  after it.
    x = static_cast<float> (m_boundsDip.left);

    for (size_t group = 0; group < 2; group++)
    {
        for (const HeaderItem & item : lines[group])
        {
            text.MeasureString ((item.label.empty() ? item.value : item.label + L": " + item.value).c_str(), textPx, DxuiTheme::kBodyFace, w, h);

            isWrapped = isWrapped || x + w > m_headerRightPx;
            x        += w + gap;

            (isWrapped ? flowed[1] : flowed[0]).push_back (item);
        }
    }

    flowed[1].insert (flowed[1].end(), lines[2].begin(), lines[2].end());
    flowed[1].insert (flowed[1].end(), lines[3].begin(), lines[3].end());

    for (size_t line = 0; line < kHeaderLines; line++)
    {
        x     = static_cast<float> (m_boundsDip.left);
        y     = m_boundsDip.top + line * rowH;
        right = (line == 0) ? m_headerRightPx : static_cast<float> (m_boundsDip.right);

        hr = text.PushClipRect (x, y, std::max (0.0f, right - x), rowH);
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (const HeaderItem & item : flowed[line])
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
//  The track's sectors with data, in the order they pass the head, each in
//  a slot of whole rows, outlined and labeled with its number, the row
//  after its last byte padded: the pending edit's
//  bytes for a sector being edited, and the marks for a byte the guest also
//  wrote, a changed byte, a byte of a find hit and, while comparing, a byte
//  that differs from B's paired sector (FR-121). B's view has the paired
//  sectors in the same slots. The selected sector's outline is in the
//  accent color, and a sector whose data checksum failed has its outline in
//  the failure color.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::ShowBytes (const TrackAnalysis & track)
{
    static constexpr size_t           s_kSector = DiskFieldFormat::kSectorBytes;
    size_t                            perRow    = static_cast<size_t> (std::max (m_hexA.GetBytesPerRow(), 1));
    size_t                            slotLen   = (s_kSector + perRow - 1) / perRow * perRow;
    const DiskInspectorColors &       colors    = m_context.palette.colors;
    bool                              isB       = m_context.trackB != nullptr;
    uint32_t                          edge      = DxuiColor::Mix (s_kPanelArgb, m_panelTheme.Foreground(), s_kRegionShare);
    uint32_t                          selected  = m_panelTheme.Accent() | 0xFF000000u;
    uint32_t                          failed    = colors.failedChecksum | 0xFF000000u;
    uint64_t                          countA    = m_sourceA.GetByteCount();
    uint64_t                          countB    = m_sourceB.GetByteCount();
    int                               current   = m_context.model->GetSectorIndex();
    vector<Byte>                      bytesA;
    vector<Byte>                      bytesB;
    vector<uint8_t>                   marksA;
    vector<uint8_t>                   marksB;
    vector<BlockHexSource::Region>    regionsA;
    vector<BlockHexSource::Region>    regionsB;



    m_slots.clear();
    m_selectedSlot = -1;
    m_slotLen      = slotLen;

    BuildSlots (track);

    for (size_t slot = 0; slot < m_slots.size(); slot++)
    {
        m_selectedSlot = (m_slots[slot].sector >= 0 && m_slots[slot].sector == current) ? static_cast<int> (slot) : m_selectedSlot;
    }

    bytesA.assign (m_slots.size() * slotLen, 0);
    marksA.assign (bytesA.size(), s_kMarkPad);
    bytesB.assign (isB ? bytesA.size() : 0, 0);
    marksB.assign (bytesB.size(), s_kMarkPad);

    for (size_t slot = 0; slot < m_slots.size(); slot++)
    {
        int                      index   = m_slots[slot].sector;
        int                      field   = m_slots[slot].field;
        std::span<const Byte>    data    = (field >= 0) ? std::span<const Byte> (track.fields[field].data.bytes) : std::span<const Byte>();
        int                      edit    = (m_context.edits != nullptr && index >= 0) ? m_context.edits->Find (track, index) : -1;
        const PendingEdit *      change  = (edit >= 0) ? &m_context.edits->GetEdit (edit) : nullptr;
        std::span<const Byte>    shown   = (change != nullptr) ? std::span<const Byte> (change->bytes) : data;
        bool                     isLoose = false;
        int                      sectorB = m_slots[slot].sectorB;
        std::span<const Byte>    paired  = !isB ? std::span<const Byte>() : (index >= 0) ? GetPairedBytes (track, index, isLoose) :
                                           (field >= 0) ? GetLooseBBytes (track, slot, sectorB) : GetSectorBBytes (sectorB);
        size_t                   base    = slot * slotLen;
        bool                     isBad   = index >= 0 && track.sectors[index].isDataCheck && !track.sectors[index].isDataGood;
        bool                     isOddA  = isBad || index < 0;
        bool                     isOddB  = paired.empty() || isLoose;
        bool                     isSel   = static_cast<int> (slot) == m_selectedSlot;
        uint32_t                 argb    = isOddA ? failed : isSel ? selected : edge;
        uint32_t                 argbB   = isOddB ? failed : isSel ? selected : edge;
        std::wstring             label   = (index >= 0) ? std::format (L"Sector ${:02X}{}", track.sectors[index].sector, isBad ? L", checksum failed" : L"") :
                                           (sectorB < 0) ? std::wstring (L"Data field with no address field") :
                                           (field >= 0)  ? std::format (L"Sector ${:02X}: data field with no address field", m_context.trackB->sectors[sectorB].sector)
                                                         : std::format (L"Sector ${:02X}: none to pair", m_context.trackB->sectors[sectorB].sector);
        int                      editB   = (m_context.edits != nullptr && sectorB >= 0 && !isLoose) ? m_context.edits->Find (*m_context.trackB, sectorB, 1) : -1;
        const PendingEdit *      changeB = (editB >= 0) ? &m_context.edits->GetEdit (editB) : nullptr;



        //  With a pending edit on B's sector, B's view has its bytes, and A's
        //  bytes are compared with them.
        if (changeB != nullptr)
        {
            paired = changeB->bytes;
        }

        for (size_t i = 0; i < s_kSector; i++)
        {
            int  at = static_cast<int> (i);

            bytesA[base + i] = (i < shown.size()) ? shown[i] : Byte (0);
            marksA[base + i] = (field >= 0) ? s_kMarkNone : s_kMarkMissing;

            if (i >= shown.size())
            {
                continue;
            }

            if (change != nullptr && change->IsConflict (at))
            {
                marksA[base + i] = s_kMarkConflict;
            }
            else if (change != nullptr && change->IsChanged (at))
            {
                marksA[base + i] = s_kMarkChanged;
            }
            else if (index >= 0 && m_context.findMarks != nullptr && m_context.findMarks->IsByteMarked (&track, index, at))
            {
                marksA[base + i] = s_kMarkHit;
            }
            else if (isB && !paired.empty())
            {
                marksA[base + i] = (i >= paired.size() || paired[i] != shown[i]) ? s_kMarkDiffers : s_kMarkMatches;
            }
        }

        regionsA.push_back ({ base, s_kSector, label, argb, isSel, isOddA ? failed : 0 });

        if (!isB)
        {
            continue;
        }

        for (size_t i = 0; i < s_kSector; i++)
        {
            int  at = static_cast<int> (i);

            bytesB[base + i] = (i < paired.size()) ? paired[i] : Byte (0);
            marksB[base + i] = paired.empty()                                  ? s_kMarkMissing  :
                               (changeB != nullptr && changeB->IsConflict (at)) ? s_kMarkConflict :
                               (changeB != nullptr && changeB->IsChanged (at))  ? s_kMarkChanged  :
                               (i >= shown.size() || shown[i] != bytesB[base + i]) ? s_kMarkDiffers : s_kMarkMatches;
        }

        regionsB.push_back ({ base, s_kSector,
                              paired.empty()  ? (index >= 0 ? std::format (L"Sector ${:02X}: none to pair", track.sectors[index].sector) : std::wstring (L"No sector to pair")) :
                              isLoose         ? std::format (L"Sector ${:02X}: data field with no address field", track.sectors[index].sector) :
                              (sectorB >= 0)  ? std::format (L"Sector ${:02X}", m_context.trackB->sectors[sectorB].sector) : label,
                              argbB, isSel, isOddB ? failed : 0 });
    }

    m_sourceA.SetBytes   (bytesA);
    m_sourceB.SetBytes   (bytesB);
    m_sourceA.SetRegions (std::move (regionsA));
    m_sourceB.SetRegions (std::move (regionsB));

    for (size_t i = 0; i < marksA.size(); i++)
    {
        m_sourceA.SetMark (i, marksA[i]);
    }

    for (size_t i = 0; i < marksB.size(); i++)
    {
        m_sourceB.SetMark (i, marksB[i]);
    }

    for (DxuiHexView * hex : { &m_hexA, &m_hexB })
    {
        if (hex->GetAddressSpace() != slotLen)
        {
            hex->SetAddressSpace (slotLen);
        }
    }

    //  A new count of bytes is a new set of sectors.
    if (m_sourceA.GetByteCount() != countA)
    {
        m_hexA.SetSource (&m_sourceA);
        m_shownSlot = -1;
    }

    if (m_sourceB.GetByteCount() != countB)
    {
        m_hexB.SetSource (&m_sourceB);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::BuildSlots
//
//  A slot for each of A's data fields, in the order they pass the head:
//  with its sector when an address field leads it, and alone when none does.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::BuildSlots (const TrackAnalysis & track)
{
    std::vector<int>  sectorOf (track.fields.size(), -1);



    m_slots.clear();

    for (size_t i = 0; i < track.sectors.size(); i++)
    {
        if (track.sectors[i].dataField >= 0)
        {
            sectorOf[track.sectors[i].dataField] = static_cast<int> (i);
        }
    }

    for (size_t f = 0; f < track.fields.size(); f++)
    {
        if (track.fields[f].role == FieldRole::Data && (sectorOf[f] >= 0 || !track.fields[f].data.bytes.empty()))
        {
            m_slots.push_back ({ sectorOf[f], static_cast<int> (f) });
        }
    }

    AddSectorsOnlyInB (track);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::AddSectorsOnlyInB
//
//  While comparing, a slot after A's for each of B's sectors that pairs with
//  none of A's sectors or data fields.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::AddSectorsOnlyInB (const TrackAnalysis & track)
{
    const TrackAnalysis *  trackB = m_context.trackB;
    std::vector<bool>      isUsed (trackB != nullptr ? trackB->sectors.size() : 0, false);
    size_t                 countA = m_slots.size();



    if (trackB == nullptr)
    {
        return;
    }

    for (size_t slot = 0; slot < countA; slot++)
    {
        int  sectorB = -1;



        if (m_slots[slot].sector >= 0)
        {
            sectorB               = DiskComparer::FindPairedSector (track, m_slots[slot].sector, *trackB);
            m_slots[slot].sectorB = sectorB;
        }
        else
        {
            (void) GetLooseBBytes (track, slot, sectorB);
            m_slots[slot].sectorB = sectorB;
        }

        if (sectorB >= 0)
        {
            isUsed[sectorB] = true;
        }
    }

    for (size_t i = 0; i < trackB->sectors.size(); i++)
    {
        if (!isUsed[i] && trackB->sectors[i].dataField >= 0)
        {
            m_slots.push_back ({ -1, -1, static_cast<int> (i) });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetSectorBBytes
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> SectorsView::GetSectorBBytes (int sectorB) const
{
    const TrackAnalysis *  trackB = m_context.trackB;



    if (trackB == nullptr || sectorB < 0 || trackB->sectors[sectorB].dataField < 0)
    {
        return {};
    }

    return trackB->fields[trackB->sectors[sectorB].dataField].data.bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetLooseBBytes
//
//  B's bytes for A's data field in slot `slot` that no address field leads:
//  the nth such field of A pairs with the nth of B's sectors that aren't
//  paired with a sector of A. Sets outSectorB to that sector.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> SectorsView::GetLooseBBytes (const TrackAnalysis & track, size_t slot, int & outSectorB) const
{
    const TrackAnalysis *  trackB = m_context.trackB;
    std::vector<bool>      isUsed (trackB != nullptr ? trackB->sectors.size() : 0, false);
    int                    nth    = 0;



    outSectorB = -1;

    for (size_t s = 0; s < slot; s++)
    {
        nth += (m_slots[s].sector < 0 && m_slots[s].field >= 0) ? 1 : 0;
    }

    for (size_t i = 0; trackB != nullptr && i < track.sectors.size(); i++)
    {
        int  paired = DiskComparer::FindPairedSector (track, static_cast<int> (i), *trackB);

        if (paired >= 0)
        {
            isUsed[paired] = true;
        }
    }

    for (size_t i = 0; trackB != nullptr && outSectorB < 0 && i < trackB->sectors.size(); i++)
    {
        if (!isUsed[i] && trackB->sectors[i].dataField >= 0 && nth-- == 0)
        {
            outSectorB = static_cast<int> (i);
        }
    }

    return (outSectorB >= 0) ? std::span<const Byte> (trackB->fields[trackB->sectors[outSectorB].dataField].data.bytes) : std::span<const Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::ScrollToSelected
//
//  The selected sector's first row at the top of the views, at the next
//  paint, when the Sectors tab is opened.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::ScrollToSelected()
{
    m_shownSlot = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::ScrollToSelectedIfMoved
//
//  A sector selected anywhere but in the views, such as on the platter, is
//  scrolled to the top, and again when the row width changes; one clicked
//  in the views stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::ScrollToSelectedIfMoved()
{
    int  perRow = std::max (m_hexA.GetBytesPerRow(), 1);



    if (m_selectedSlot >= 0 && (m_selectedSlot != m_shownSlot || perRow != m_shownPerRow))
    {
        m_hexA.SetTopRow (static_cast<uint64_t> (m_selectedSlot) * (m_slotLen / perRow));
        m_shownSlot    = m_selectedSlot;
        m_shownPerRow  = perRow;
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

        case s_kMarkMissing:
        case s_kMarkPad:
            outStyle.ink = s_kPanelArgb;
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

    if (mark != s_kMarkDiffers && mark != s_kMarkMatches && mark != s_kMarkMissing && mark != s_kMarkPad && offset < source.GetBytes().size() && source.GetBytes()[offset] == 0)
    {
        outStyle.ink = m_panelTheme.ForegroundMuted();
    }

    return outStyle.fill != 0 || outStyle.ink != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetPairedBytes
//
//  B's bytes for A's sector `index` while comparing, and whether they are a
//  data field no address field leads: the sector B pairs
//  with it on the same quarter track, when that sector has data.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> SectorsView::GetPairedBytes (const TrackAnalysis & track, int index, bool & outIsLoose) const
{
    const TrackAnalysis *  trackB = m_context.trackB;
    int                    paired = (trackB != nullptr) ? DiskComparer::FindPairedSector (track, index, *trackB) : -1;
    int                    field  = -1;
    int                    nth    = 0;



    field      = (paired >= 0) ? trackB->sectors[paired].dataField : -1;
    outIsLoose = false;

    //  With no sector of the same number in B, as when B's address field is
    //  damaged, the nth of A's unpaired sectors pairs with the nth of B's
    //  data fields that no address field leads.
    if (trackB != nullptr && paired < 0 && index >= 0)
    {
        std::vector<bool>  isLed (trackB->fields.size(), false);



        for (int i = 0; i < index; i++)
        {
            nth += (DiskComparer::FindPairedSector (track, i, *trackB) < 0) ? 1 : 0;
        }

        for (const AnalyzedSector & sector : trackB->sectors)
        {
            if (sector.addressField >= 0 && sector.dataField >= 0)
            {
                isLed[sector.dataField] = true;
            }
        }

        for (size_t i = 0; field < 0 && i < trackB->fields.size(); i++)
        {
            if (trackB->fields[i].role == FieldRole::Data && !isLed[i] && !trackB->fields[i].data.bytes.empty() && nth-- == 0)
            {
                field      = static_cast<int> (i);
                outIsLoose = true;
            }
        }
    }

    return (field >= 0) ? std::span<const Byte> (trackB->fields[field].data.bytes) : std::span<const Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::FollowSelection
//
//  The main hex view's selection and column from the view model's, in the
//  selected sector's slot. A selection of nibbles, as made in the strip,
//  selects the bytes whose high bits are in them.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::FollowSelection()
{
    int                       first  = m_context.model->GetFirstByte();
    int                       count  = m_context.model->GetByteCount();
    int                       anchor = m_context.model->GetByteAnchor();
    DxuiHexViewTypes::Column  column = m_context.model->IsTextColumn() ? DxuiHexViewTypes::Column::Text : DxuiHexViewTypes::Column::Hex;
    uint64_t                  base   = static_cast<uint64_t> (std::max (m_selectedSlot, 0)) * m_slotLen;
    uint64_t                  from   = 0;
    uint64_t                  to     = 0;



    m_isFollowing = true;

    if (m_selectedSlot < 0)
    {
        first = -1;
    }

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
        from = base + ((anchor >= first && anchor < first + count) ? static_cast<uint64_t> (anchor) : static_cast<uint64_t> (first));
        to   = base + ((from == base + first) ? static_cast<uint64_t> (first + count - 1) : static_cast<uint64_t> (first));

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
//  a drag, a key or typing changes it there. A byte in another sector's slot
//  selects that sector, and a selection stays in its anchor's sector.
//
////////////////////////////////////////////////////////////////////////////////

void SectorsView::TakeSelection()
{
    static constexpr uint64_t  s_kSector = DiskFieldFormat::kSectorBytes;
    uint64_t                   anchor    = m_hexA.GetSelectionAnchor();
    uint64_t                   caret     = m_hexA.GetCaret();
    int                        slot      = static_cast<int> (anchor / m_slotLen);
    uint64_t                   base      = static_cast<uint64_t> (slot) * m_slotLen;



    if (m_isFollowing || !m_hexA.HasSelection() || slot >= static_cast<int> (m_slots.size()))
    {
        return;
    }

    if (slot != m_selectedSlot && m_slots[slot].sector >= 0)
    {
        m_context.model->SelectSector (m_context.model->GetQuarterTrack(), m_slots[slot].sector);
        m_selectedSlot = slot;
        m_shownSlot    = slot;
    }

    anchor = std::min (anchor, base + s_kSector - 1);
    caret  = std::clamp (caret, base, base + s_kSector - 1);

    m_context.model->SelectBytes (static_cast<int> (anchor - base), 1, m_hexA.GetActiveColumn() == DxuiHexViewTypes::Column::Text);

    if (caret != anchor)
    {
        m_context.model->ExtendBytes (static_cast<int> (caret - base));
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
    RECT           boundsA   = m_hexA.GetBounds();
    RECT           boundsB   = m_hexB.GetBounds();
    bool           isOverA   = m_hexA.IsVisible() && PtInRect (&boundsA, ev.positionDip);
    bool           isOverB   = m_hexB.IsVisible() && PtInRect (&boundsB, ev.positionDip);
    DxuiHexView  * over      = isOverA ? &m_hexA : isOverB ? &m_hexB : nullptr;
    bool           isHandled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && m_isB && PtInRect (&m_toggleRect, ev.positionDip))
    {
        m_arrangement = m_isSide ? Arrangement::Stacked : Arrangement::SideBySide;
        isHandled     = true;

        PlaceBytes (m_isB);
    }
    else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && (isOverA || isOverB))
    {
        DxuiHexView &  view     = isOverA ? m_hexA : m_hexB;
        int            side     = isOverA ? 0 : 1;
        int            at       = HitTest (view, ev.positionDip);
        int64_t        nowMs    = static_cast<int64_t> (GetTickCount64());
        bool           isDouble = at >= 0 && at == m_pressAt && side == m_pressSide && m_pressMs != 0 && nowMs - m_pressMs <= static_cast<int64_t> (GetDoubleClickTime());

        m_dragView   = isDouble ? nullptr : &view;
        m_activeSide = side;
        m_pressSide  = side;
        isHandled    = true;
        m_pressAt    = at;
        m_pressMs    = isDouble ? 0 : nowMs;

        (void) view.OnMouse (ev);

        if (isDouble && m_onOpen)
        {
            m_onOpen (static_cast<int> (std::min<uint64_t> (static_cast<uint64_t> (at) % m_slotLen, DiskFieldFormat::kSectorBytes - 1)));
        }
    }
    else if ((ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Up) && m_dragView != nullptr)
    {
        DxuiHexView *  view = m_dragView;

        m_dragView = (ev.kind == DxuiMouseEventKind::Move) ? view : nullptr;
        isHandled  = true;

        (void) view->OnMouse (ev);
    }
    else if (ev.kind == DxuiMouseEventKind::Wheel && over != nullptr)
    {
        isHandled = over->OnMouse (ev);
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
    m_hexA.GoToOffset (static_cast<uint64_t> (std::max (m_selectedSlot, 0)) * m_slotLen + static_cast<uint64_t> (offset));
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
//  SectorsView::GetEditB
//
//  The pending edit of B's sector in the selected sector's slot, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::GetEditB() const
{
    int  sectorB = GetSelectedSectorB();



    return (m_context.edits != nullptr && sectorB >= 0) ? m_context.edits->Find (*m_context.trackB, sectorB, 1) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetSelectedSectorB
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::GetSelectedSectorB() const
{
    bool  isSlot = m_context.trackB != nullptr && m_selectedSlot >= 0 && m_selectedSlot < static_cast<int> (m_slots.size());



    return isSlot ? m_slots[m_selectedSlot].sectorB : -1;
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



    if (!m_isEditing || (GetActiveSide() == 0 ? GetEdit() : GetEditB()) < 0 || m_context.model->GetFirstByte() < 0 || ev.ctrl || ev.alt)
    {
        return false;
    }

    if (ev.kind != DxuiKeyEventKind::Char && !isCaretKey)
    {
        return false;
    }

    FollowSelection();

    return (GetActiveSide() == 0) ? m_hexA.OnKey (ev) : m_hexB.OnKey (ev);
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
    return HitTest (m_hexA, pointPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::HitTest
//
//  The byte of a hex view under a point, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SectorsView::HitTest (const DxuiHexView & view, POINT pointPx)
{
    DxuiHexViewTypes::HitResult  hit = view.IsVisible() ? view.HitTestPoint (pointPx) : DxuiHexViewTypes::HitResult();



    return hit.hit ? static_cast<int> (hit.offset) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView::GetTooltip
//
//  A byte of a find hit has a tip (FR-056): its offset, and the hit. A
//  sector's label cut off at the view's right has its full text as a tip.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorsView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    at     = HitTest (pointPx);
    int                    slot   = (at >= 0) ? static_cast<int> (at / m_slotLen) : -1;
    int                    sector = (slot >= 0 && slot < static_cast<int> (m_slots.size())) ? m_slots[slot].sector : -1;
    int                    index  = (at >= 0) ? static_cast<int> (at % m_slotLen) : -1;
    bool                   hasTip = index >= 0 && sector >= 0 && m_context.findMarks != nullptr && m_context.findMarks->IsByteMarked (track, sector, index);



    if (m_isB && PtInRect (&m_toggleRect, pointPx))
    {
        outText     = m_isSide ? L"Show the second image below" : L"Show the second image side by side";
        outAnchorPx = m_toggleRect;
        return true;
    }

    for (const DxuiHexView * hex : { &m_hexA, &m_hexB })
    {
        if (hex->IsVisible() && hex->TryGetLabelTipAt (pointPx, outAnchorPx, outText))
        {
            return true;
        }
    }

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
    bool          hasTip = !(m_isB && PtInRect (&m_toggleRect, pointPx)) && GetTooltip (pointPx, text, anchor);



    if (hasTip)
    {
        outSwatches = { { 1, m_context.palette.colors.findHit } };
    }

    return hasTip;
}