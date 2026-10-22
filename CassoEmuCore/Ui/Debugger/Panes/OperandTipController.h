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
    virtual bool                   IsTipCovered     (int view, POINT clientPx, const RECT & tipRect, const OperandTipScreen & screen) const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController
//
//  A disassembly row's operand and result that the pane cuts off, drawn
//  whole over their cell in a popup: the window the popup is in, whether
//  the tip is up, its layout, the cell it covers, and the renderer, scale
//  and screen it was laid out with, which new rows and layouts are checked
//  against. A window with no HWND, as a test builds it, measures with the
//  renderer and screen the test sets. The color tip is the tooltip last put
//  up below the operand tip; it goes when that tip does.
//
////////////////////////////////////////////////////////////////////////////////

class OperandTipController
{
public:
    OperandTipController (IOperandTipHost & host, const ICodeViews & views);

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

    //  Takes the tip down; takes it down when new rows or a new layout of a
    //  view change its cell, or when the pointer moves off the cell; and, once
    //  a frame, follows the pointer outside the window's mouse events.
    void                 Hide           ();
    void                 Check          (int view);
    void                 CheckPointer   (POINT clientPx);
    void                 FollowPointer  ();

    //  A floating window closed, which takes down a tip up in it.
    void                 OnPopupHostClosed (const DxuiHwndSource * host);

    //  The tooltip put up below the tip, which goes with it; and a tooltip
    //  going away, forgotten if it is that one.
    void                 SetColorTip    (DxuiTooltip * tip) { m_colorTip = m_hasTip ? tip : nullptr; }
    void                 ForgetColorTip (const DxuiTooltip * tip);

    //  A color tip over a cell is placed below the operand and result where
    //  they lie whole over their cell, rather than over their lines.
    //  Below the whole tip, from the cell's left: an L-shaped tip's rect
    //  starts at the pane's edge, and the color tip is about the cell.
    RECT                 GetColorTipAnchor (const RECT & cell) const
    {
        return m_hasTip ? RECT { m_layout.firstLeft, m_layout.rect.top, m_layout.rect.right, m_layout.rect.bottom } : cell;
    }

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
    bool                 TryMake        (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                         OperandResultTip::Layout & out, OperandTipCell & outCell) const;
    uint32_t             GetRowFill     (const DxuiListView * list, int row) const;
    OperandTipScreen     GetScreen      (const DxuiHwndSource * host) const;

    static bool          IsTextSelectedUnderTip (const DxuiListView * list, int row, const OperandResultTip::Layout & layout);
    static BOOL CALLBACK CollectWindowOver      (HWND hwnd, LPARAM param);

    IOperandTipHost           & m_host;
    const ICodeViews          & m_views;
    DxuiInPlaceTip              m_tip;
    DxuiHwndSource            * m_popupHost  = nullptr;
    bool                        m_hasTip     = false;
    OperandResultTip::Layout    m_layout;
    OperandTipCell              m_cell;
    IDxuiTextRenderer         * m_text       = nullptr;
    DxuiDpiScaler               m_scaler;
    OperandTipScreen            m_screen;
    IDxuiTextRenderer         * m_testText   = nullptr;
    OperandTipScreen            m_testScreen;
    DxuiTooltip               * m_colorTip   = nullptr;
};
