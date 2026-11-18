#pragma once

class DisassemblyGutter;
class DxuiListView;
class FindWidgetSet;
class IDxuiControl;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneContentMouseHost
//
//  The window's side of the mouse over the panes' content: its
//  lists, press targets and text boxes, which of them is in the window the
//  event is routed to, the focus, the tips, and the disassembly views.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneContentMouseHost
{
public:
    virtual ~IPaneContentMouseHost() = default;

    virtual std::vector<DxuiListView *>  GetLists            () const = 0;
    virtual std::vector<IDxuiControl *>  GetPressTargets     () const = 0;
    virtual std::vector<IDxuiControl *>  GetHoverBoxes       () const = 0;
    virtual bool                         IsRoutable          (const IDxuiControl * control) const = 0;
    virtual void                         FocusControl        (IDxuiControl * control) = 0;
    virtual void                         UpdateTooltip       (POINT clientPx) = 0;
    virtual void                         NoteListReleased    (DxuiListView * list, POINT clientPx) = 0;
    virtual void                         NoteCodeListPressed (DxuiListView * list) = 0;
    virtual DisassemblyGutter          & GetCodeGutter       () const = 0;
    virtual FindWidgetSet              & GetFindWidgets      () const = 0;

    //  The wheel over a disassembly view scrolls the view through memory;
    //  false off every view.
    virtual bool                         ScrollCodeViewAt    (const DxuiMouseEvent & ev) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse
//
//  The mouse over the panes' content, once the window's bars, the dock site
//  and each pane's routes have passed on it: the list under the pointer,
//  or the one mid-drag, the press targets, the text boxes' hover, the
//  disassembly gutter and the find widgets.
//
////////////////////////////////////////////////////////////////////////////////

class PaneContentMouse
{
public:
    explicit PaneContentMouse (IPaneContentMouseHost & host);

    bool  Route      (const DxuiMouseEvent & ev);

    //  A press offered to a control, which takes the focus when it takes the
    //  press; handled is set then, and nothing is offered once it is.
    void  OfferPress (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled);

private:
    bool  RouteMove  (const DxuiMouseEvent & ev);
    bool  RouteDown  (const DxuiMouseEvent & ev);
    bool  RouteUp    (const DxuiMouseEvent & ev);
    bool  RouteWheel (const DxuiMouseEvent & ev);

    IPaneContentMouseHost  & m_host;
};
