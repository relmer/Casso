#include "Pch.h"

#include "Core/DxuiViewport.h"
#include "Core/DxuiThread.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::SetSizePolicy
//
//  Selects how an enclosing layout decides this viewport's bounds.
//  `Fixed` clamps to `PreferredSizeDip()`. `Preferred` offers the
//  preferred size but allows the parent layout to stretch. `Fill` (the
//  default) reports no preference and accepts whatever the parent
//  assigns.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::SetSizePolicy (SizePolicy policy)
{
    DXUI_ASSERT_UI_THREAD();

    m_policy = policy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::SetPreferredSizeDip
//
//  Records the preferred size in DIPs. Only consulted by enclosing
//  layouts when the policy is `Fixed` or `Preferred`; ignored in
//  `Fill` mode.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::SetPreferredSizeDip (SIZE sizeDip)
{
    DXUI_ASSERT_UI_THREAD();

    m_preferredSizeDip = sizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::SetConsumesInput
//
//  Toggles whether the viewport forwards mouse / key events to the
//  registered input sink. When `true` the viewport also marks itself
//  focusable so focus traversal can land on it; when `false` it
//  reverts to non-focusable and swallows nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::SetConsumesInput (bool consumesInput)
{
    DXUI_ASSERT_UI_THREAD();

    m_consumesInput = consumesInput;
    SetFocusable (consumesInput);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::SetInputSink
//
//  Installs the consumer-supplied input sink. Passing `nullptr`
//  detaches the sink. The viewport keeps a non-owning pointer; the
//  caller must outlive the viewport or call `SetInputSink(nullptr)`
//  before the sink is destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::SetInputSink (IDxuiViewportInputSink * sink)
{
    DXUI_ASSERT_UI_THREAD();

    m_sink = sink;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::IsReservedChord
//
//  Returns true for the keystrokes that Dxui reserves for itself even
//  when a viewport is in `consumesInput` mode. The framework needs
//  these chords to drive focus traversal (Tab / Shift+Tab), dismiss
//  modal surfaces (Esc), and activate the menu bar (Alt-alone, F10).
//
//  Modifier-bearing variants (Ctrl+Tab, Ctrl+Esc, Alt+F10, etc.) are
//  NOT reserved -- those forward to the sink so the hosted app can
//  bind them to its own actions.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiViewport::IsReservedChord (const DxuiKeyEvent & ev)
{
    // Char events never count as reserved chords; only key transitions do.
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return false;
    }

    // Tab / Shift+Tab (no Ctrl) -- focus traversal.
    if (ev.vk == VK_TAB && !ev.ctrl)
    {
        return true;
    }

    // Esc (no Ctrl) -- cancel modal surfaces / drop focus.
    if (ev.vk == VK_ESCAPE && !ev.ctrl)
    {
        return true;
    }

    // F10 (no Alt) -- menu activation.
    if (ev.vk == VK_F10 && !ev.alt)
    {
        return true;
    }

    // Alt-alone -- menu activation.
    if (ev.vk == VK_MENU)
    {
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::Layout
//
//  Stores the new bounds rectangle on the IDxuiControl base, then
//  fires the bounds-changed callback when the rectangle differs from
//  the last value we notified for. The first call always fires so the
//  external renderer learns the initial geometry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::Layout (
    const RECT          & boundsDip,
    const DxuiDpiScaler & scaler)
{
    bool  changed = false;

    DXUI_ASSERT_UI_THREAD();

    (void) scaler;
    SetBounds (boundsDip);

    changed = !m_hasNotifiedBounds
           || boundsDip.left   != m_lastNotifiedBoundsDip.left
           || boundsDip.top    != m_lastNotifiedBoundsDip.top
           || boundsDip.right  != m_lastNotifiedBoundsDip.right
           || boundsDip.bottom != m_lastNotifiedBoundsDip.bottom;

    if (changed)
    {
        m_lastNotifiedBoundsDip = boundsDip;
        m_hasNotifiedBounds     = true;
        if (m_onBoundsChanged)
        {
            m_onBoundsChanged (boundsDip);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::Paint
//
//  No-op. The external renderer (e.g. the Apple ][ framebuffer
//  D3D pass) draws into the same swap chain at the rectangle reported
//  by `Bounds()`. Chrome above the viewport paints on top through the
//  normal control-tree fanout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiViewport::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme)
{
    DXUI_ASSERT_UI_THREAD();

    (void) painter;
    (void) text;
    (void) theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::OnMouse
//
//  Forwards mouse events to the sink when `consumesInput` is set and a
//  sink is registered. Otherwise the event is left unconsumed so
//  parent panels and the host window can handle it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiViewport::OnMouse (const DxuiMouseEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_consumesInput && m_sink != nullptr)
    {
        return m_sink->OnViewportMouse (ev);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::OnKey
//
//  Forwards non-reserved keys to the sink when `consumesInput` is set
//  and a sink is registered. Reserved chords (Tab / Shift+Tab / Esc /
//  Alt-alone / F10) return `false` so the framework's focus manager,
//  menu bar, and modal surfaces can act on them.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiViewport::OnKey (const DxuiKeyEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    if (!m_consumesInput || m_sink == nullptr)
    {
        return false;
    }

    if (IsReservedChord (ev))
    {
        return false;
    }

    return m_sink->OnViewportKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport::ClassifyHit
//
//  Always reports `Client`. The viewport is a transparent rectangle as
//  far as the host window's hit-testing is concerned: caption /
//  resize-edge classification belongs to chrome controls, not the
//  externally-rendered region in the middle.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiViewport::ClassifyHit (POINT clientDip) const
{
    (void) clientDip;
    return DxuiHitTestKind::Client;
}
