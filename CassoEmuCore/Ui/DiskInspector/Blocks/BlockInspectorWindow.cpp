#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockCommandSet.h"
#include "Ui/DiskInspector/Blocks/BlockEditController.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorState.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorTips.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Blocks/BlockRepairController.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/InspectorMarquee.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::BlockInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorWindow::BlockInspectorWindow()  = default;
BlockInspectorWindow::~BlockInspectorWindow() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorWindow::Open (
    HINSTANCE             hInstance,
    HWND                  owner,
    const IDxuiTheme    * theme,
    IDiskFileIo         & fileIo,
    const std::wstring  & path,
    std::wstring        & outError)
{
    static constexpr LPCWSTR  kpszClassName = L"CassoBlockInspector";



    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;
    size_t                    markAt = std::wstring::npos;



    m_theme = theme;
    m_session.SetFile (fileIo, path);

    outError.clear();

    params.title                    = BlockInspectorState::BuildTitle (m_session.GetFileName(), std::wstring(), false, markAt);
    params.hInstance                = hInstance;
    params.ownerHwnd                = nullptr;
    params.placementAnchorHwnd      = owner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = kpszClassName;
    params.placement                = DxuiWindowPlacement::Default;
    params.fitToWorkArea            = true;

    hr = Create (params);
    CHRAF (hr, outError = L"The block inspector window can't be created.");

    SetTheme (theme);
    m_tips->SetTheme (*theme);
    Show (true);
    m_session.Open();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnCreate
//
//  The menu bar is created before the panel and filled once the commands,
//  which act on the panel, are built.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::OnCreate()
{
    m_menus.Create  (*this, GetPopupHost(), GetTextRenderer(), m_commands);
    m_status.Create (*this, [this] (double zoom) { SetZoom (zoom); }, false);

    m_panel = CreateChild<BlockInspectorPanel>();

    CreateParts();
    m_menus.Rebuild (m_commands);

    m_panel->Init (&m_model, &m_worker, m_theme, GetHwnd(), GetTextRenderer(), &m_commands);

    m_isDarkSurface = m_theme == nullptr || DiskInspectorPalette::IsDarkSurface (m_theme->Background());

    m_icons.SetDpi         (GetDpiForWindow (GetHwnd()));
    m_icons.SetSizePx      (MulDiv (DxuiTreeView::s_kIconDip, (int) GetDpiForWindow (GetHwnd()), USER_DEFAULT_SCREEN_DPI));
    m_icons.SetDarkSurface (m_isDarkSurface);
    m_panel->GetNavigator().SetIcons     (&m_icons);
    m_panel->GetNavigator().SetPopupHost (GetPopupHost());
    m_panel->SetPopupHost                (GetPopupHost());

    m_panel->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });
    m_panel->SetOnChanged      ([this] () { m_isDirty = true; });
    m_panel->SetOnJobQueued    ([this] () { m_isDirty = true; });
    m_panel->SetOnCopy         ([this] () { m_edits->CopyBlock(); });
    m_panel->SetOnHint         ([this] (const std::wstring & hint) { m_status.SetHint (hint); m_isDirty = true; });
    m_edits->ConnectEditBar();

    FocusControl (m_panel->GetHexView());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::CreateParts
