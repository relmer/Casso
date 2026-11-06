#include "Pch.h"

#include "Shell/Components/ShellHistoryOverlays.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellHistory.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/EmulatorShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays::Layout
//
//  The tube is curved and drawn in perspective, so the scene's hit test
//  finds the glass the rewind bar lies along and the replay indicator is
//  placed at the top of. The search starts from the middle of the monitor,
//  which lands on the glass.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistoryOverlays::Layout (const RECT & viewportRect)
{
    m_screenRectPx = viewportRect;

    if (m_shell.CrtMonitorActive())
    {
        RewindBar::ScreenTestFn  isOnGlass = [this] (int x, int y) { return m_shell.m_scene->DeskSceneHit (x, y).target == SceneHitResult::Target::Glass; };
        const RECT             & monitor   = m_shell.m_scene->GetScene().Composition().monitorRectPx;
        POINT                    middle    = { (monitor.left + monitor.right) / 2, (monitor.top + monitor.bottom) / 2 };

        if (isOnGlass (middle.x, middle.y))
        {
            m_screenRectPx = RewindBar::FindScreen (middle, isOnGlass);
        }

        m_rewindBar.SetScreenTest (isOnGlass);
    }
    else
    {
        m_rewindBar.SetScreenTest (nullptr);
    }

    m_rewindBar.Layout     (viewportRect,   m_shell.m_scaler);
    m_branchPreview.Layout (m_screenRectPx, m_shell.m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays::Adopt
//
//  Over the picture, under the banners and the notices.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistoryOverlays::Adopt (DxuiPanel & root)
{
    root.Adopt (m_branchPreview);

    m_rewindBar.Adopt       (root);
    m_replayIndicator.Adopt (root);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays::Sync
//
//  Once a frame, from the per-frame UI upkeep.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistoryOverlays::Sync()
{
    m_rewindBar.Sync();

    SyncReplayIndicator();
    SyncBranchPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays::SetHidden
//
//  Hidden for a capture paint, and synced with history's current state again
//  once it ends.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistoryOverlays::SetHidden (bool hidden)
{
    m_isHidden = hidden;

    m_rewindBar.SetHidden (hidden);

    if (hidden)
    {
        m_replayIndicator.Hide();
        (void) m_branchPreview.SetPicture (nullptr);
    }
    else
    {
        SyncReplayIndicator();
        SyncBranchPreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays::SyncReplayIndicator
//
//  Its opacity pulses on the animation tick, which runs while it is drawn.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistoryOverlays::SyncReplayIndicator()
{
    ShellHistory  & history = *m_shell.m_history;
    RECT            client  = {};
    bool            isShown = false;
    int64_t         nowMs   = (int64_t) GetTickCount64();



    if (m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        m_replayIndicator.Hide();
        return;
    }

    isShown = ReplayIndicator::IsShown (history.GetBarThumbnails().IsBehindLive(), m_shell.m_cpuManager.IsPaused(), history.IsRewinding(), m_isHidden);

    m_replayIndicator.Sync (isShown, history.GetReplayIndicatorText(), m_screenRectPx, m_chrome.ComputeTopOverlayEdgePx (client), m_shell.m_scaler, nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays::SyncBranchPreview
//
//  The hovered fork row's picture over the screen, drawn again on request
//  when it or its place changes. Beside the rewind bar's open flyout, it is
//  placed beside the flyout, above the bar.
//
////////////////////////////////////////////////////////////////////////////////

void ShellHistoryOverlays::SyncBranchPreview()
{
    BranchPreview::Picture  picture   = m_isHidden ? nullptr : m_shell.m_history->GetHoveredBranchPicture();
    RECT                    bounds    = m_screenRectPx;
    RECT                    area      = {};
    RECT                    flyout    = {};
    POINT                   corner    = {};
    RECT                    before    = m_branchPreview.GetBounds();
    bool                    isChanged = false;



    if (picture != nullptr && picture->height > 0 && m_rewindBar.TryGetFlyoutRect (flyout))
    {
        corner = { flyout.left, flyout.top };
        ScreenToClient (m_shell.m_hwnd, &corner);
        OffsetRect (&flyout, corner.x - flyout.left, corner.y - flyout.top);

        area        = m_rewindBar.GetScreen();
        area.bottom = m_rewindBar.GetBounds().top;
        bounds      = BranchPreview::FitBeside (area, flyout, (float) picture->width / (float) picture->height,
                                                m_shell.m_scaler.ToPx (BranchPreview::kGapDp));
    }

    isChanged = m_branchPreview.SetPicture (std::move (picture));

    if (!EqualRect (&before, &bounds))
    {
        m_branchPreview.Layout (bounds, m_shell.m_scaler);
        isChanged = true;
    }

    if (isChanged)
    {
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }
}
