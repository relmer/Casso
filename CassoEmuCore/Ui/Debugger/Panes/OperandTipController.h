#pragma once

#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/Panes/ICodeViews.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipScreen and OperandTipCell
//
//  The screen around the window the tip is in, in that window's client
//  pixels: each monitor's work area, and the windows lying over this one,
//  such as a floating pane, command bar or timeline. And the view and row
//  the tip covers, with the part of the cell, in the window's pixels, where
//  the pointer has to be for the tip to stay up.
//
////////////////////////////////////////////////////////////////////////////////

struct OperandTipScreen
{
    std::vector<RECT>  workAreas;
    std::vector<RECT>  overWindows;
};

struct OperandTipCell
{
    int   view = -1;
    int   row  = -1;
    RECT  area = {};
};

//  The parts of a row the tip lays out apart from the rest of its text: the
//  column of its operand and result, the column its code starts in and how
//  many characters into that column's text, and the symbol its operand is
//  written with, as "name = $value", empty for none.
struct OperandTipParts
{
    static constexpr size_t  kNone = SIZE_MAX;

    size_t        operandColumn = kNone;
    size_t        codeColumn    = kNone;
    int           codeChar      = 0;
    std::wstring  symbol;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IOperandTipHost
//
//  The window's facts the operand tip reads: the popup host and renderer
//  of the window the pointer is in, whether a drag or a menu has the
//  pointer, the view and row under a point, and whether anything lying over
//  the panes overlaps the tip or is under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

class IOperandTipHost
{
public:
    virtual ~IOperandTipHost() = default;

    virtual DxuiHwndSource       * GetTipPopupHost  () = 0;
    virtual IDxuiTextRenderer    * GetTipText       () = 0;
    virtual const DxuiDpiScaler  & GetTipScaler     () const = 0;
    virtual const DxuiTheme      * GetTipTheme      () const = 0;
    virtual HWND                   GetTipOwner      () const = 0;
    virtual bool                   IsTipBlocked     () const = 0;
    virtual bool                   TryFindTipRow    (POINT clientPx, int & view, int & row) const = 0;

    //  The list or text view a view's tip covers, and a row's parts: a
    //  disassembly row's operand and result, its code and its operand's
    //  symbol, or a source line's. A view has a list or a text view, not
    //  both.
    virtual DxuiListView *         GetTipList       (int view) const = 0;
    virtual DxuiTextView *         GetTipTextView   (int view) const = 0;
    virtual OperandTipParts        GetTipParts      (int view, int row) const = 0;
    virtual bool                   IsTipCovered     (int view, POINT clientPx, const RECT & tipRect, const OperandTipScreen & screen) const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController
//
//  The text of a disassembly row, a BASIC line or a source line that its
//  view cuts off, drawn whole over the row in a popup: the window the popup
//  is in, whether the tip is up, its layout, the row it covers, and the
//  renderer, scale and screen it was laid out with, which new rows and
//  layouts are checked against. A window with no HWND, as a test builds it,
//  measures with the renderer and screen the test sets. The color tip is the
//  tooltip last put up below the operand tip; it goes when that tip does.
//
////////////////////////////////////////////////////////////////////////////////

class OperandTipController
{
public:
    explicit OperandTipController (IOperandTipHost & host);

    OperandTipController (const OperandTipController &)             = delete;
    OperandTipController & operator= (const OperandTipController &) = delete;

    bool                              HasTip         () const { return m_hasTip; }
    const OperandResultTip::Layout  & GetLayout      () const { return m_layout; }
    const OperandTipCell            & GetCell        () const { return m_cell; }

    //  The tip in the window the pointer is in, at that window's scale,
    //  after a mouse event; a tip up in another window goes.
    void                 Update         (const DxuiMouseEvent & ev, bool isPressed);

    //  The tip laid out for a mouse event, with a renderer and screen given;
    //  and the tip over a point, none where the cell is whole in the pane or
    //  something lies over it.
    void                 Place          (const DxuiMouseEvent & ev, bool isPressed, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text,
                                         const OperandTipScreen & screen);
    bool                 TryGet         (POINT clientPx, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                         OperandResultTip::Layout & out, OperandTipCell & outCell) const;

