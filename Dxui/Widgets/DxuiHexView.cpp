#include "Pch.h"

#include "DxuiHexView.h"

#include "Core/DxuiClipboard.h"
#include "Widgets/DxuiContextMenu.h"
#include "Window/DxuiHwndSource.h"
#include "Theme/DxuiColor.h"
#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::BuildContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::BuildContextMenu (std::vector<DxuiPopupMenuItem> & items)
{
    std::vector<DxuiPopupMenuItem>  columns;
    std::shared_ptr<DxuiCommand>    parent;



    //  A choice, checked when the view is set to it, applying and reporting.
    auto  choice = [this] (const wchar_t * label, std::function<bool()> checked, std::function<void()> apply)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = label;
        command->radio     = true;
        command->isChecked = std::move (checked);
        command->dispatch  = [this, apply = std::move (apply)]()
        {
            apply();

            if (m_onSettings)
            {
                m_onSettings();
            }
        };

        return DxuiPopupMenuItem::ForCommand (command);
    };

    items.push_back (choice (L"Show &text only", [this]() { return !m_showValues; },
                                                 [this]() { SetShowValues (!m_showValues); }));

    for (int bytes : { 1, 2, 4 })
    {
        static constexpr const wchar_t *  s_kLabels[] = { L"&1-byte integer", L"&2-byte integer", nullptr, L"&4-byte integer" };

        items.push_back (choice (s_kLabels[bytes - 1], [this, bytes]() { return m_showValues && m_grouping == bytes; },
                                                       [this, bytes]() { if (SetGrouping (bytes)) { SetShowValues (true); } }));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (ValueFormat format : { ValueFormat::Hex, ValueFormat::Signed, ValueFormat::Unsigned })
    {
        const wchar_t *  label = (format == ValueFormat::Hex) ? L"&Hexadecimal" : (format == ValueFormat::Signed) ? L"&Signed" : L"&Unsigned";

        items.push_back (choice (label, [this, format]() { return m_format == format; },
                                        [this, format]() { SetShowValues (true); SetValueFormat (format); }));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int count : { 0, 1, 2, 4, 8, 16 })
    {
        static constexpr const wchar_t *  s_kCounts[] = { L"&Auto", L"&1", L"&2", L"&4", L"&8", L"1&6" };
        int                               index       = (count == 0) ? 0 : (count == 1) ? 1 : (count == 2) ? 2 : (count == 4) ? 3 : (count == 8) ? 4 : 5;

        columns.push_back (choice (s_kCounts[index], [this, count]() { return m_layout.m_columns == count; },
                                                     [this, count]() { SetColumns (count); }));
    }

    parent        = std::make_shared<DxuiCommand>();
    parent->label = L"Columns";
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (columns)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    {
        std::shared_ptr<DxuiCommand>  copy = std::make_shared<DxuiCommand>();

        copy->label       = L"&Copy";
        copy->accelerator = L"Ctrl+C";
        copy->isEnabled   = [this]() { return m_selection.m_hasSelection && GetSelectionCount() <= kMaxCopyBytes; };
        copy->dispatch    = [this]() { CopySelection(); };
        items.push_back (DxuiPopupMenuItem::ForCommand (copy));
    }

    if (m_onBuildMenu)
    {
        m_onBuildMenu (items);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::ShowContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::ShowContextMenu (POINT atDip)
{
    DxuiHwndSource *                host = DxuiHwndSource::FromHwnd (m_hwnd);
    std::vector<DxuiPopupMenuItem>  items;



    if (host == nullptr)
    {
        return;
    }

    BuildContextMenu (items);
    DxuiContextMenu::Show (*host, atDip.x, atDip.y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetSource
//
//  A new set of bytes is a new view: the old selection pointed at offsets
//  that mean nothing now, and the old scroll position may be past the end.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetSource (const IDxuiHexSource * source)
{
    m_source = source;

    ClearSelection();
    m_layout.ResetLineIndex();
    m_scroll.m_topRow = 0;

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetOriginAddress
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetOriginAddress (uint64_t address)
{
    m_originAddress = address;

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetAddressSpace
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetAddressSpace (uint64_t bytes)
{
    m_addressSpace = bytes;

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::TryGetByteTipAt
//
//  Anchored on the byte the point landed on, in the column it landed in, so
//  the tip moves from byte to byte with the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::TryGetByteTipAt (POINT clientDip, RECT & outCellDip, std::wstring & outTip) const
{
    HitResult  hit;



    if (m_source == nullptr || IsDragging() || !m_pending.empty() || IsOverScrollbar (clientDip))
    {
        return false;
    }

    hit = HitTestPoint (clientDip);

    if (!hit.hit || !m_source->TryGetByteTip (hit.offset, outTip))
    {
        return false;
    }

    outCellDip = GetByteRect (hit.offset, hit.column);

    return outCellDip.right > outCellDip.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionFill
//
//  A theme's selection can sit close to its background, which reads as faint
//  once the accent is gone from a view without focus.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexView::GetSelectionFill (const IDxuiTheme & theme)
{
    return DxuiColor::ComputeFillForContrast (theme.SelectionBackground(), theme.TextViewBackground(), kSelectionFillContrast);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetCaretInk
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexView::GetCaretInk (const IDxuiTheme & theme)
{
    return DxuiColor::ChooseInkFor (theme.Accent(), 0xFFFFFFFF, 0xFF000000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetSelectionInk
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexView::GetSelectionInk (const IDxuiTheme & theme)
{
    constexpr float     kTextContrast = 4.5f;   // WCAG AA for text
    constexpr uint32_t  kWhite        = 0xFFFFFFFF;
    constexpr uint32_t  kBlack        = 0xFF000000;
    uint32_t            fill          = GetSelectionFill (theme);
    uint32_t            ink           = DxuiColor::ChooseInkFor (fill, theme.Foreground(), theme.TextViewBackground());



    //  Lifting the fill off the background can bring it close to the theme's
    //  inks; white or black then reads where they do not.
    if (DxuiColor::ComputeContrastRatio (ink, fill) < kTextContrast)
    {
        ink = DxuiColor::ChooseInkFor (fill, kWhite, kBlack);
    }

    return ink;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetBytesPerRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetBytesPerRow (int count)
{
    assert (count > 0);

    if (count <= 0)
    {
        return;
    }

    m_bytesPerRow      = count;
    m_layout.m_columns = s_kFixedRowWidth;

    //  A grouping the new row width cannot divide falls back to single bytes,
    //  which every row width divides.
    if ((m_bytesPerRow % m_grouping) != 0)
    {
        m_grouping = kDefaultGrouping;
    }

    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetGrouping
//
//  One, two, four or eight bytes between spaces, and only a grouping that
//  divides the row evenly; a partial group at the end of a row would place the
//  same byte index at different positions on different rows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::SetGrouping (int bytesPerGroup)
{
    bool  known = (bytesPerGroup == 1) || (bytesPerGroup == 2)
               || (bytesPerGroup == 4) || (bytesPerGroup == 8);



    if (!known)
    {
        return false;
    }

    //  A row counted in values is always a whole number of them; only a row
    //  set in bytes can be one the grouping does not divide.
    if ((m_layout.m_columns == s_kFixedRowWidth) && ((m_bytesPerRow % bytesPerGroup) != 0))
    {
        return false;
    }

    m_grouping = bytesPerGroup;

    m_layout.RecomputeRowWidth();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetZoom
//
//  The cells are measured again at the new size on the next paint.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetZoom (float zoom)
{
    if (zoom == m_painter.m_zoom || zoom <= 0.0f)
    {
        return;
    }

    m_painter.m_zoom         = zoom;
    m_cellWidthDip           = 0;
    m_layout.m_cellHeightDip = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetColumns (int valuesPerRow)
{
    assert (valuesPerRow >= 0);

    m_layout.m_columns = (std::max) (valuesPerRow, 0);

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetValueFormat
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetValueFormat (ValueFormat format)
{
    m_format = format;

    m_layout.RecomputeRowWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetShowValues
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetShowValues (bool show)
{
    m_showValues = show;

    m_layout.ResetLineIndex();
    m_layout.RecomputeRowWidth();
    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetBreakLines
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetBreakLines (bool breakLines)
{
    m_breakLines = breakLines;

    m_layout.ResetLineIndex();
    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::SetCellSizeDip
//
//  One character cell of the host's fixed-width face. Every column position
//  is a whole number of cells, so hex digits and characters stay aligned down
//  the view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::SetCellSizeDip (int widthDip, int heightDip)
{
    assert (widthDip > 0 && heightDip > 0);

    m_cellWidthDip           = (std::max) (widthDip,  0);
    m_layout.m_cellHeightDip = (std::max) (heightDip, 0);

    m_layout.RecomputeRowWidth();
    m_layout.ClampTopRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GoToOffset
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::GoToOffset (uint64_t offset)
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    if (offset >= bytes)
    {
        return;
    }

    SelectByte (offset, m_selection.m_activeColumn);
    EnsureByteVisible (offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetLastOffset
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexView::GetLastOffset() const
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    return (bytes > 0) ? (bytes - 1) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::OnMouse  (IDxuiControl override)
//
//  A press goes to the scrollbars, then to the byte under it: a right-click
//  selects and runs the host's menu callback, or the view's menu without
//  one. A drag goes to a thumb being dragged, then to the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::OnMouse (const DxuiMouseEvent & ev)
{
    HitResult  hit;
    bool       handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down:
        if (m_scroll.OnPress (ev))
        {
            return true;
        }

        hit = m_layout.HitTestPoint (ev.positionDip);

        if (!hit.hit)
        {
            return false;
        }

        if (ev.button == DxuiMouseButton::Right)
        {
            m_selection.OnContextPress (hit);

            if (m_onContextMenu)
            {
                m_onContextMenu (ev.positionDip);
            }
            else
            {
                ShowContextMenu (ev.positionDip);
            }

            return true;
        }

        if (ev.button != DxuiMouseButton::Left)
        {
            return false;
        }

        m_selection.OnPress (ev, hit);
        return true;

    case DxuiMouseEventKind::Move:
        return m_scroll.OnDrag (ev) || m_selection.OnDrag (ev);

    case DxuiMouseEventKind::Up:
        if (m_scroll.TryRelease (handled))
        {
            return handled;
        }

        return m_selection.OnRelease();

    case DxuiMouseEventKind::Wheel:
        return m_scroll.OnWheel (ev);

    default:
        break;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::OnKey  (IDxuiControl override)
//
//  Copy and select all are standard commands, so the keystroke and the menu
//  row run the same code; the other keys move the caret or the column.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return TypeEditChar ((wchar_t) ev.vk);
    }

    if ((ev.kind != DxuiKeyEventKind::Down) || (m_layout.GetRowCount() == 0))
    {
        return false;
    }

    if (ev.vk == 'A' || ev.vk == 'C' || ev.vk == 'X' || ev.vk == 'V')
    {
        return InvokeCommand (DxuiCommandRouter::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift));
    }

    //  Escape drops a value typed only in part; with nothing typed it is not
    //  the view's, so a dialog can still take it.
    if (ev.vk == VK_ESCAPE)
    {
        if (m_pending.empty())
        {
            return false;
        }

        m_pending.clear();
        return true;
    }

    return m_selection.OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::TypeEditChar
//
//  A character goes to the column the caret is in. Line mode shows no values
//  to type into, so it takes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::TypeEditChar (wchar_t ch)
{
    if (!m_editable || m_source == nullptr || IsLineMode() || GetRowCount() == 0)
    {
        return false;
    }

    return (GetActiveColumn() == Column::Text) ? TypeCharacter (ch) : TypeHexDigit (ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::TypeHexDigit
//
//  The digits are the value as the column shows it, most significant first;
//  the bytes go to the source low byte first, as the value is read. Decimal
//  formats are not typed into, since a hex digit would not mean what the
//  column shows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::TypeHexDigit (wchar_t ch)
{
    int                   digit  = GetDigitValue (ch);
    size_t                group  = (size_t) (m_showValues ? m_grouping : 1);
    uint64_t              value  = 0;
    std::vector<uint8_t>  bytes;



    if (digit < 0 || m_format != ValueFormat::Hex)
    {
        return false;
    }

    if (m_pending.empty())
    {
        m_editStart = m_layout.SnapToValue (GetCaret(), false);
    }

    m_pending.push_back (DxuiHexPainter::GetHexDigit (digit));

    if (m_pending.size() < group * 2)
    {
        return true;
    }

    for (wchar_t each : m_pending)
    {
        value = (value << 4) | (uint64_t) GetDigitValue (each);
    }

    for (size_t index = 0; index < group; index++)
    {
        bytes.push_back ((uint8_t) ((value >> (index * 8)) & 0xFF));
    }

    m_pending.clear();
    WriteAndAdvance (m_editStart, bytes);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::TypeCharacter
//
//  A printable character becomes its byte in the view's encoding: Apple text
//  sets the high bit, which is how the screen shows a normal character.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::TypeCharacter (wchar_t ch)
{
    static constexpr wchar_t  kFirstPrintable = 0x20;
    static constexpr wchar_t  kLastPrintable  = 0x7E;
    static constexpr uint8_t  kAppleHighBit   = 0x80;
    uint8_t                   byte            = 0;



    if (ch < kFirstPrintable || ch > kLastPrintable)
    {
        return false;
    }

    byte = (uint8_t) ch;

    if (m_encoding == TextEncoding::AppleHighBit)
    {
        byte |= kAppleHighBit;
    }

    WriteAndAdvance (GetCaret(), std::span<const uint8_t> (&byte, 1));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::WriteAndAdvance
//
//  A write the source takes moves the caret past it; one it refuses leaves the
//  caret on the cell and tells the host.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::WriteAndAdvance (uint64_t offset, std::span<const uint8_t> bytes)
{
    if (m_source->WriteBytes (offset, bytes))
    {
        m_selection.MoveCaretTo (offset + bytes.size(), false);
        return;
    }

    if (m_onWriteRefused)
    {
        m_onWriteRefused (offset);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::GetDigitValue
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexView::GetDigitValue (wchar_t ch)
{
    int  value = -1;



    if (ch >= L'0' && ch <= L'9')
    {
        value = ch - L'0';
    }
    else if (ch >= L'A' && ch <= L'F')
    {
        value = ch - L'A' + 10;
    }
    else if (ch >= L'a' && ch <= L'f')
    {
        value = ch - L'a' + 10;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::QueryCommand  (IDxuiControl override)
//
//  The view handles Copy and Select all, in the active column's format. The
//  bytes are read-only here, so Cut and Paste are not handled and pass to the
//  containing control.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    uint64_t  bytes = (m_source != nullptr) ? m_source->GetByteCount() : 0;



    switch (command)
    {
    case DxuiStandardCommand::Copy:
        //  A copy is built in memory, so a selection past the cap is not
        //  copied at all rather than exhausting memory on a huge file.
        outEnabled = m_selection.m_hasSelection && GetSelectionCount() <= kMaxCopyBytes;
        return true;

    case DxuiStandardCommand::SelectAll:
        outEnabled = (bytes > 0);
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::InvokeCommand  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;



    if (!QueryCommand (command, enabled))
    {
        return false;
    }

    if (enabled && (command == DxuiStandardCommand::Copy))
    {
        CopySelection();
    }
    else if (enabled)
    {
        SelectAll();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::OnFocusEntered  (IDxuiControl override)
//
//  Focus arriving forward starts at the hex column and backward at the text
//  column, so Shift+Tab from the next control goes to the text column, and
//  another Shift+Tab to the hex column.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::OnFocusEntered (bool forward)
{
    m_selection.m_activeColumn = (forward && m_showValues) ? Column::Hex : Column::Text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    m_layout.RecomputeRowWidth();
    m_layout.ClampTopRow();
    m_scroll.SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexView::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float    x      = (float) m_boundsDip.left;
    float    y      = (float) m_boundsDip.top;
    float    width  = (float) (m_boundsDip.right - m_boundsDip.left);
    float    height = (float) m_layout.GetHeightDip();
    int      cap    = 0;
    HRESULT  hr     = S_OK;



    //  The background goes through the painter, whose layer is beneath the
    //  text renderer's, so the scrollbar the painter draws is not covered.
    painter.FillRect (x, y, width, height, theme.TextViewBackground());

    m_painter.EnsureCellSize (text, theme);

    //  The regions can change under the view, so their lanes are found again
    //  for every frame.
    if (m_regions.IsDrawn())
    {
        m_regions.RefreshLanes();
    }

    cap = m_layout.GetRowCap();

    if ((m_source == nullptr) || (cap <= 0))
    {
        return;
    }

    //  The rows stop short of the scrollbar, which paints over nothing.
    if (IsScrollbarVisible())
    {
        width -= (float) m_scaler.ToPx (s_kScrollbarWidthDip);
    }

    hr = text.PushClipRect (x, y, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_regions.IsDrawn())
    {
        painter.PushClip  (x, y, width, height);
        m_regions.Paint   (painter, text, theme);
        painter.PopClip   ();
    }

    for (int row = 0; row < cap; row++)
    {
        uint64_t  index = m_scroll.m_topRow + (uint64_t) row;
        int       count = 0;

        if (index >= m_layout.GetRowCount())
        {
            break;
        }

        count = m_painter.ReadRow (index);

        m_painter.PaintRow (text, theme, index, count);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_scroll.SyncScrollbar();

    if (IsScrollbarVisible())
    {
        m_scroll.m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }

    if (IsHorzScrollbarVisible())
    {
        m_scroll.m_horzScroll.Paint (painter, theme.ForegroundMuted());
    }
}
