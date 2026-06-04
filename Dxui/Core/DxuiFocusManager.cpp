#include "Pch.h"

#include "Core/DxuiFocusManager.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiThread.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusManager
//
////////////////////////////////////////////////////////////////////////////////

DxuiFocusManager::DxuiFocusManager ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiFocusManager
//
////////////////////////////////////////////////////////////////////////////////

DxuiFocusManager::~DxuiFocusManager ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Attach
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::Attach (DxuiPanel * root)
{
    DXUI_ASSERT_UI_THREAD();

    m_root = root;
    m_scopes.clear();
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::SetTheme (const IDxuiTheme * theme)
{
    DXUI_ASSERT_UI_THREAD();

    m_theme = theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowEpsilonDip
//
//  Returns the explicit test-seam override if set, otherwise pulls
//  BodyLineHeightDip() from the attached theme, otherwise falls back
//  to a hard-coded constant.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiFocusManager::RowEpsilonDip () const
{
    constexpr float  s_kDefaultRowEpsilonDip = 16.0f;


    if (m_rowEpsilonOverridden)
    {
        return m_rowEpsilonOverrideDip;
    }

    if (m_theme != nullptr)
    {
        return m_theme->BodyLineHeightDip();
    }

    return s_kDefaultRowEpsilonDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectFocusables
//
//  Depth-first walk of the visible / enabled subtree. Skips
//  kTabIndexExcluded controls and any control marked !Visible /
//  !Enabled / !Focusable.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::CollectFocusables (IDxuiControl * root, std::vector<IDxuiControl *> & out) const
{
    if (root == nullptr)
    {
        return;
    }

    if (!root->Visible() || !root->Enabled())
    {
        return;
    }

    if (root->Focusable() && root->TabIndex() != IDxuiControl::kTabIndexExcluded)
    {
        out.push_back (root);
    }

    for (size_t i = 0; i < root->ChildCount(); ++i)
    {
        CollectFocusables (root->Child (i), out);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Rebuild
//
//  Rebuilds the tab order. Controls with explicit non-negative
//  TabIndex() values sort first by ascending index. Remaining
//  geometry-mode controls sort by (top / rowEpsilon, left).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::Rebuild ()
{
    std::vector<IDxuiControl *>  raw;
    IDxuiControl *               scopeRoot = nullptr;
    float                        eps       = 1.0f;


    DXUI_ASSERT_UI_THREAD();

    m_tabOrder.clear();

    if (m_root == nullptr)
    {
        return;
    }

    scopeRoot = m_scopes.empty() ? static_cast<IDxuiControl *> (m_root) : m_scopes.back().root;
    if (scopeRoot == nullptr)
    {
        scopeRoot = m_root;
    }

    CollectFocusables (scopeRoot, raw);

    eps = RowEpsilonDip();
    if (eps <= 0.0f)
    {
        eps = 1.0f;
    }

    std::sort (raw.begin(), raw.end(),
        [eps] (IDxuiControl * a, IDxuiControl * b) -> bool
        {
            int  taIdx = a->TabIndex();
            int  tbIdx = b->TabIndex();
            bool aExpl = (taIdx >= 0);
            bool bExpl = (tbIdx >= 0);

            if (aExpl && bExpl)
            {
                return taIdx < tbIdx;
            }
            if (aExpl != bExpl)
            {
                return aExpl;  // explicit indices come first
            }
            RECT  ra = a->Bounds();
            RECT  rb = b->Bounds();
            int   ba = (int) ((float) ra.top / eps);
            int   bb = (int) ((float) rb.top / eps);
            if (ba != bb)
            {
                return ba < bb;
            }
            return ra.left < rb.left;
        });

    m_tabOrder = std::move (raw);

    // Drop focus if previously-focused control is no longer in the order.
    if (m_focused != nullptr)
    {
        bool  stillThere = false;

        for (IDxuiControl * ctl : m_tabOrder)
        {
            if (ctl == m_focused)
            {
                stillThere = true;
                break;
            }
        }
        if (!stillThere)
        {
            m_focused = nullptr;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocused
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::SetFocused (IDxuiControl * ctl)
{
    IDxuiControl *  prior = m_focused;


    DXUI_ASSERT_UI_THREAD();

    if (prior == ctl)
    {
        return;
    }

    m_focused = ctl;

    if (prior != nullptr)
    {
        prior->OnFocusChanged (false);
    }
    if (ctl != nullptr)
    {
        ctl->OnFocusChanged (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveFocus
//
//  Advances focus by +1 (Tab) or -1 (Shift+Tab) through the tab
//  order. Wraps at both ends.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::MoveFocus (int direction)
{
    size_t  count = m_tabOrder.size();
    size_t  idx   = 0;
    size_t  cur   = count;
    size_t  next  = 0;


    if (count == 0)
    {
        return false;
    }

    for (idx = 0; idx < count; ++idx)
    {
        if (m_tabOrder[idx] == m_focused)
        {
            cur = idx;
            break;
        }
    }

    if (cur == count)
    {
        // No current focus -- pick the first (forward) or last (backward).
        next = (direction > 0) ? 0 : (count - 1);
    }
    else if (direction > 0)
    {
        next = (cur + 1) % count;
    }
    else
    {
        next = (cur == 0) ? (count - 1) : (cur - 1);
    }

    SetFocused (m_tabOrder[next]);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveFocusSpatial
//
//  Spatial arrow navigation: picks the nearest focusable in the
//  arrow's direction using bounding-box centroids. Distance is the
//  squared Euclidean distance between centroids; candidates that are
//  not in the correct half-plane are filtered out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::MoveFocusSpatial (DxuiFocusKey arrow)
{
    IDxuiControl *  best     = nullptr;
    long            bestDist = 0;
    RECT            curR     = {};
    long            curCx    = 0;
    long            curCy    = 0;


    if (m_focused == nullptr)
    {
        return MoveFocus (+1);
    }

    curR  = m_focused->Bounds();
    curCx = (curR.left + curR.right)  / 2;
    curCy = (curR.top  + curR.bottom) / 2;

    for (IDxuiControl * candidate : m_tabOrder)
    {
        RECT  rr   = {};
        long  cx   = 0;
        long  cy   = 0;
        long  dx   = 0;
        long  dy   = 0;
        long  dist = 0;
        bool  keep = false;

        if (candidate == m_focused)
        {
            continue;
        }
        rr = candidate->Bounds();
        cx = (rr.left + rr.right)  / 2;
        cy = (rr.top  + rr.bottom) / 2;
        dx = cx - curCx;
        dy = cy - curCy;

        switch (arrow)
        {
        case DxuiFocusKey::ArrowLeft:   keep = (dx < 0); break;
        case DxuiFocusKey::ArrowRight:  keep = (dx > 0); break;
        case DxuiFocusKey::ArrowUp:     keep = (dy < 0); break;
        case DxuiFocusKey::ArrowDown:   keep = (dy > 0); break;
        default:                        keep = false;    break;
        }

        if (!keep)
        {
            continue;
        }

        dist = dx * dx + dy * dy;
        if (best == nullptr || dist < bestDist)
        {
            best     = candidate;
            bestDist = dist;
        }
    }

    if (best != nullptr)
    {
        SetFocused (best);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::HandleKey (DxuiFocusKey key)
{
    DXUI_ASSERT_UI_THREAD();

    switch (key)
    {
    case DxuiFocusKey::Tab:
        return MoveFocus (+1);
    case DxuiFocusKey::ShiftTab:
        return MoveFocus (-1);
    case DxuiFocusKey::ArrowUp:
    case DxuiFocusKey::ArrowDown:
    case DxuiFocusKey::ArrowLeft:
    case DxuiFocusKey::ArrowRight:
        return MoveFocusSpatial (key);
    case DxuiFocusKey::Escape:
        if (!m_scopes.empty())
        {
            PopScope();
            return true;
        }
        return false;
    case DxuiFocusKey::Enter:
    case DxuiFocusKey::Space:
        return false;     // activation routed via the focused control's OnKey
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushScope
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::PushScope (IDxuiControl * scopeRoot)
{
    Scope  scope;


    DXUI_ASSERT_UI_THREAD();

    scope.root       = scopeRoot;
    scope.priorFocus = m_focused;
    m_scopes.push_back (scope);
    m_focused = nullptr;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopScope
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::PopScope ()
{
    Scope  scope;


    DXUI_ASSERT_UI_THREAD();

    if (m_scopes.empty())
    {
        return;
    }

    scope = m_scopes.back();
    m_scopes.pop_back();
    Rebuild();
    SetFocused (scope.priorFocus);
}
