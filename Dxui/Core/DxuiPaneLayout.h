#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSide / DxuiPaneLayoutNode
//
//  A layout is a tree whose inner nodes split an area in two and whose leaves
//  are tab groups. A pane is always in a tab group; a group of one is a pane
//  on its own. A horizontal split puts `first` on the left of `second`, a
//  vertical one puts it above.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiDockSide
{
    Left,
    Top,
    Right,
    Bottom,
};

struct DxuiPaneLayoutNode
{
    enum class Kind { Split, Tabs };

    Kind                                 kind       = Kind::Tabs;

    //  Split.
    bool                                 horizontal = true;
    float                                ratio      = 0.5f;
    std::unique_ptr<DxuiPaneLayoutNode>  first;
    std::unique_ptr<DxuiPaneLayoutNode>  second;

    //  Tabs.
    std::vector<std::wstring>            panes;
    int                                  active     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout
//
//  Where every pane of a docking window is: in the tree, floating in a window
//  of its own, or hidden against an edge. Pure data, with no controls, so
//  every docking operation is tested without drawing anything; the dock site
//  makes its controls match whatever this says.
//
//  PANES ARE NAMED BY STABLE STRINGS the application chooses (`registers`,
//  `memory2`), which is what makes a saved layout meaningful in a later
//  session.
//
//  A PANE THAT LEAVES KEEPS ITS PLACE. A floating or auto-hidden pane
//  remembers the pane it sat beside, so docking it back returns it there. A
//  pane the application does not currently show (a device the machine lacks)
//  stays in the tree and is skipped when the tree is arranged, so it reopens
//  where it was when it is shown again.
//
//  THE TEXT FORM IS THE LIBRARY'S OWN: a version, then the tree as nested
//  lists. It is not JSON because the library has no JSON; an application
//  stores the text wherever it keeps its settings. Text that does not parse,
//  or carries another version, is refused, and the caller falls back to its
//  default layout.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPaneLayout
{
public:
    static constexpr int  kVersion = 1;

    struct Floating
    {
        std::wstring  pane;
        std::wstring  monitorKey;
        RECT          rectDip = {};
        std::wstring  homePane;
    };

    struct AutoHidden
    {
        std::wstring  pane;
        DxuiDockSide  edge = DxuiDockSide::Left;
        std::wstring  homePane;
    };

    //  One monitor, as the application identifies it.
    struct Monitor
    {
        std::wstring  key;
        RECT          workDip   = {};
        bool          isPrimary = false;
    };

    //  A tab group laid out: its panes, the one shown, and its area.
    struct GroupRect
    {
        std::vector<std::wstring>  panes;
        std::wstring               active;
        RECT                       rect = {};
    };

    using ShownFn   = std::function<bool (const std::wstring & pane)>;
    using MinSizeFn = std::function<SIZE (const std::wstring & pane)>;

    DxuiPaneLayout () = default;
    DxuiPaneLayout (const DxuiPaneLayout & other)             { CopyFrom (other); }
    DxuiPaneLayout & operator= (const DxuiPaneLayout & other) { CopyFrom (other); return *this; }

    //  A layout of one pane, the start of every default.
    static DxuiPaneLayout  MakeSingle (const std::wstring & pane);

    const DxuiPaneLayoutNode *       GetRoot       () const { return m_root.get(); }
    const std::vector<Floating> &    GetFloating   () const { return m_floating; }
    const std::vector<AutoHidden> &  GetAutoHidden () const { return m_autoHidden; }

    bool  IsDocked     (const std::wstring & pane) const;
    bool  IsFloating   (const std::wstring & pane) const;
    bool  IsAutoHidden (const std::wstring & pane) const;
    bool  Contains     (const std::wstring & pane) const;

    //  The panes tabbed together with `pane`, itself included, in tab order.
    std::vector<std::wstring>  GetGroup (const std::wstring & pane) const;

    //  Operations. Each returns false and changes nothing when it would not
    //  change the layout (a drop onto a pane's own place) or names a pane
    //  that is not there.
    bool  DockToSide  (const std::wstring & pane, const std::wstring & target, DxuiDockSide side);
    bool  DockToEdge  (const std::wstring & pane, DxuiDockSide side);
    bool  TabWith     (const std::wstring & pane, const std::wstring & target);
    bool  Float       (const std::wstring & pane, const std::wstring & monitorKey, const RECT & rectDip);
    bool  AutoHide    (const std::wstring & pane, DxuiDockSide edge);
    bool  DockBack    (const std::wstring & pane);
    bool  Activate    (const std::wstring & pane);
    bool  Close       (const std::wstring & pane);

    //  A new pane, tabbed with `target` when it is docked, else at the right
    //  edge.
    bool  Add         (const std::wstring & pane, const std::wstring & target);

    //  Keyboard docking: into the group whose area lies next in that direction,
    //  or against the window's edge when none does.
    bool  MoveByArrow (const std::wstring & pane, DxuiDockSide direction, const RECT & areaDip,
                       const ShownFn & shown, const MinSizeFn & minSize);

    //  Every shown group's area. A split gives each side at least what its
    //  panes need, as far as the area allows, before the ratio decides the
    //  rest; a side whose panes are all hidden gives its area to the other.
    std::vector<GroupRect>  Arrange (const RECT & areaDip, const ShownFn & shown, const MinSizeFn & minSize) const;

    //  Load-time repairs: panes the application does not know are dropped;
    //  floating panes on a monitor that is absent move to the primary's work
    //  area at their saved size.
    void  DropUnknown     (const std::function<bool (const std::wstring & pane)> & isKnown);
    void  PlaceOnMonitors (const std::vector<Monitor> & monitors);

    std::wstring  ToText () const;
    static bool   TryParse (const std::wstring & text, DxuiPaneLayout & out);

private:
    using Node = DxuiPaneLayoutNode;

    void   CopyFrom (const DxuiPaneLayout & other);

    static std::unique_ptr<Node>  CloneNode    (const Node * node);
    static std::unique_ptr<Node>  MakeTabs     (const std::wstring & pane);
    static Node *                 FindGroup    (Node * node, const std::wstring & pane);
    static bool                   RemoveFrom   (std::unique_ptr<Node> & slot, const std::wstring & pane);
    static bool                   HasShown     (const Node * node, const ShownFn & shown);
    static SIZE                   GetMinimum   (const Node * node, const ShownFn & shown, const MinSizeFn & minSize);
    static void                   ArrangeNode  (const Node * node, const RECT & area, const ShownFn & shown,
                                                const MinSizeFn & minSize, std::vector<GroupRect> & out);
    static void                   DropUnknownIn (std::unique_ptr<Node> & slot,
                                                 const std::function<bool (const std::wstring & pane)> & isKnown);
    static void                   WriteNode    (const Node * node, std::wstring & out);
    static std::unique_ptr<Node>  ReadNode     (const std::vector<std::wstring> & tokens, size_t & at);
    static std::vector<std::wstring>  Tokenize (const std::wstring & text);
    static const wchar_t *        GetSideName  (DxuiDockSide side);
    static bool                   TryGetSide   (const std::wstring & name, DxuiDockSide & side);

    bool   Detach       (const std::wstring & pane);
    void   Split        (Node & group, std::unique_ptr<Node> added, DxuiDockSide side);
    std::wstring  GetNeighbor (const std::wstring & pane) const;

    std::unique_ptr<Node>    m_root;
    std::vector<Floating>    m_floating;
    std::vector<AutoHidden>  m_autoHidden;
};
