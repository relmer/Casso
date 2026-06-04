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
        child->SetParent (this);
        m_children.push_back (std::move (child));
        MarkDirty();
    }
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
        if (it->get() == child)
        {
            (*it)->SetParent (nullptr);
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

    for (auto & child : m_children)
    {
        child->SetParent (nullptr);
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

    for (auto & child : m_children)
    {
        DxuiPanel *  childPanel = dynamic_cast<DxuiPanel *> (child.get());

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
        for (auto & child : m_children)
        {
            if (child->Visible())
            {
                visibleRaw.push_back (child.get());
            }
        }

        m_layout->Arrange (boundsDip,
                           scaler,
                           std::span<IDxuiControl * const> (visibleRaw.data(), visibleRaw.size()));
    }

    for (auto & child : m_children)
    {
        DxuiPanel *  childPanel = dynamic_cast<DxuiPanel *> (child.get());

        if (childPanel != nullptr && child->Visible())
        {
            childPanel->Layout (child->Bounds(), scaler);
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

    for (auto & child : m_children)
    {
        if (child->Visible())
        {
            child->Paint (painter, text, theme);
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
        IDxuiControl *  child = it->get();

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
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanel::OnKey (const DxuiKeyEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        IDxuiControl *  child = it->get();

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

    for (auto & child : m_children)
    {
        child->OnThemeChanged();
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

    for (auto & child : m_children)
    {
        child->Tick (nowMs);
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
