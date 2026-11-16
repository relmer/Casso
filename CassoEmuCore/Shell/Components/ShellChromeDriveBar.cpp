#include "Pch.h"

#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Layout/DriveLabel.h"
#include "Ui/Chrome/DriveWidget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::LayoutDriveBar
//
//  The slots are the compact Disk II widget's size at the bar's DPI, so the
//  widgets placed in them and the ones the control draws line up. The bar is
//  drawn at the window's DPI, not zoomed with the desk scene: the scene
//  zooms out to fit the desk, and the bar shrank with it.
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
    std::array<DriveWidget, 2>  & widgets     = m_shell.m_disks->GetDriveChrome();
    DriveBarFan                 & fan         = m_driveBarFan.GetFan();
    DxuiDpiScaler                 scaler;
    RECT                          probe       = {};
    SIZE                          slot        = {};
    int                           gap         = MulDiv (s_kCompactDriveWidgetGapDp, (int) dpi, s_kBaseDpi);
    int                           bottomGap   = MulDiv (s_kLabelBottomGapDp,        (int) dpi, s_kBaseDpi);
    int                           gapY        = MulDiv (4, (int) dpi, s_kBaseDpi);
    int                           pad         = MulDiv (6, (int) dpi, s_kBaseDpi);
    int                           arrow       = MulDiv (14, (int) dpi, s_kBaseDpi);
    bool                          sliding     = m_shell.AreDrivesAutoHidden();
    int                           originX     = 0;
    int                           rowTop      = 0;
    int                           stripBottom = (m_driveStripAreaPx.bottom > m_driveStripAreaPx.top) ? (int) m_driveStripAreaPx.bottom : clientH;



    m_driveBarDpi     = dpi;
    m_driveBarInsetPx = bottomInsetPx;
    m_driveBarClientW = clientW;
    m_driveBarClientH = clientH;
    scaler.SetDpi (dpi);

    fan.SetClasses (DriveBarFan::Group (m_shell.m_scene->GetDrivePlan()));
    SetDiskIiBarCaptions (true);
    m_driveBarFan.SetCaptionWidthDip (s_kDriveBarCaptionDip);

    widgets[0].Layout (RECT {}, scaler);
    probe   = widgets[0].GetOuterRect();
    slot    = { probe.right - probe.left, probe.bottom - probe.top };
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

    m_driveBarFan.SetDpi     (dpi);
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
        DeskDrive  drive = { model, 6, w, m_shell.m_disks->MachineHasBuiltInDrive() && w == 0 };

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



    scaler.SetDpi (m_driveBarDpi);

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
//  drive or a hard disk browses. A click in a box between drives is taken
//  and does nothing.
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
        const DeskDrive  & drive = fan.GetDrive (c, entry);
        RECT               slot  = fan.GetSlotRect (c, entry);

        if (drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c)
        {
            m_shell.m_disks->Eject (6, drive.drive);
            m_shell.m_disks->BrowseForDisk (drive.drive, &slot);
        }
        else if (ShellDeskScene::IsFloppy35 (drive))
        {
            m_shell.m_scene->BrowseFloppy35 (drive);
        }
        else if (ShellDeskScene::IsHardDisk (drive))
        {
            m_shell.BrowseHardDisk (drive);
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