//
//  The parts of block mode, each with the panel's and the window's
//  interfaces it calls, and the commands over them.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::CreateParts()
{
    BlockInspectorPanel  & panel = *m_panel;



    m_session.Attach (panel, panel);

    m_edits      = std::make_unique<BlockEditController>   (BlockEditController::Parts   { m_model, panel, panel, *this, *this, m_session, m_session, m_clipboard });
    m_repair     = std::make_unique<BlockRepairController> (BlockRepairController::Parts { m_model, panel, panel, panel, *this, m_session, *m_edits, m_drives });
    m_tips       = std::make_unique<BlockInspectorTips>    (panel, panel);
    m_commandSet = std::make_unique<BlockCommandSet>       (BlockCommandSet::Parts       { m_model, panel, panel, panel, panel, *this, *this, m_session, m_session, *m_edits, *m_repair }, m_commands);

    m_tips->SetPopupHost (GetPopupHost());
    m_commandSet->Build();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::RenderIfChanged
//
//  Each finished job's completion, then a reload that came in while a job
//  ran, then a repaint when something changed or a long job's progress moves.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::RenderIfChanged()
{
    if (GetHwnd() == nullptr || m_panel == nullptr)
    {
        return;
    }

    m_worker.RunFinished();

    m_session.RunPendingReload();

    m_panel->UpdateBusy();
    m_edits->UpdateEditBar();
    m_session.UpdateTitle();
    DxuiPanel::Tick ((int64_t) GetTickCount64());
    m_menus.Tick ((int64_t) GetTickCount64());
    m_status.Tick (GetHwnd());
    m_tips->Tick ((int64_t) GetTickCount64());

    //  The window's context menus open with an animation each frame
    //  advances, as the disk inspector's do.
    if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().WantsTick())
    {
        GetPopupHost()->GetContextMenu().Tick ((int64_t) GetTickCount64());
        m_isDirty = true;
    }

    if (m_panel->GetMap().IsFlying())
    {
        m_panel->GetMap().StepFlight (InspectorMarquee::GetClockMs());
    }

    //  The map's marquee marches and its current block shimmers, as the
    //  disk inspector's do, so the map is drawn each frame while they're on it.
    if (m_isDirty || m_panel->IsLongJob() || m_panel->GetMap().IsAnimating())
    {
        m_isDirty = false;
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Layout
//
//  The menu bar and the status band at the window's scale, and the panel
//  between them at the window's zoom.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    DxuiDpiScaler  zoomed;
    int            menus  = m_menus.GetHeightPx (scaler);
    int            status = m_status.GetHeightPx (scaler);



    zoomed.SetDpi (static_cast<UINT> (std::lround (scaler.GetDpi() * m_zoom)));

    SetBounds (boundsPx);
    m_menus.Layout  ({ boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + menus }, boundsPx, scaler);
    m_status.Layout ({ boundsPx.left, boundsPx.bottom - status, boundsPx.right, boundsPx.bottom }, scaler);

    if (m_tips != nullptr)
    {
        m_tips->Layout (scaler.GetDpi(), boundsPx.right - boundsPx.left, boundsPx.bottom - boundsPx.top);
    }

    if (m_panel != nullptr)
    {
        m_panel->Layout ({ boundsPx.left, boundsPx.top + menus, boundsPx.right, boundsPx.bottom - status }, zoomed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (DiskInspectorPalette::IsDarkSurface (theme.Background()));



    DxuiPanel::Paint (painter, text, theme);
    m_status.Paint (painter, text, theme, BlockInspectorState::BuildLead (m_model, m_worker.IsBusy(), m_panel->GetSelectionLine()), palette.colors.sectorBad);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::SetZoom
//
//  The whole window's zoom, as the track inspector's.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::SetZoom (double zoom)
{
    DxuiDpiScaler  scaler;



    m_zoom = std::clamp (zoom, DxuiZoomControl::kMinZoom, DxuiZoomControl::kMaxZoom);
    scaler.SetDpi (GetDpi());

    m_status.SetZoom (m_zoom);
    Layout (GetBounds(), scaler);
    m_isDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnMouse
//
//  The mouse's Back and Forward buttons run their commands; Ctrl with the
//  wheel, or the wheel over the zoom, zooms the window.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::OnMouse (const DxuiMouseEvent & ev)
{
    if (m_commandSet->TryRunMouseButton (ev))
    {
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && (ev.ctrl || m_status.IsOverZoom (ev.positionDip)))
    {
        SetZoom (DxuiZoomControl::StepZoom (m_zoom, ev.wheelDelta > 0 ? 1 : -1));
        return true;
    }

    m_isDirty = true;
    m_tips->OnMouse (ev);

    return DxuiPanel::OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnKey
//
//  The menus first, then the window's keys, then text being typed, then the
//  hex view's keys while editing, then the shortcuts, then the focused
//  control.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::OnKey (const DxuiKeyEvent & ev)
{
    IDxuiControl  * focused  = nullptr;
    bool            isResult = false;



    m_isDirty = true;

    if (m_menus.OnKey (ev))
    {
        return true;
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return DxuiPanel::OnKey (ev);
    }

    if (m_commandSet->TryRunWindowKey (ev, isResult))
    {
        return isResult;
    }

    focused = GetFocusManager().GetFocusedControl();

    //  Text being typed takes its keys first: Ctrl+A, the clipboard, undo,
    //  Escape and the arrows.
    if (m_panel->GetNavigator().IsTyping (focused))
    {
        return DxuiPanel::OnKey (ev);
    }

    if (m_panel->IsEditing() && focused == (IDxuiControl *) m_panel->GetHexView() && m_edits->HandleKey (ev))
    {
        return true;
    }

    if (m_commandSet->TryRunKey (ev, isResult))
    {
        return isResult;
    }

    return DxuiPanel::OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ConfirmClose
//
//  A job that can be canceled is canceled; while one that can't be is
//  running, the window stays open. Then the unsaved edits are confirmed.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::ConfirmClose()
{
    std::wstring  label;
    bool          canCancel = false;
    uint64_t      done      = 0;
    uint64_t      total     = 0;



    if (m_worker.TryGetLongJob (label, canCancel, done, total) && !canCancel)
    {
        ShowWarning (label + L" hasn't finished. Close the window after it finishes.");
        return false;
    }

    m_worker.Cancel();

    return m_edits->ConfirmUnsaved();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::CloseWindow
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::CloseWindow()
{
    if (m_isClosed)
    {
        return;
    }

    m_isClosed = true;

    m_worker.Cancel();
    Hide();

    if (m_onClosed)
    {
        m_onClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::OnWindowClose()
{
    if (ConfirmClose())
    {
        CloseWindow();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ChangeTheme
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ChangeTheme (const IDxuiTheme * theme)
{
    bool  isDark = DiskInspectorPalette::IsDarkSurface (theme->Background());



    m_theme = theme;

    SetTheme (theme);
    m_tips->SetTheme (*theme);

    //  A theme on the other surface takes the icons drawn for it.
    if (isDark != m_isDarkSurface && m_panel != nullptr)
    {
        m_isDarkSurface = isDark;
        m_icons.SetDarkSurface (isDark);
        m_panel->GetNavigator().InvalidateIcons();
        m_panel->GetNavigator().Sync();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ShowWarning
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ShowWarning (const std::wstring & text)
{
    int  shown = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), L"Block inspector", MB_OK | MB_ICONWARNING);



    IGNORE_RETURN_VALUE (shown, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::AskUnsaved
//
////////////////////////////////////////////////////////////////////////////////

BlockUnsavedChoice BlockInspectorWindow::AskUnsaved (const std::wstring & line)
{
    static constexpr int  kCancel  = 0;
    static constexpr int  kSave    = 1;
    static constexpr int  kDiscard = 2;



    int  choice = InspectorDialogs::ShowChoice (GetHwnd(), m_theme, L"Unsaved changes", { line },
                                                { { L"Save", kSave, true, false }, { L"Discard", kDiscard, false, false }, { L"Cancel", kCancel, false, true } });



    return choice == kSave ? BlockUnsavedChoice::Save : choice == kDiscard ? BlockUnsavedChoice::Discard : BlockUnsavedChoice::Cancel;
}
