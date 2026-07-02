#include "Pch.h"

#include "Core/DxuiPanel.h"
#include "Core/DxuiThread.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanel
//
////////////////////////////////////////////////////////////////////////////////

DxuiPanel::DxuiPanel()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiPanel
//
////////////////////////////////////////////////////////////////////////////////

DxuiPanel::~DxuiPanel()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendChild
//
//  Internal helper used by Add<T>(). Sets the child's parent pointer
//  to this panel, then moves it into the children vector.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::AppendChild (std::unique_ptr<IDxuiControl> child)
{
    DXUI_ASSERT_UI_THREAD();

    if (child)
    {
        ChildSlot  slot;

        child->SetParent (this);
        slot.raw   = child.get();
        slot.owned = std::move (child);
        m_children.push_back (std::move (slot));
        MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Adopt
//
//  Registers a non-owned child. The panel includes it in paint /
//  input / focus / tick / theme / DPI walks but does NOT destroy it
//  when the panel is destroyed. Caller retains lifetime ownership.
//  Re-adopting the same pointer is a no-op; adopting a pointer
//  already owned by this panel asserts.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::Adopt (IDxuiControl & nonOwnedChild)
{
    ChildSlot  slot;


    DXUI_ASSERT_UI_THREAD();

    for (const auto & existing : m_children)
    {
        if (existing.raw == &nonOwnedChild)
        {
            _ASSERTE (existing.owned == nullptr && "Adopt called on a control already owned via Add<T>().");
            return;
        }
    }

    nonOwnedChild.SetParent (this);
    slot.raw = &nonOwnedChild;
    m_children.push_back (std::move (slot));
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveAdopted
//
//  Drops a previously adopted child by pointer match. Returns false
//  for children not present or for owned children (use Remove for
//  those). The pointer's lifetime is unaffected; only the panel's
//  participation reference is removed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanel::RemoveAdopted (IDxuiControl & child)
{
    DXUI_ASSERT_UI_THREAD();

    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (it->raw == &child && it->owned == nullptr)
        {
            it->raw->SetParent (nullptr);
            m_children.erase (it);
            MarkDirty();
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearAdopted
//
//  Drops every adopted entry. Owned children added via Add<T> are
//  left untouched.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::ClearAdopted ()
{
    DXUI_ASSERT_UI_THREAD();

    for (auto it = m_children.begin(); it != m_children.end(); )
    {
        if (it->owned == nullptr)
        {
            it->raw->SetParent (nullptr);
            it = m_children.erase (it);
        }
        else
        {
            ++it;
        }
    }

    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Remove
//
//  Drops the matching child. Returns false for null inputs or for
//  controls that aren't owned by this panel.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanel::Remove (IDxuiControl * child)
{
    DXUI_ASSERT_UI_THREAD();

    if (child == nullptr)
    {
        return false;
    }

    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (it->raw == child && it->owned != nullptr)
        {
            it->owned->SetParent (nullptr);
            m_children.erase (it);
            MarkDirty();
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::Clear()
{
    DXUI_ASSERT_UI_THREAD();

    for (auto & slot : m_children)
    {
        slot.raw->SetParent (nullptr);
    }
    m_children.clear();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetLayout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::SetLayout (std::unique_ptr<IDxuiLayout> layout)
{
    DXUI_ASSERT_UI_THREAD();

    m_layout = std::move (layout);
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PropagateDpi
//
//  Recursively notifies every child panel that the effective DPI has
//  changed. Non-panel children inherit DPI implicitly via their
//  parent's next Layout() call; panels in turn re-propagate.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::PropagateDpi (const DxuiDpiScaler & scaler)
{
    DXUI_ASSERT_UI_THREAD();

    m_lastScaler = scaler;
    m_haveLast   = (m_lastBoundsDip.right > m_lastBoundsDip.left);

    for (auto & slot : m_children)
    {
        DxuiPanel *  childPanel = dynamic_cast<DxuiPanel *> (slot.raw);

        if (childPanel != nullptr)
        {
            childPanel->PropagateDpi (scaler);
        }
    }

    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PropagateTheme
//
//  Recursively invokes OnThemeChanged() on every child.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::PropagateTheme()
{
    DXUI_ASSERT_UI_THREAD();

    OnThemeChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Stores the requested bounds and scaler then asks the layout policy
//  to arrange each child. A panel with no layout policy leaves child
//  bounds untouched (e.g., absolute positioning).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    std::vector<IDxuiControl *>  visibleRaw;


    DXUI_ASSERT_UI_THREAD();

    SetBounds (boundsDip);
    m_lastBoundsDip = boundsDip;
    m_lastScaler    = scaler;
    m_haveLast      = true;
    m_dirty         = false;

    if (m_layout)
    {
        visibleRaw.reserve (m_children.size());
        for (auto & slot : m_children)
        {
            if (slot.raw->Visible())
            {
                visibleRaw.push_back (slot.raw);
            }
        }

        m_layout->Arrange (boundsDip,
                           scaler,
                           std::span<IDxuiControl * const> (visibleRaw.data(), visibleRaw.size()));
    }

    //
    //  Propagate the layout pass to every visible child (not just child
    //  panels). A layout's Arrange positions children via SetBounds, which
    //  does NOT update a leaf widget's DPI scaler -- so DxuiLabel /
    //  DxuiButton fonts would render at the base 96-DPI size. Calling
    //  Layout on every child threads the real scaler down so leaf widgets
    //  scale their text; child panels recurse as before. Layout is cheap
    //  and idempotent (bounds were just set; this re-applies them plus the
    //  scaler), and IDxuiControl::Layout is implemented by every control.
    //
    for (auto & slot : m_children)
    {
        if (slot.raw->Visible())
        {
            slot.raw->Layout (slot.raw->Bounds(), scaler);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Fan out to visible children in insertion order (back-to-front so
//  later siblings paint on top).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DXUI_ASSERT_UI_THREAD();

    for (auto & slot : m_children)
    {
        if (slot.raw->Visible())
        {
            slot.raw->Paint (painter, text, theme);

#if defined (DXUI_DEBUG_BOUNDS)
            //
            //  Layout-debugging aid: outline every child's bounds so a
            //  screenshot reveals exact geometry (zero-size widgets,
            //  overflow, overlap). Compile with /D DXUI_DEBUG_BOUNDS.
            //
            {
                RECT  b = slot.raw->Bounds();

                painter.OutlineRect ((float) b.left,
                                     (float) b.top,
                                     (float) (b.right  - b.left),
                                     (float) (b.bottom - b.top),
                                     2.0f,
                                     0xFFFF00FF);
            }
#endif
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  Dispatch front-to-back (last-added child first). First child that
//  returns true consumes the event; subsequent children are skipped.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanel::OnMouse (const DxuiMouseEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        IDxuiControl *  child = it->raw;

        if (child->Visible() && child->Enabled())
        {
            if (child->OnMouse (ev))
            {
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CursorForPoint
//
//  Fans the cursor query front-to-back (last-added child first), mirroring
//  OnMouse. The first child that advertises a cursor wins; children that
//  have no preference return nullptr and the walk continues.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiPanel::CursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;


    DXUI_ASSERT_UI_THREAD();

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        const IDxuiControl *  child = it->raw;

        if (child->Visible())
        {
            cursor = child->CursorForPoint (clientPx);

            if (cursor != nullptr)
            {
                break;
            }
        }
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanel::OnKey (const DxuiKeyEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        IDxuiControl *  child = it->raw;

        if (child->Visible() && child->Enabled())
        {
            if (child->OnKey (ev))
            {
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnThemeChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::OnThemeChanged()
{
    DXUI_ASSERT_UI_THREAD();

    for (auto & slot : m_children)
    {
        slot.raw->OnThemeChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::Tick (int64_t nowMs)
{
    DXUI_ASSERT_UI_THREAD();

    for (auto & slot : m_children)
    {
        slot.raw->Tick (nowMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnVisibilityChanged
//
//  When this panel's own visibility changes, propagate dirty up to the
//  parent so the parent relayouts on next pump.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::OnVisibilityChanged()
{
    DxuiPanel *  parentPanel = dynamic_cast<DxuiPanel *> (Parent());


    MarkDirty();
    if (parentPanel != nullptr)
    {
        parentPanel->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChildVisibilityChanged
//
//  Called by IDxuiControl::SetVisible when a child toggles visibility.
//  Marks the panel dirty so the next pump relayouts.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::OnChildVisibilityChanged (IDxuiControl * /*child*/)
{
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDirty
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanel::MarkDirty()
{
    m_dirty = true;
}
