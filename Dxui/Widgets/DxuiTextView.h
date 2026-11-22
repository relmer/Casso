#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiScrollbar.h"
#include "DxuiTextLayout.h"
#include "DxuiTextPainter.h"
#include "DxuiTextPicture.h"
#include "DxuiTextScroll.h"
#include "DxuiTextSelection.h"
#include "DxuiTextViewPictures.h"
#include "DxuiTextViewInterfaces.h"
#include "IDxuiTextPictureHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView
//
//  Read-only text in the fixed-width face, laid out as rows of cells, with
//  selection by character.
//
//  CELLS LINE UP IN COLUMNS AND THE LAST ONE WRAPS. Every cell but a row's last
//  starts in a column as wide as that column's widest cell. The last cell takes
//  the rest of the width and wraps at a space, and a wrapped line continues
//  under the last cell's first character, not under the row's, or a row's
//  wrap indent past it, as source code wraps under its operand. A BASIC line is
//  a number and a statement, so a long statement wraps under itself; a detail
//  is a label and a value, so a long value wraps under the value.
//
//  A TABLE DOESN'T WRAP. With SetNoWrap, each row is one line, and a row wider
//  than the view scrolls sideways by whole characters under a horizontal
//  scrollbar. Proportional text always wraps.
//
//  A row can have a warning, which draws a warning mark before its first cell,
//  and spans, runs of a cell's characters drawn in colors of their own, as
//  source code's syntax is colored.
//
//  The host sets the character cell size, or the view measures it from the
//  theme's fixed-width face, so layout and selection can be tested without a
//  device.
//
//  PROPORTIONAL TEXT, such as an image's details, is drawn in the theme's
//  body face, with every cell but a row's last in the muted color. Each
//  character's place is measured once a paint has the renderer, and the
//  columns, wrapping and selection are laid out by those places instead of
//  by cells.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextView : public IDxuiControl, private IDxuiTextGeometry, private IDxuiTextPictureHost
{
public:
    using Span     = DxuiTextLayout::Span;
    using Picture  = DxuiTextLayout::Picture;
    using Row      = DxuiTextLayout::Row;
    using Position = DxuiTextLayout::Position;

    //  Asked for a context menu at a point in the same coordinates as the bounds.
    using ContextMenuFn = std::function<void (POINT atPoint)>;

    DxuiTextView() { m_focusable = true; }
    ~DxuiTextView() override = default;

    //  Replaces the text, clearing the selection and scrolling to the top.
    void                       SetRows      (std::vector<Row> rows);
    const DxuiTextLayout     & GetLayout    () const { return m_layout; }
    DxuiTextScroll           & GetScroll    ()       { return m_scroll; }
    const DxuiTextScroll     & GetScroll    () const { return m_scroll; }
    DxuiTextSelection        & GetSelection ()       { return m_selection; }
    const DxuiTextSelection  & GetSelection () const { return m_selection; }

    //  One character cell, in the same coordinates as the bounds.
    void  SetCellSize      (int widthPx, int heightPx);
    void  SetProportional  (bool proportional);
    void  SetNoWrap        (bool noWrap);
    void  SetIconFace      (const wchar_t * face) { m_iconFace = face; }
    void  SetOwnerWindow   (HWND hwnd)            { m_hwnd = hwnd; }
    void  SetOnContextMenu (ContextMenuFn fn)     { m_onContextMenu = std::move (fn); }

    //  The face's size as a multiple of the theme's, and how far the text
    //  color goes from the background toward the theme's foreground (1 is
    //  the full foreground).
    void   SetZoom         (float zoom);
    float  GetZoom         () const          { return m_zoom; }
    void   SetTextStrength (float strength)  { m_textStrength = strength; }

    //  The text measured and laid out at its bounds' width before it is drawn,
    //  and the height its lines take there, for a view sized to its text.
    void  Measure            (IDxuiTextRenderer & text, const IDxuiTheme & theme);
    int   GetContentHeightPx () const;

    //  The rows' pictures: painting, hit testing, the outline, and the zoom,
    //  pan and ping of a picture shown as a map.
    DxuiTextViewPictures        & GetPictures ()       { return m_pictures; }
    const DxuiTextViewPictures  & GetPictures () const { return m_pictures; }

    //  Whether every row of more than one cell fits the text's width with each
    //  cell at its natural width, so no cell wraps: false for a table that
    //  doesn't fit. True before the text is measured.
    bool  DoColumnsFit () const;

    bool  IsInteracting   () const { return m_selection.IsDragging() || m_scroll.IsDragging(); }
    bool  IsOverScrollbar (POINT pt) const override { return m_scroll.IsOverScrollbar (pt); }

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                ShowScrollbarMenu (HWND hwnd, POINT pt) override { return DxuiScrollbar::ShowMenu (hwnd, pt, m_scroll.GetScrollbarMenu (pt)); }
    bool                OnKey             (const DxuiKeyEvent & ev) override { return m_scroll.OnKey (ev); }
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                InvokeCommand     (DxuiStandardCommand command) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Text"; }

private:
    static constexpr int  s_kPadDip             = 6;
    static constexpr int  s_kScrollbarWidthDip  = 10;


    //  IDxuiTextGeometry: the bounds and the place of the text in them.
    const RECT           & GetViewBounds  () const override { return m_boundsDip; }
    POINT                  GetTextOrigin  () const override { return POINT { m_boundsDip.left + m_scaler.ToPx (s_kPadDip), m_boundsDip.top + m_scaler.ToPx (s_kPadDip) }; }
    int                    GetTextColumns (bool scrollbar) const override;
    int                    GetPadPx       () const override { return m_scaler.ToPx (s_kPadDip); }
    int                    GetBarWidthPx  () const override { return m_scaler.ToPx (s_kScrollbarWidthDip); }
    const DxuiDpiScaler  & GetScaler      () const override { return m_scaler; }

    //  IDxuiTextPictureHost: the lines, rows and geometry the pictures read.
    int                      GetPictureLineCount    () const override         { return m_layout.GetLineCount(); }
    int                      GetPictureLineRow      (int line) const override { return m_layout.GetLines()[(size_t) line].row; }
    int                      GetPictureRowCount     () const override         { return (int) m_layout.GetRows().size(); }
    const DxuiTextPicture  * GetRowPicture          (int row) const override  { return (row >= 0 && row < (int) m_layout.GetRows().size()) ? m_layout.GetRows()[(size_t) row].picture.get() : nullptr; }
    int                      GetPictureTopLine      () const override         { return m_scroll.GetTopLine(); }
    int                      GetPictureLineHeight   () const override         { return m_layout.GetCellHeightPx(); }
    int                      GetPictureTextTop      () const override         { return (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip); }
    float                    GetPictureTextLeft     () const override         { return (float) GetTextOrigin().x - (float) m_scroll.GetLeftColumn() * m_layout.GetCellAdvance(); }
    float                    GetPictureTextWidth    () const override         { return (float) GetTextColumns (m_scroll.IsScrollbarVisible()) * m_layout.GetCellAdvance(); }
    const DxuiDpiScaler    & GetPictureScaler       () const override         { return m_scaler; }
    void                     Rebuild                ();
    void                     EnsureCellSize         (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    DxuiTextLayout        m_layout;
    DxuiTextScroll        m_scroll        { m_layout, *this };
    DxuiTextSelection     m_selection     { m_layout, *this, m_scroll };
    DxuiTextViewPictures  m_pictures      { *this };
    DxuiTextPainter       m_painter       { m_layout, *this, m_scroll, m_selection, m_pictures };
    float                 m_zoom          = 1.0f;
    float                 m_textStrength  = 1.0f;
    HWND                  m_hwnd          = nullptr;
    const wchar_t       * m_iconFace      = L"Segoe MDL2 Assets";
    ContextMenuFn         m_onContextMenu;
    DxuiDpiScaler         m_scaler;
};
