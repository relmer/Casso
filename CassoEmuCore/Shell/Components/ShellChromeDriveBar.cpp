#include "Pch.h"

#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellRenderer.h"
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
//  With auto-hide on the desk, the row rises from the bottom edge as far as
//  the desk slide has come, and the bar is hidden while the slide is down.
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::LayoutDriveBar (int bottomInsetPx, int clientW, int clientH, UINT dpi)
{
    std::array<DriveWidget, 2>  & widgets   = m_shell.m_disks->GetDriveChrome();
    DriveBarFan                 & fan       = m_driveBarFan.GetFan();
    DxuiDpiScaler                 scaler;
    RECT                          probe     = {};
    SIZE                          slot      = {};
    int                           gap       = MulDiv (s_kCompactDriveWidgetGapDp, (int) dpi, s_kBaseDpi);
    int                           bottomGap = MulDiv (s_kLabelBottomGapDp,        (int) dpi, s_kBaseDpi);
    int                           gapY      = MulDiv (4, (int) dpi, s_kBaseDpi);
    int                           pad       = MulDiv (6, (int) dpi, s_kBaseDpi);
    int                           originX   = 0;
    int                           rowTop    = 0;



    m_driveBarDpi     = dpi;
    m_driveBarInsetPx = bottomInsetPx;
    m_driveBarClientW = clientW;
    m_driveBarClientH = clientH;
    scaler.SetDpi (dpi);

    fan.SetClasses (DriveBarFan::Group (m_shell.m_scene->GetDrivePlan()));

    widgets[0].Layout (RECT {}, scaler);
    probe   = widgets[0].GetOuterRect();
    slot    = { probe.right - probe.left, probe.bottom - probe.top };
    originX = std::max (0, (clientW - DriveBarFan::GetRowWidth (fan.GetClassCount(), slot.cx, gap)) / 2);
    rowTop  = std::max (clientH - bottomInsetPx, clientH - (int) slot.cy - bottomGap);

    if (m_shell.IsDeskSlideActive())
    {
        rowTop = clientH - (int) lroundf (m_deskSlideProgress * (float) (slot.cy + bottomGap));
    }

    fan.Layout (originX, rowTop, slot, gap, gapY, pad);

    if (m_shell.IsDeskSlideActive() && m_deskSlideProgress <= 0.0f)
    {
        fan.Close();
    }

    m_driveBarFan.SetDpi     (dpi);
    m_driveBarFan.SetBounds  (RECT { 0, 0, clientW, clientH });
    m_driveBarFan.SetVisible (fan.GetClassCount() > 0 && (!m_shell.IsDeskSlideActive() || m_deskSlideProgress > 0.0f));

    PlaceDriveBarWidgets();
    (void) SyncDriveBarViews();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SlideDriveBar
//
////////////////////////////////////////////////////////////////////////////////

void ShellChrome::SlideDriveBar()
{
    if (m_driveBarClientW > 0 && m_shell.m_scene->AreDrivesOnBar() && !m_shell.m_renderer->GetD3D().IsFullscreen())
    {
        LayoutDriveBar (m_driveBarInsetPx, m_driveBarClientW, m_driveBarClientH, m_driveBarDpi);
        m_shell.m_scene->SyncSceneDriveChrome();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::IsPointOnDriveBar
//
//  On a visible drive's slot or on the open fan's panel.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::IsPointOnDriveBar (POINT pt) const
{
    const DriveBarFan  & fan   = m_driveBarFan.GetFan();
    RECT                 panel = fan.GetPanelRect();
    int                  c     = -1;
    int                  entry = -1;



    return m_driveBarFan.IsVisible() && (fan.HitTest (pt, c, entry) || PtInRect (&panel, pt) != FALSE);
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::PlaceDriveBarWidgets
//
//  Each Disk II widget is placed in its drive's slot while the slot is
//  visible, and hidden otherwise.
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
                    drive.drive == w && fan.IsSlotShown (c, i))
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
//  drive browses. A click on a hard disk, or on the panel between drives, is
//  taken and does nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellChrome::HandleDriveBarClick (POINT pt)
{
    const DriveBarFan  & fan   = m_driveBarFan.GetFan();
    RECT                 panel = fan.GetPanelRect();
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

        took = true;
    }
    else
    {
        took = PtInRect (&panel, pt) != FALSE;
    }

    return took;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellChrome::SyncDriveBarViews
//
//  Each drive's caption, image name and busy lamp. A caption is the drive's
//  kind, numbered when the class has more than one of that kind; an HD20SC's
//  is its SCSI ID. Returns true when a caption, name or lamp changed.
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
        std::vector<DriveBarEntryView>  row;
        int                             floppy35Count = 0;
        int                             floppy35Index = 0;



        for (int i = 0; i < fan.GetEntryCount (c); i++)
        {
            floppy35Count += ShellDeskScene::IsFloppy35 (fan.GetDrive (c, i)) ? 1 : 0;
        }

        for (int i = 0; i < fan.GetEntryCount (c); i++)
        {
            const DeskDrive    & drive = fan.GetDrive (c, i);
            DriveBarEntryView    view;
            std::string          path;



            switch (drive.model)
            {
                case DeskDriveModel::DiskII:
                case DeskDriveModel::Disk2c:
                    view.diskII = true;
                    break;

                case DeskDriveModel::HardDisk20SC:
                    view.caption = std::format (L"ID {}", drive.drive);
                    break;

                case DeskDriveModel::ProFile:
                    view.caption = L"PROFILE";
                    break;

                default:
                    floppy35Index++;
                    view.caption = (floppy35Count > 1) ? std::format (L"3.5 #{}", floppy35Index) : std::wstring (L"3.5");
                    break;
            }

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
            moved = views[c][i].name != m_driveBarViews[c][i].name || views[c][i].busy != m_driveBarViews[c][i].busy ||
                    views[c][i].caption != m_driveBarViews[c][i].caption;
        }
    }

    if (moved)
    {
        m_driveBarViews = views;
        m_driveBarFan.SetViews (std::move (views));
    }

    return moved;
}
