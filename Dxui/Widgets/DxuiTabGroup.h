#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiPaneFrame.h"
#include "Widgets/DxuiTabStrip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup
//
//  A set of controls shown one at a time, with the tabs that pick between
//  them: a tab group of a docking layout, presented as Visual Studio presents
//  its groups.
//
//  - A DOCUMENT group has its tabs along its top, a close button on the
//    selected tab and on the tab under the pointer.
//  - A TOOL WINDOW group has a title bar along its top -- the active pane's
//    title, a menu button, a pin and a close button -- and, holding more
//    than one pane, its tabs along its bottom.
//
//  Each group has one outline, round the pane and its selected tab, in the
//  focus accent while the user works in the group and in the border color
//  otherwise. PaintFrame draws it, with the rounded corners and the joins
//  where the selected tab meets the line along its band (see DxuiPaneFrame),
//  after every sibling has painted, so it lies over the pane's controls.
//
//  THE TABS ARE A DxuiTabStrip, the strip Casso Explorer uses, in its document
//  or tool-window style; the group draws no tab of its own.
//
//  THE GROUP OWNS NOTHING BUT ITS STRIP. The controls belong to whatever
//  panel holds them; the group lays the active one out in its body and hides
//  the rest, the way DxuiSplitter lays out around its sash without owning
//  the panes. That is what lets a dock site move a control from one group to
//  another without either group copying it.
//
//  A tab can carry an indicator, a dot beside its title, for a pane whose
//  content changed while it was not shown, and a leading mark of its own
//  state ahead of its title.
//
//  Pressing a tab activates it; dragging a tab, or a tool window's title bar,
//  past the system's drag distance reports a drag start instead, so the dock
//  site can take the pane. Ctrl+Tab and Ctrl+Shift+Tab cycle while the group
//  has focus.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabGroup : public IDxuiControl
{
public:
    using ActivatedFn = std::function<void (int index)>;
    using DragFn      = std::function<void (int index, POINT pointDip)>;

    enum class Kind
    {
        Document,
        ToolWindow,
    };

    enum class TitleButton
    {
        Menu,
        Pin,
        Close,
    };

    using TitleButtonFn = std::function<void (TitleButton button, int index, POINT pointDip)>;
    using CloseTabFn    = std::function<void (int index)>;
    using CanCloseFn    = std::function<bool (int index)>;

    DxuiTabGroup();
    ~DxuiTabGroup() override = default;

    void  AddTab       (const std::wstring & title, IDxuiControl * content);
    bool  RemoveTab    (IDxuiControl * content);
    void  SetTitle     (IDxuiControl * content, const std::wstring & title);
    void  SetIndicator (IDxuiControl * content, bool on);
    void  SetTabTip    (IDxuiControl * content, const std::wstring & tip);

    //  Room a tab's pane keeps in a tool window's title bar, ahead of the
    //  menu button, for a control of its own; zero keeps none. Only the
    //  active tab's room is kept, and the title stops short of it.
    void  SetTitleExtra    (IDxuiControl * content, int widthDip);
    RECT  GetTitleExtraRect () const;

    //  A mark ahead of a tab's title: a mark of the tab's own state, not a
    //  change to look at. A glyph drawn in the given face, so a caller can
    //  repeat a mark its content already uses, or a dot when no glyph is
    //  given. A zero color is no mark.
    struct LeadingMark
    {
        std::wstring  glyph;
        std::wstring  face;
        uint32_t      argb = 0;

        bool  operator== (const LeadingMark &) const = default;
    };

    void  SetLeadingMark (IDxuiControl * content, const LeadingMark & mark);

    void  SetKind        (Kind kind);
    Kind  GetKind        () const { return m_kind; }

    //  The accent outline of the group the user is working in.
    void  SetFocusedLook (bool focused) { m_focusedLook = focused; }
    bool  HasFocusedLook () const       { return m_focusedLook; }

    size_t          GetTabCount   () const { return m_tabs.size(); }
    int             GetActive     () const { return m_active; }
    IDxuiControl *  GetContent    (int index) const;
    IDxuiControl *  GetActiveContent () const { return GetContent (m_active); }
    bool            HasIndicator  (int index) const;
    int             IndexOf       (IDxuiControl * content) const;

    //  Shows the tab's control and clears its indicator.
    void  SetActive    (int index);

    void  SetOnActivated   (ActivatedFn fn)   { m_onActivated   = std::move (fn); }
    void  SetOnDragStart   (DragFn fn)        { m_onDragStart   = std::move (fn); }
    void  SetOnTitleButton (TitleButtonFn fn) { m_onTitleButton = std::move (fn); }

    //  A drag of the title bar, which moves the whole group; with no handler
    //  it is reported to the drag start handler as a drag of the active tab.
    void  SetOnTitleDragStart (DragFn fn)     { m_onTitleDrag   = std::move (fn); }

    //  Forgets a press on the title bar or its buttons, for a drag another
    //  window took over: that window gets the button's release, not this one.
    void  CancelPress () { m_titlePressed = false; m_titleDragged = false; m_pressButton = -1; }

    //  Where the last press of a tab (`tab`) or of the title bar fell, from
    //  the top left of the tab or title bar it pressed.
    POINT  GetGrabOffset (bool tab) const;

    //  A tool window shows its tab strip even for a single pane, as a pane
    //  being torn off does while it is carried.
    void  SetStripForced (bool forced) { m_stripForced = forced; }

    //  A document tab's close button closes its pane; a tool window's close
    //  button shows only while its active pane can close.
    void  SetOnCloseTab    (CloseTabFn fn)    { m_onCloseTab    = std::move (fn); }
    void  SetCanClose      (CanCloseFn fn)    { m_canClose      = std::move (fn); }

    //  A + just past the last tab, as a browser puts one, shown while `shown`
    //  answers true and running `add` when pressed. Neither set: no +.
    using NewTabShownFn = std::function<bool (const DxuiTabGroup & group)>;
    using NewTabFn      = std::function<void (const DxuiTabGroup & group)>;
    void  SetNewTab      (NewTabShownFn shown, NewTabFn add);
    RECT  GetNewTabRect  () const;

    //  The tab under a point in the bounds' coordinates, or -1.
    int   HitTestTab   (POINT pointDip) const;
    RECT  GetTabRect   (int index) const;
    RECT  GetBodyRect  () const;
    RECT  GetTitleRect () const;
    RECT  GetStripRect () const;
    RECT  GetTitleButtonRect (TitleButton button) const;

    //  Whether a point is on the group's own chrome -- its title bar or its
    //  tabs -- rather than on the pane it shows.
    bool  IsChromeAt   (POINT pointDip) const;

    //  Where a tab dropped at a point lands among this group's tabs, and the
    //  gap opened there while it hovers (see DxuiTabStrip::SetInsertGap).
    //  A group showing no tabs takes a drop after its one tab.
    int   GetInsertIndexAt (POINT pointDip) const;
    void  SetInsertGap     (int index, int widthPx);
    RECT  GetInsertGapRect () const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Tabs"; }

    //  The outline and the corner caps, which the dock site paints after
    //  every sibling has painted, so they lie over the pane's controls
    //  whatever the child order.
    void  PaintFrame (IDxuiPainter & painter, const IDxuiTheme & theme) const;

    //  The tab band, without the line between it and the pane: Visual
    //  Studio's 31 px at 125%.
    static constexpr int  kStripDip       = 25;
    static constexpr int  kTitleDip       = 25;

    //  A title bar button's square: Visual Studio's 30-px pitch at 125%.
    static constexpr int  kTitleButtonDip = 24;
    static constexpr int  kTabPadDip      = 10;
    static constexpr int  kCharDip        = 7;
    static constexpr int  kIndicatorDip   = 10;
    static constexpr int  kDragDip        = 4;
    static constexpr int  kNewTabDip      = 24;

private:
    struct Tab
    {
        std::wstring    title;
        IDxuiControl  * content       = nullptr;
        bool            indicator     = false;
        LeadingMark     leadMark;
        std::wstring    tip;
        int             titleExtraDip = 0;
    };

    void  LayoutContent ();
    void  LayoutStrip   ();
    void  SyncStrip     ();
    bool  HasStrip      () const;
    bool  IsCloseShown  () const;
    int   GetTitleButtonAt (POINT pointDip) const;
    void  PaintTitle    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void  RunPending    ();

    DxuiPaneFrameSpec    GetFrameSpec   () const;
    DxuiPaneFrameColors  GetFrameColors (const IDxuiTheme & theme) const;

    std::vector<Tab>     m_tabs;
    int                  m_active        = -1;
    Kind                 m_kind          = Kind::Document;
    bool                 m_focusedLook   = false;
    bool                 m_stripForced   = false;
    uint32_t             m_indicatorArgb = 0;
    IDxuiTextRenderer  * m_measure       = nullptr;   // the renderer of the last paint, which outlives the group
    DxuiDpiScaler        m_scaler;
    DxuiTabStrip         m_strip;
    ActivatedFn          m_onActivated;
    DragFn               m_onDragStart;
    DragFn               m_onTitleDrag;
    TitleButtonFn        m_onTitleButton;
    CloseTabFn           m_onCloseTab;
    CanCloseFn           m_canClose;
    NewTabShownFn        m_newTabShown;
    NewTabFn             m_newTab;
    int                  m_pendingClose  = -1;
    bool                 m_pendingNewTab = false;
    int                  m_hoverButton   = -1;
    int                  m_pressButton   = -1;
    bool                 m_titlePressed  = false;
    bool                 m_titleDragged  = false;
    POINT                m_pressedAt     = {};
};
