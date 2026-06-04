#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusManager
//
//  Owns the focus walk for a panel tree. Build order:
//      1. Attach a root panel.
//      2. Call Rebuild() after the tree shape or visibility changes
//         (DxuiPanel marks itself dirty; the host calls Rebuild() on
//         the next message-pump tick when m_dirty is set).
//      3. Dispatch input keys via HandleKey().
//
//  Tab order is geometry-based (reading order across the bounding-
//  rect row buckets, ties broken by left edge) unless a control sets
//  an explicit non-negative TabIndex(). Sentinels:
//      kTabIndexGeometry (-1): use geometry order.
//      kTabIndexExcluded (-2): skip Tab traversal entirely; the
//          control remains mouse-focusable but never receives Tab.
//
//  Focus scopes: PushScope(root) saves the current focus and restricts
//  subsequent tab walks to root's subtree. PopScope() restores the
//  prior focus and scope.
//
//  RowEpsilonDip() defaults to IDxuiTheme::BodyLineHeightDip() and
//  collapses rows whose `top` values differ by less than the epsilon
//  into a single reading-order band. SetRowEpsilonDip() is a test seam.
//
////////////////////////////////////////////////////////////////////////////////



class IDxuiTheme;
class DxuiPanel;



enum class DxuiFocusKey
{
    Tab,
    ShiftTab,
    Escape,
    Enter,
    Space,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
};



class DxuiFocusManager
{
public:
    DxuiFocusManager  ();
    ~DxuiFocusManager();

    void   Attach           (DxuiPanel * root);
    void   SetTheme         (const IDxuiTheme * theme);
    void   Rebuild          ();
    bool   HandleKey        (DxuiFocusKey key);

    void   PushScope        (IDxuiControl * scopeRoot);
    void   PopScope         ();

    IDxuiControl *  Focused() const                  { return m_focused; }
    void            SetFocused (IDxuiControl * ctl);

    float  RowEpsilonDip    () const;
    void   SetRowEpsilonDip (float epsilonDip)        { m_rowEpsilonOverrideDip = epsilonDip; m_rowEpsilonOverridden = true; }

    size_t TabOrderCount    () const                  { return m_tabOrder.size(); }
    IDxuiControl *  TabOrderAt (size_t index) const   { return (index < m_tabOrder.size()) ? m_tabOrder[index] : nullptr; }

private:
    struct Scope
    {
        IDxuiControl *  root          = nullptr;
        IDxuiControl *  priorFocus    = nullptr;
    };


    void   CollectFocusables (IDxuiControl * root, std::vector<IDxuiControl *> & out) const;
    bool   MoveFocus         (int direction);   // +1 forward, -1 backward
    bool   MoveFocusSpatial  (DxuiFocusKey arrow);

    DxuiPanel *                  m_root                 = nullptr;
    const IDxuiTheme *           m_theme                = nullptr;
    std::vector<IDxuiControl *>  m_tabOrder;
    IDxuiControl *               m_focused              = nullptr;
    std::vector<Scope>           m_scopes;
    float                        m_rowEpsilonOverrideDip = 0.0f;
    bool                         m_rowEpsilonOverridden  = false;
};
