#include "Pch.h"

#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/DiskPortChainRule.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Layout/DriveLabel.h"
#include "Ui/Chrome/DriveWidget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::LayoutDriveBar
//
//  The slots are the compact Disk II widget's size at the bar's DPI, so the
//  widgets placed in them and the ones the control draws line up. The bar is
//  drawn at the window's DPI times the bar's zoom, not zoomed with the desk
//  scene: the scene zooms out to fit the desk, and the bar shrank with it.
//  While a class is closed, its last clicked drive is drawn in its widget.
//
//  With the drives auto-hidden (the auto-hide setting, or fullscreen), every
//  class is expanded, and the whole bar rises from the bottom edge as far as
//  the slide has come: the desk slide with the monitor, the strip without
//  it. The fans arrive with the widgets, and the bar is hidden while the
//  slide is down.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::LayoutDriveBar (int bottomInsetPx, int clientW, int clientH, UINT dpi)
{
    std::array<DriveWidget, 2>           & widgets     = m_shell.m_disks->GetDriveChrome();
    DriveBarFan                          & fan         = m_driveBarFan.GetFan();
    UINT                                   barDpi      = 0;
    DxuiDpiScaler                          scaler;
    DxuiDpiScaler                          unzoomed;
    RECT                                   probe       = {};
    SIZE                                   slot        = {};
    int                                    gap         = 0;
    int                                    bottomGap   = 0;
    int                                    gapY        = 0;
    int                                    pad         = 0;
    int                                    arrow       = 0;
    bool                                   sliding     = m_shell.AreDrivesAutoHidden();
    int                                    originX     = 0;
    int                                    rowTop      = 0;
    int                                    stripBottom = (m_driveStripAreaPx.bottom > m_driveStripAreaPx.top) ? (int) m_driveStripAreaPx.bottom : clientH;
    std::vector<std::vector<DeskDrive>>    classes     = DriveBarFan::Group (m_shell.m_scene->GetDrivePlan());



    m_driveBarDpi       = dpi;
    m_driveBarInsetPx   = bottomInsetPx;
    m_driveBarClientW   = clientW;
    m_driveBarClientH   = clientH;

    DriveBarFan::PutFirst (classes, m_driveBarRecent);
    fan.SetClasses (std::move (classes));
    SetDiskIiBarCaptions (true);
    m_driveBarFan.SetCaptionWidthDip (s_kDriveBarCaptionDip);

    // A box's width at 100%, which caps the zoom so every box fits.
    unzoomed.SetDpi (dpi);
    widgets[0].Layout (RECT {}, unzoomed);
    probe                = widgets[0].GetOuterRect();
    m_driveBarBoxWidthPx = (probe.right - probe.left) + MulDiv (2 * s_kDriveBarPadDp + s_kDriveBarArrowDp, (int) dpi, s_kBaseDpi);

    barDpi              = (UINT) lroundf ((float) dpi * GetDriveBarZoom());
    m_driveBarZoomedDpi = barDpi;
    bottomGap           = MulDiv (s_kLabelBottomGapDp,  (int) barDpi, s_kBaseDpi);
    gapY                = MulDiv (4,                    (int) barDpi, s_kBaseDpi);
    pad                 = MulDiv (s_kDriveBarPadDp,     (int) barDpi, s_kBaseDpi);
    arrow               = MulDiv (s_kDriveBarArrowDp,   (int) barDpi, s_kBaseDpi);
    scaler.SetDpi (barDpi);

    widgets[0].Layout (RECT {}, scaler);
    probe   = widgets[0].GetOuterRect();
    slot    = { probe.right - probe.left, probe.bottom - probe.top };
    gap     = DriveBarFan::GetFittingGap (fan.GetClassCount(), slot.cx, 2 * pad + arrow,
                                          MulDiv (s_kCompactDriveWidgetGapDp, (int) barDpi, s_kBaseDpi), clientW,
                                          MulDiv (s_kDriveBarEdgeDp,      (int) dpi, s_kBaseDpi),
                                          MulDiv (s_kDriveBarMinBoxGapDp, (int) dpi, s_kBaseDpi));
    originX = std::max (0, (clientW - DriveBarFan::GetRowWidth (fan.GetClassCount(), slot.cx, gap)) / 2);
    rowTop  = std::max (clientH - bottomInsetPx, clientH - (int) slot.cy - bottomGap);

    fan.SetExpandAll (sliding);
    fan.Layout       (originX, rowTop, slot, gap, gapY, pad, arrow);

    if (sliding)
    {
        fan.Close();
        fan.SetFanProgress (1.0f);

        // The band has no height under auto-hide, so the row is placed from
        // the bottom of the strip's area: above the //c's switch strip in a
        // window, which is drawn over anything under it, and the window's
        // bottom edge in fullscreen.
        rowTop = stripBottom - (int) slot.cy - bottomGap
               + (int) lroundf ((1.0f - m_driveBarSlideProgress) * (float) (fan.GetExpandedHeight() + pad + bottomGap));
        fan.Layout (originX, rowTop, slot, gap, gapY, pad, arrow);
    }

    m_driveBarFan.SetDpi     (barDpi);
    m_driveBarFan.SetBounds  (RECT { 0, 0, clientW, sliding ? stripBottom : clientH });
    m_driveBarFan.SetVisible (fan.GetClassCount() > 0 && (!sliding || m_driveBarSlideProgress > 0.0f));

    PlaceDriveBarWidgets();
    (void) SyncDriveBarViews();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SlideDriveBar
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SlideDriveBar (float progress)
{
    m_driveBarSlideProgress = progress;

    if (m_driveBarClientW > 0 && m_shell.m_scene->AreDrivesOnBar())
    {
        LayoutDriveBar (m_driveBarInsetPx, m_driveBarClientW, m_driveBarClientH, m_driveBarDpi);
        m_shell.m_scene->SyncSceneDriveChrome();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::IsPointOnDriveBar
//
//  In a class's box, and inside the bar's bounds, which end above the
//  //c's switch strip while the bar slides.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::IsPointOnDriveBar (POINT pt) const
{
    const DriveBarFan  & fan    = m_driveBarFan.GetFan();
    RECT                 bounds = m_driveBarFan.GetBounds();
    bool                 on     = false;



    for (int c = 0; c < fan.GetClassCount() && m_driveBarFan.IsVisible(); c++)
    {
        RECT  panel = fan.GetPanelRect (c);

        on = on || (PtInRect (&panel, pt) != FALSE && PtInRect (&bounds, pt) != FALSE);
    }

    return on;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::ZoomDriveBar
//
//  The band under the scene grows with the bar, so the whole window is laid
//  out again, as for a change of where the drives go. The zoom appears as a
//  percent for a moment, centered across the window and a little under a
//  fifth of its height above the bottom.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::ZoomDriveBar (POINT pt, float notches)
{
    GlobalUserPrefs  & prefs    = m_shell.m_settings->GetPrefs();
    float              zoom     = DriveBarFan::GetZoomAfterWheel (GetDriveBarZoom(), notches);
    float              fit      = GetDriveBarFitZoom();
    RECT               rcClient = {};
    int64_t            nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now().time_since_epoch()).count();
    int                middleY  = 0;
    int                halfH    = 0;



    if (!IsPointOnDriveBar (pt))
    {
        return false;
    }

    // Past the widest zoom that fits, the bar stays at the last step that
    // does. Zooming in never lowers a zoom saved in a wider window.
    if (zoom > fit)
    {
        zoom = std::floor (fit / DriveBarFan::kZoomStep + 0.001f) * DriveBarFan::kZoomStep;
    }

    if (notches > 0.0f && zoom < prefs.driveBarZoom)
    {
        zoom = prefs.driveBarZoom;
    }

    if (zoom != prefs.driveBarZoom)
    {
        prefs.driveBarZoom = zoom;
        m_shell.m_settings->SaveGlobalPrefs();

        if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &rcClient))
        {
            (void) m_shell.OnSize ((UINT) (rcClient.right - rcClient.left), (UINT) (rcClient.bottom - rcClient.top));
            m_shell.m_scene->SyncSceneDriveChrome();
            m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
        }
    }

    if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &rcClient))
    {
        middleY = rcClient.bottom - (int) lroundf ((float) rcClient.bottom * s_kDriveBarZoomReadoutRise);
        halfH   = m_shell.m_scaler.ToPx ((int) s_kDriveBarZoomReadoutFontDp);

        m_driveBarZoomReadout.SetText        (std::format (L"Widgets: {:.0f}%", GetDriveBarZoom() * 100.0f));
        m_driveBarZoomReadout.SetFontSizeDip (s_kDriveBarZoomReadoutFontDp);
        m_driveBarZoomReadout.SetAlign       (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        m_driveBarZoomReadout.SetDpi         (m_shell.m_scaler.GetDpi());
        m_driveBarZoomReadout.SetOpacity     (1.0f);
        m_driveBarZoomReadout.Layout         (RECT { 0, middleY - halfH, rcClient.right, middleY + halfH }, m_shell.m_scaler);
        m_driveBarZoomReadout.SetVisible     (true);

        m_driveBarZoomReadoutEndMs = nowMs + s_kDriveBarZoomReadoutMs;
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::TickDriveBarZoomReadout
//
//  Fades the zoom readout over its last moments and hides it at the end.
//  Returns whether it changed, so the frame is drawn again.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::TickDriveBarZoomReadout (int64_t nowMs)
{
    int64_t  leftMs  = m_driveBarZoomReadoutEndMs - nowMs;
    bool     changed = m_driveBarZoomReadoutEndMs != 0;



    if (m_driveBarZoomReadoutEndMs != 0 && leftMs <= 0)
    {
        m_driveBarZoomReadout.SetVisible (false);
        m_driveBarZoomReadoutEndMs = 0;
    }
    else if (m_driveBarZoomReadoutEndMs != 0)
    {
        m_driveBarZoomReadout.SetOpacity (std::min (1.0f, (float) leftMs / (float) s_kDriveBarZoomReadoutFadeMs));
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::GetDriveBarZoom
//
//  The saved zoom, clamped to the bar's limits in case the file was
//  edited, and to the widest zoom at which every class fits in the window.
//
////////////////////////////////////////////////////////////////////////////////

float ShellChrome::GetDriveBarZoom() const
{
    return std::min (DriveBarFan::GetZoomAfterWheel (m_shell.m_settings->GetPrefs().driveBarZoom, 0.0f), GetDriveBarFitZoom());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::GetDriveBarFitZoom
//
//  The widest zoom at which every class box on the bar fits across the
//  window, or kMaxZoom before the bar's first layout.
//
////////////////////////////////////////////////////////////////////////////////

float ShellChrome::GetDriveBarFitZoom() const
{
    int    classes = (int) DriveBarFan::Group (m_shell.m_scene->GetDrivePlan()).size();
    UINT   dpi     = m_shell.m_scaler.GetDpi();
    float  fit     = DriveBarFan::kMaxZoom;



    if (m_driveBarBoxWidthPx > 0 && m_lastClientWidthPx > 0)
    {
        fit = DriveBarFan::GetFittingZoom (classes, m_driveBarBoxWidthPx, m_lastClientWidthPx,
                                           MulDiv (s_kDriveBarEdgeDp,      (int) dpi, s_kBaseDpi),
                                           MulDiv (s_kDriveBarMinBoxGapDp, (int) dpi, s_kBaseDpi));
    }

    return fit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::HideDriveBar
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::HideDriveBar()
{
    m_driveBarFan.GetFan().Close();
    m_driveBarFan.SetVisible (false);
    SetDiskIiBarCaptions (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SetDiskIiBarCaptions
//
//  On the bar, each Disk II widget has the drive bar's two-line label;
//  off it, "DRIVE N" again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SetDiskIiBarCaptions (bool onBar)
{
    std::array<DriveWidget, 2>  & widgets = m_shell.m_disks->GetDriveChrome();
    DeskDriveModel                model   = m_shell.m_scene->IsMachineC() ? DeskDriveModel::Disk2c : DeskDriveModel::DiskII;



    for (int w = 0; w < (int) widgets.size(); w++)
    {
        DeskDrive  drive = { model, 6, w, m_shell.m_disks->MachineHasBuiltInDrive() && w == 0, DiskPortChainRule::IsOffered (m_shell.m_machine.GetConfig()) };

        if (onBar)
        {
            widgets[(size_t) w].SetBarCaption (DriveLabel::GetTitle (drive, false), DriveLabel::GetDetail (model, 0, 0), s_kDriveBarCaptionDip);
        }
        else
        {
            widgets[(size_t) w].SetBarCaption (std::wstring(), std::wstring(), 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::PlaceDriveBarWidgets
//
//  Each Disk II widget is placed in its drive's slot while the slot is
//  visible, and hidden otherwise. In a fan sliding out from behind the
//  widget, a Disk II appears once its slot is clear of the widget: its
//  widget draws over everything, so it cannot be cropped like the others.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::PlaceDriveBarWidgets()
{
    std::array<DriveWidget, 2>  & widgets = m_shell.m_disks->GetDriveChrome();
    const DriveBarFan           & fan     = m_driveBarFan.GetFan();
    DxuiDpiScaler                 scaler;



    scaler.SetDpi (m_driveBarZoomedDpi);

    for (int w = 0; w < (int) widgets.size(); w++)
    {
        RECT  slot  = {};
        bool  shown = false;



        for (int c = 0; c < fan.GetClassCount() && !shown; c++)
        {
            for (int i = 0; i < fan.GetEntryCount (c) && !shown; i++)
            {
                const DeskDrive  & drive = fan.GetDrive (c, i);

                if ((drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c) &&
                    drive.drive == w && fan.IsSlotShown (c, i) &&
                    (i == 0 || fan.GetSlotRect (c, i).bottom <= fan.GetRowTop()))
                {
                    slot  = fan.GetSlotRect (c, i);
                    shown = true;
                }
            }
        }

        if (shown && m_driveBarFan.IsVisible())
        {
            widgets[(size_t) w].SetVisible (true);
            widgets[(size_t) w].Layout (RECT { slot.left, slot.top, slot.left, slot.top }, scaler);
        }
        else
        {
            widgets[(size_t) w].SetVisible (false);
            widgets[(size_t) w].Hide();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::UpdateDriveBarHover
//
//  Returns true when the open fan or the drive under the pointer changed,
//  which requires a repaint.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::UpdateDriveBarHover (POINT pt)
{
    DriveBarFan  & fan     = m_driveBarFan.GetFan();
    bool           changed = false;
    int            c       = -1;
    int            entry   = -1;



    if (!m_driveBarFan.IsVisible())
    {
        return false;
    }

    if (fan.UpdateHover (pt))
    {
        if (fan.GetOpenClass() >= 0)
        {
            fan.SetFanProgress (0.0f);
            m_driveBarFanStartMs = GetTickCount64();
        }

        // A drive clicked while the fan was open is moved to the bottom of
        // its class once the fan has closed.
        if (fan.GetOpenClass() < 0 && m_driveBarRecentPending)
        {
            m_driveBarRecentPending = false;
            LayoutDriveBar (m_driveBarInsetPx, m_driveBarClientW, m_driveBarClientH, m_driveBarDpi);
        }

        PlaceDriveBarWidgets();
        m_shell.m_scene->SyncSceneDriveChrome();
        changed = true;
    }

    if (!fan.HitTest (pt, c, entry))
    {
        c     = -1;
        entry = -1;
    }

    if (c != m_driveBarHoverClass || entry != m_driveBarHoverEntry)
    {
        m_driveBarHoverClass = c;
        m_driveBarHoverEntry = entry;
        m_driveBarFan.SetHovered (c, entry);
        changed = true;
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::HandleDriveBarClick
//
//  A Disk II ejects and browses, as its widget does elsewhere; a 3.5-inch
//  drive or a hard disk browses. Either way the drive becomes its class's
//  last clicked drive, drawn in its widget once the class closes. A click
//  in a box between drives is taken and does nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::HandleDriveBarClick (POINT pt)
{
    const DriveBarFan  & fan   = m_driveBarFan.GetFan();
    int                  c     = -1;
    int                  entry = -1;
    bool                 took  = false;



    if (!m_driveBarFan.IsVisible())
    {
        return false;
    }

    if (fan.HitTest (pt, c, entry))
    {
        const DeskDrive  drive = fan.GetDrive (c, entry);

        MarkDriveBarRecent (pt);

        if (drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c)
        {
            m_shell.m_disks->Eject (6, drive.drive);
            m_shell.m_disks->BrowseForDisk (drive.drive);
        }
        else if (ShellDeskScene::TakesImage (drive))
        {
            m_shell.m_scene->BrowseDeskDrive (drive);
        }

        took = true;
    }
    else
    {
        took = IsPointOnDriveBar (pt);
    }

    return took;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::MarkDriveBarRecent
//
//  The drive at `pt` becomes its class's last clicked drive, drawn in the
//  class's widget once the class closes. A left click or a right click
//  marks it. A point off the bar's drives changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::MarkDriveBarRecent (POINT pt)
{
    const DriveBarFan  & fan   = m_driveBarFan.GetFan();
    int                  c     = -1;
    int                  entry = -1;



    if (m_driveBarFan.IsVisible() && fan.HitTest (pt, c, entry))
    {
        const DeskDrive  drive = fan.GetDrive (c, entry);

        std::erase_if (m_driveBarRecent, [&drive] (const DeskDrive & recent)
                       { return DeskDrivePlan::GetClass (recent.model) == DeskDrivePlan::GetClass (drive.model); });
        m_driveBarRecent.push_back (drive);
        m_driveBarRecentPending = entry != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::AnimateDriveBarFan
//
//  Slides an opening fan up from behind its widget. Returns true while it is
//  moving, which requires a repaint.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::AnimateDriveBarFan()
{
    DriveBarFan  & fan    = m_driveBarFan.GetFan();
    bool           moving = fan.GetOpenClass() >= 0 && !fan.IsExpandAll() && fan.GetFanProgress() < 1.0f;
    float          t      = 0.0f;



    if (moving)
    {
        t = (float) (GetTickCount64() - m_driveBarFanStartMs) / (float) s_kDriveBarFanSlideMs;
        t = std::min (1.0f, t);

        // Ease out, so the fan arrives rather than stopping.
        fan.SetFanProgress (1.0f - (1.0f - t) * (1.0f - t));
        PlaceDriveBarWidgets();

        if (t >= 1.0f)
        {
            m_shell.m_scene->SyncSceneDriveChrome();
        }
    }

    return moving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SyncDriveBarViews
//
//  Each drive's two-line label, image name and busy lamp. The label is
//  numbered with its ProDOS slot and drive when the class has two drives of
//  that kind (DriveLabel::GetTitle). Returns true when a label, name or lamp
//  changed.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::SyncDriveBarViews()
{
    const DriveBarFan                            & fan    = m_driveBarFan.GetFan();
    std::vector<std::vector<DriveBarEntryView>>    views;
    uint64_t                                       now    = (m_shell.m_machine.GetCpu() != nullptr) ? *m_shell.m_machine.GetCpu()->GetBusCyclePtr() : 0;
    uint64_t                                       window = m_shell.m_machine.GetConfig().clockSpeed / 10;
    bool                                           moved  = false;



    for (int c = 0; c < fan.GetClassCount(); c++)
    {
        std::vector<DriveBarEntryView>    row;
        std::map<DeskDriveModel, int>     kinds;



        for (int i = 0; i < fan.GetEntryCount (c); i++)
        {
            kinds[fan.GetDrive (c, i).model]++;
        }

        for (int i = 0; i < fan.GetEntryCount (c); i++)
        {
            const DeskDrive    & drive = fan.GetDrive (c, i);
            DriveBarEntryView    view;
            std::string          path;



            view.diskII = drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c;
            view.title  = DriveLabel::GetTitle  (drive, kinds[drive.model] > 1);
            view.detail = DriveLabel::GetDetail (drive.model, m_shell.m_scene->GetDriveDiskBlocks (drive), m_shell.m_scene->GetDriveCapacityBlocks (drive));

            if (!view.diskII)
            {
                path      = m_shell.m_scene->GetDriveImagePath (drive);
                view.name = path.empty() ? std::wstring() : fs::path (path).filename().wstring();
                view.busy = m_shell.m_scene->IsDriveBusy (drive, now, window);
            }

            row.push_back (std::move (view));
        }

        views.push_back (std::move (row));
    }

    moved = views.size() != m_driveBarViews.size();

    for (size_t c = 0; c < views.size() && !moved; c++)
    {
        moved = views[c].size() != m_driveBarViews[c].size();

        for (size_t i = 0; i < views[c].size() && !moved; i++)
        {
            moved = views[c][i].name  != m_driveBarViews[c][i].name  || views[c][i].busy   != m_driveBarViews[c][i].busy ||
                    views[c][i].title != m_driveBarViews[c][i].title || views[c][i].detail != m_driveBarViews[c][i].detail;
        }
    }

    if (moved)
    {
        m_driveBarViews = views;
        m_driveBarFan.SetViews (std::move (views));
    }

    return moved;
}