    //  Takes the tip down; lays it out again for the cell under a pointer
    //  that has not moved, after the wheel or when new rows or a new layout
    //  of a view change the cells; takes it down when the pointer moves off
    //  the cell; and, once a frame, follows the pointer outside the window's
    //  mouse events.
    void                 Hide           ();
    void                 PlaceAtPointer ();
    void                 Check          (int view);
    void                 CheckPointer   (POINT clientPx);
    bool                 IsOverTipArea  (POINT clientPx) const;
    void                 FollowPointer  ();

    //  A floating window closed, which takes down a tip up in it.
    void                 OnPopupHostClosed (const DxuiHwndSource * host);

    //  The tooltip put up below the tip, which goes with it; and a tooltip
    //  going away, forgotten if it is that one.
    void                 SetColorTip    (DxuiTooltip * tip) { m_colorTip = m_hasTip ? tip : nullptr; }
    void                 ForgetColorTip (const DxuiTooltip * tip);

    //  A color tip over a cell is placed below the operand and result where
    //  they lie whole over their cell, rather than over their lines.
    RECT                 GetColorTipAnchor (const RECT & cell) const { return m_hasTip ? m_layout.rect : cell; }

    //  Whether the tip is up for the row under a point with its operand's
    //  symbol among the operand's values, which takes the place of the
    //  symbol tip there.
    bool                 IsSymbolShownAt (POINT clientPx) const;

    //  The renderer and screen the tip is measured with in a window that
    //  has no HWND, as a test builds it.
    void                 SetDeviceForTest (IDxuiTextRenderer * text, const OperandTipScreen & screen) { m_testText = text; m_testScreen = screen; }

private:
    struct WindowSearch
    {
        HWND               owner = nullptr;
        HWND               tipIn = nullptr;
        std::vector<RECT>  rects;
    };

    void                 Show           ();
    void                 SetTip         (OperandResultTip::Layout && layout, const OperandTipCell & cell, IDxuiTextRenderer & text, const DxuiDpiScaler & scaler,
                                         const OperandTipScreen & screen);
    void                 FitColumns     (int view, IDxuiTextRenderer & text) const;
    bool                 TryMake        (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                         OperandResultTip::Layout & out, OperandTipCell & outCell) const;
    bool                 TryMakeOnList  (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                         OperandResultTip::Layout & out, OperandTipCell & outCell) const;
    bool                 TryMakeOnText  (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                         OperandResultTip::Layout & out, OperandTipCell & outCell) const;
    uint32_t             GetRowFill     (const DxuiListView * list, int row) const;
    OperandTipScreen     GetScreen      (const DxuiHwndSource * host) const;
    void                 SetHighlight   (DxuiTextView * view, int row);

    //  The room either side of a text view's text under the tip, as a dense
    //  list pads a cell.
    static constexpr int kTextPadDip = 4;

    static bool          IsTextSelectedUnderTip (const DxuiListView * list, int row, const OperandResultTip::Layout & layout);
    static bool          TryAddListSegment      (const DxuiListView & list, int row, size_t column, const OperandTipParts & parts, uint32_t ink,
                                                 OperandResultTip::Placement & placement);
    static void          AddTextSegments        (const DxuiTextView & view, int row, const OperandTipParts & parts, uint32_t ink,
                                                 IDxuiTextRenderer & text, OperandResultTip::Placement & placement);
    static std::vector<uint32_t>  GetCellColors (const DxuiListView::Cell & cell, uint32_t ink);
    static RECT          GetRowWorkArea         (const RECT & row, const std::vector<RECT> & workAreas);
    static BOOL CALLBACK CollectWindowOver      (HWND hwnd, LPARAM param);

    IOperandTipHost           & m_host;
    DxuiInPlaceTip              m_tip;
    DxuiHwndSource            * m_popupHost  = nullptr;
    DxuiTextView              * m_lit        = nullptr;
    bool                        m_hasTip     = false;
    OperandResultTip::Layout    m_layout;
    OperandTipCell              m_cell;
    IDxuiTextRenderer         * m_text       = nullptr;
    DxuiDpiScaler               m_scaler;
    OperandTipScreen            m_screen;
    IDxuiTextRenderer         * m_testText   = nullptr;
    OperandTipScreen            m_testScreen;
    DxuiTooltip               * m_colorTip   = nullptr;

    //  Where the pointer last moved, or turned the wheel, with no button
    //  down, in the popup host's client pixels; false once a button is
    //  pressed or the pointer is outside the window.
    POINT                       m_pointer    = {};
    bool                        m_isResting  = false;
};
