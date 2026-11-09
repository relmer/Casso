#include "Pch.h"
#include "Ui/DiskInspector/DiskInspectorWindow.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/CompareDialog.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Ui/DiskInspector/Views/DifferencesView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"
#include "Seams/Win32DiskFileIo.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Seams/Win32HostDialogs.h"
#include "Ui/DiskInspector/ExportDialog.h"
#include "Ui/DiskInspector/FileMapGridView.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Components/ComparisonPart.h"
#include "Ui/DiskInspector/Components/EditingPart.h"
#include "Ui/DiskInspector/Components/FindPart.h"
#include "Ui/DiskInspector/Components/HostLink.h"
#include "Ui/DiskInspector/Components/ScreenNotes.h"
#include "Ui/DiskInspector/Components/WalkthroughPart.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/Components/SelectionPart.h"
#include "Ui/DiskInspector/Components/ViewTables.h"
#include "Ui/DiskInspector/Components/InspectorMenus.h"
#include "Ui/DiskInspector/Components/InspectorToolbar.h"
#include "Ui/DiskInspector/Components/PlatterCorners.h"
#include "Ui/DiskInspector/Components/StatusBand.h"
#include "Ui/DiskInspector/Components/StripControls.h"
#include "Ui/DiskInspector/Components/TrackColumn.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/FindBar.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/DiskInspector/Views/FluxView.h"
#include "Ui/DiskInspector/GoToDialog.h"
#include "Ui/DiskInspector/Views/NibblesView.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/Views/SectorsView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/ZoomStops.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SetZoom
//
//  A zoom above 100% stops where the content's controls would collide. The
//  window's size and position stay as they are, and its minimum size follows
//  the zoom up to its present size.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SetZoom (double zoom)
{
    static constexpr double  kStep         = 0.01;
    static constexpr int     kNoteMs       = 4000;
    static constexpr int     kNoteWidthDip = 300;
    static constexpr int     kNoteGapDip   = 8;



    double  scale  = static_cast<double> (GetDpi()) / USER_DEFAULT_SCREEN_DPI;
    RECT    window = {};
    LONG    status = 0;



    m_zoom = std::clamp (zoom, DxuiZoomControl::kMinZoom, DxuiZoomControl::kMaxZoom);

    while (m_zoom > 1.0 && !FitsZoom (m_zoom))
    {
        m_zoom = std::max (1.0, m_zoom - kStep);
    }

    if (m_zoom < std::min (zoom, DxuiZoomControl::kMaxZoom) - kStep / 2)
    {
        status = m_boundsDip.bottom - m_statusBand->GetHeightPx (m_session.hostScaler) - m_session.hostScaler.ToPx (kNoteGapDip);

        //  It has a tooltip apart from the hover tips, as any pointer move
        //  would otherwise dismiss or replace it.
        m_tooltip.HideImmediate();
        m_zoomNote.ShowTimed ({ m_boundsDip.right - m_session.hostScaler.ToPx (kNoteWidthDip), status, m_boundsDip.right, status },
                              std::format (L"Zoom is limited to {:.0f}% at this window size. Enlarge the window to zoom further.", m_zoom * 100.0),
                              static_cast<int64_t> (GetTickCount64()), kNoteMs, DxuiPopupPlacement::Above);
    }

    m_statusBand->SetZoom (m_zoom);

    if (GetHwnd() != nullptr && GetWindowRect (GetHwnd(), &window))
    {
        SetMinSizeDip ({ static_cast<LONG> (std::min (std::lround (kMinWidthDip  * std::max (1.0, m_zoom)), std::lround ((window.right - window.left) / scale))),
                         static_cast<LONG> (std::min (std::lround (kMinHeightDip * std::max (1.0, m_zoom)), std::lround ((window.bottom - window.top) / scale))) });
    }

    Layout (m_boundsDip, m_session.hostScaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::FitsZoom
//
//  Lays the content out at the zoom and compares the room left for a sector's
//  bytes with their full size: the largest header and every row.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::FitsZoom (double zoom)
{
    static constexpr int  kHeaderLines = 3;



    DxuiDpiScaler  zoomed;
    int            menus   = m_menus->GetHeightPx (m_session.hostScaler);
    int            status  = m_statusBand->GetHeightPx (m_session.hostScaler);
    RECT           content = { m_boundsDip.left, m_boundsDip.top + menus, m_boundsDip.right, m_boundsDip.bottom - status };
    RECT           room    = {};



    if (m_boundsDip.right <= m_boundsDip.left)
    {
        return true;
    }

    zoomed.SetDpi (static_cast<UINT> (std::lround (m_session.hostScaler.GetDpi() * zoom)));
    LayoutContent (content, zoomed);

    room = m_session.views.sectors->GetBounds();

    return room.right - room.left >= SectorsView::GetFullWidthPx (zoomed) &&
           room.bottom - room.top >= zoomed.ToPx (kHeaderLines * SectorsView::kHeaderDip + 16 * SectorsView::kRowDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnRButtonUp
//
//  DxuiWindow doesn't dispatch a right-button release, so a right drag on the
//  platter ends here.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DiskInspectorWindow::OnRButtonUp (WPARAM wParam, LPARAM lParam)
{
    DxuiMouseEvent     ev;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    UNREFERENCED_PARAMETER (wParam);

    if (m_session.views.platter->IsDragging())
    {
        ReleaseCapture();

        ev.kind        = DxuiMouseEventKind::Up;
        ev.button      = DxuiMouseButton::Right;
        ev.positionDip = { static_cast<short> (LOWORD (lParam)), static_cast<short> (HIWORD (lParam)) };

        (void) m_session.views.platter->OnMouse (ev);

        result = DxuiMessageResult::Handled;
        Invalidate();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnKeyUp
//
//  A released key goes to the menus only. Alt released alone returns Handled,
//  because DefWindowProc would otherwise enter the system's modal menu mode,
//  which stops wheel input to the window.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DiskInspectorWindow::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    DxuiKeyEvent       ev;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    UNREFERENCED_PARAMETER (lParam);

    ev.kind = DxuiKeyEventKind::Up;
    ev.vk   = vk;
    ev.ctrl = (GetKeyState (VK_CONTROL) & 0x8000) != 0;

    if (m_menus->OnKey (ev))
    {
        result = DxuiMessageResult::Handled;
        Invalidate();
    }

    return result;
}
