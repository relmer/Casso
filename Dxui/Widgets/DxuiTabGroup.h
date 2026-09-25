#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
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
//  A group with focus shows a 1-pixel accent border and an accent outline on
//  its selected tab.
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

    //  The accent border and outline of the group the user is working in.
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

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Tabs"; }

    static constexpr int  kStripDip       = 28;
    static constexpr int  kTitleDip       = 24;
    static constexpr int  kTitleButtonDip = 22;
    static constexpr int  kTabPadDip      = 10;
    static constexpr int  kCharDip        = 7;
    static constexpr int  kIndicatorDip   = 10;
    static constexpr int  kDragDip        = 4;
    static constexpr int  kNewTabDip      = 24;

private:
    struct Tab
    {
        std::wstring    title;
        IDxuiControl  * content   = nullptr;
        bool            indicator = false;
        LeadingMark     leadMark;
        std::wstring    tip;
    };

    void  LayoutContent ();
    void  LayoutStrip   ();
    void  SyncStrip     ();
    bool  HasStrip      () const;
    bool  IsCloseShown  () const;
    int   GetTitleButtonAt (POINT pointDip) const;
    void  PaintTitle    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void  RunPending    ();
    void  PaintFrame    (IDxuiPainter & painter, uint32_t argb) const;

    std::vector<Tab>     m_tabs;
    int                  m_active        = -1;
    Kind                 m_kind          = Kind::Document;
    bool                 m_focusedLook   = false;
    uint32_t             m_indicatorArgb = 0;
    IDxuiTextRenderer  * m_measure       = nullptr;   // the renderer of the last paint, which outlives the group
    DxuiDpiScaler        m_scaler;
    DxuiTabStrip         m_strip;
    ActivatedFn          m_onActivated;
    DragFn               m_onDragStart;
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
