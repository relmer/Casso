#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"
#include "Ui/DiskInspector/Blocks/BlockGoToDialog.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorState.h"
#include "Ui/DiskInspector/Blocks/BlockRepairTarget.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/RepairDialog.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Core/NumberFormat.h"





static constexpr LPCWSTR  s_kpszGlyphGoTo = L"\xE8AD";
static constexpr LPCWSTR  s_kpszGlyphCopy = L"\xE8C8";
static constexpr LPCWSTR  s_kpszGlyphEdit = L"\xE70F";
static constexpr LPCWSTR  s_kpszClassName = L"CassoBlockInspector";





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
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;
    size_t                    markAt = std::wstring::npos;



    m_theme = theme;
    m_session.SetFile (fileIo, path);

    outError.clear();
    BuildCommands();

    params.title                    = BlockInspectorState::BuildTitle (m_session.GetFileName(), std::wstring(), false, markAt);
    params.hInstance                = hInstance;
    params.ownerHwnd                = nullptr;
    params.placementAnchorHwnd      = owner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = s_kpszClassName;
    params.placement                = DxuiWindowPlacement::Default;
    params.fitToWorkArea            = true;

    hr = Create (params);
    CHRAF (hr, outError = L"The block inspector window can't be created.");

    SetTheme (theme);
    m_tooltip.SetTheme (*theme);
    Show (true);
    m_session.Open();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::BuildCommands
//
//  The commands block mode has, in the slots of the track inspector's menus.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::BuildCommands()
{
    InspectorCommands &  c        = m_commands;
    auto                 isOpen   = [this] () { return m_model.IsOpen() && !m_worker.IsBusy(); };
    auto                 hasEdits = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && !m_model.GetEdits().IsEmpty(); };



    auto  make = [] (LPCWSTR label, LPCWSTR accelerator, std::function<void()> dispatch)
    {
        auto  command = std::make_shared<DxuiCommand>();

        command->label       = label;
        command->accelerator = accelerator;
        command->dispatch    = std::move (dispatch);
        return command;
    };

    //  File
    c.save                      = make (L"&Save...", L"Ctrl+S", [this] () { Save(); });
    c.save->isEnabled           = hasEdits;
    c.saveEditedCopy            = make (L"Save &as...", L"", [this] () { SaveEditedCopy(); });
    c.saveEditedCopy->isEnabled = [this, hasEdits] () { return hasEdits() && !m_session.IsOnCard(); };
    c.close                     = make (L"&Close", L"Alt+F4", [this] () { OnWindowClose(); });

    //  Edit
    c.undoApplied                = make (L"&Undo save", L"Ctrl+Z", [this] () { UndoApplied(); });
    c.undoApplied->isEnabled     = [this, isOpen] () { return isOpen() && m_model.CanUndoApplied(); };
    c.redoApplied                = make (L"&Redo save", L"Ctrl+Y", [this] () { RedoApplied(); });
    c.redoApplied->isEnabled     = [this, isOpen] () { return isOpen() && m_model.CanRedoApplied(); };
    c.copySector                 = make (L"Copy &block", L"Ctrl+Shift+C", [this] () { CopyBlock(); });
    c.copySector->isEnabled      = isOpen;
    c.copySector->glyph          = s_kpszGlyphCopy;
    c.copySector->tip            = L"Copy the whole block as a hex dump (Ctrl+Shift+C)";
    c.copySector->isGlyphInMenus = true;
    c.editSector                 = make (L"&Edit block", L"F2", [this] () { m_panel->SetEditing (!m_panel->IsEditing()); });
    c.editSector->isEnabled      = isOpen;
    c.editSector->isChecked      = [this] () { return m_panel != nullptr && m_panel->IsEditing(); };
    c.editSector->glyph          = s_kpszGlyphEdit;
    c.discardEdits               = make (L"&Discard edits", L"", [this] () { Discard(); });
    c.discardEdits->isEnabled    = hasEdits;

    //  View
    c.zoomIn             = make (L"Zoom &in", L"+", [this] () { m_panel->GetMap().StepZoom (1); });
    c.zoomIn->isEnabled  = [this] () { return m_model.IsOpen() && !m_panel->GetMap().IsAtMost(); };
    c.zoomOut            = make (L"Zoom &out", L"-", [this] () { m_panel->GetMap().StepZoom (-1); });
    c.zoomOut->isEnabled = [this] () { return m_model.IsOpen() && !m_panel->GetMap().IsAtFit(); };
    c.fit                = make (L"&Whole volume", L"0", [this] () { m_panel->GetMap().Fit(); });
    c.fit->isEnabled     = [this] () { return m_model.IsOpen() && !m_panel->GetMap().IsAtFit(); };

    c.overlays.push_back (make (L"&Files", L"", [this] () { m_panel->SetFilesOn (!m_panel->IsFilesOn()); }));
    c.overlays.back()->isChecked = [this] () { return m_panel->IsFilesOn(); };
    c.overlays.back()->isEnabled = isOpen;
    c.overlays.push_back (make (L"D&ifferences", L"", [this] () { m_panel->SetDiffsOn (!m_panel->IsDiffsOn()); }));
    c.overlays.back()->isChecked = [this] () { return m_panel->IsDiffsOn(); };
    c.overlays.back()->isEnabled = [this] () { return m_model.IsOpen() && m_model.IsComparing(); };

    for (const auto & [label, kind] : { std::pair<LPCWSTR, BlockPlace::Kind> (L"&Card",   BlockPlace::Kind::Card),
                                        std::pair<LPCWSTR, BlockPlace::Kind> (L"&Volume", BlockPlace::Kind::Volume),
                                        std::pair<LPCWSTR, BlockPlace::Kind> (L"&Block",  BlockPlace::Kind::Block) })
    {
        c.views.push_back (make (label, L"", [this, kind] () { ShowLevel (kind); }));
        c.views.back()->radio     = true;
        c.views.back()->isChecked = [this, kind] () { return m_panel->GetNavigator().GetPlace().kind == kind; };
        c.views.back()->isEnabled = [this, kind] ()
        {
            return m_model.IsOpen() && !m_worker.IsBusy() && (kind == BlockPlace::Kind::Card ? m_panel->GetNavigator().GetBrowser().HasCard() : kind == BlockPlace::Kind::Block || m_model.GetVolumeCount() > 0);
        };
    }

    //  Go
    c.back                          = make (L"&Back", L"Alt+Left", [this] () { m_panel->GoBack(); });
    c.back->isEnabled               = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && m_panel->GetNavigator().GetBrowser().CanGoBack(); };
    c.forward                       = make (L"&Forward", L"Alt+Right", [this] () { m_panel->GoForward(); });
    c.forward->isEnabled            = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && m_panel->GetNavigator().GetBrowser().CanGoForward(); };
    c.goTo                          = make (L"&Go to block...", L"Ctrl+G", [this] () { GoToBlock(); });
    c.goTo->isEnabled               = isOpen;
    c.goTo->shortLabel              = L"Go to";
    c.goTo->glyph                   = s_kpszGlyphGoTo;
    c.goTo->tip                     = L"Go to a block (Ctrl+G)";
    c.goTo->isGlyphInMenus          = true;
    c.previousSector                = make (L"&Previous block", L"Page Up", [this] () { m_panel->Step (-1); });
    c.previousSector->isEnabled     = [this] () { return m_model.IsOpen() && m_model.GetBlock() > 0; };
    c.nextSector                    = make (L"&Next block", L"Page Down", [this] () { m_panel->Step (1); });
    c.nextSector->isEnabled         = [this] () { return m_model.IsOpen() && m_model.GetBlock() + 1 < m_model.GetBlockCount(); };
    c.previousDifference            = make (L"Previous &difference", L"Shift+F8", [this] () { StepDifference (false); });
    c.previousDifference->isEnabled = [this] () { return m_model.IsOpen() && m_model.CountDifferent() > 0; };
    c.nextDifference                = make (L"Next d&ifference", L"F8", [this] () { StepDifference (true); });
    c.nextDifference->isEnabled     = [this] () { return m_model.IsOpen() && m_model.CountDifferent() > 0; };

    //  Disk
    c.compareWith = make (L"&Compare with...", L"", [this] () { m_session.CompareWithPicked(); });
    c.compareWith->isEnabled   = isOpen;
    c.endComparison            = make (L"&End comparison", L"", [this] () { m_session.EndComparison(); });
    c.endComparison->isEnabled = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && m_model.IsComparing(); };
    c.repair                   = make (L"&Repair...", L"", [this] () { Repair(); });
    c.repair->isEnabled        = [this, isOpen] () { return isOpen() && GetRepairVolume() >= 0; };

    //  Window
    c.windowZoomIn  = make (L"Zoom &in", L"Ctrl++", [this] () { SetZoom (DxuiZoomControl::StepZoom (m_zoom, 1)); });
    c.windowZoomOut = make (L"Zoom &out", L"Ctrl+-", [this] () { SetZoom (DxuiZoomControl::StepZoom (m_zoom, -1)); });
    c.actualSize    = make (L"&Actual size", L"Ctrl+0", [this] () { SetZoom (1.0); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::RunCommand
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::RunCommand (const InspectorCommands::Command & command)
{
    bool  isRun = command != nullptr && command->dispatch && command->IsEnabled();



    if (isRun)
    {
        command->dispatch();
    }

    return isRun;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::OnCreate()
{
    m_menus.Create  (*this, GetPopupHost(), GetTextRenderer(), m_commands);
    m_tooltip.SetPopupHost (GetPopupHost());
    m_status.Create (*this, [this] (double zoom) { SetZoom (zoom); }, false);

    m_panel = CreateChild<BlockInspectorPanel>();
    m_panel->Init (&m_model, &m_worker, m_theme, GetHwnd(), GetTextRenderer(), &m_commands);
    m_session.Attach (*m_panel, *m_panel);

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
    InitEditBar();
    m_panel->SetOnCopy         ([this] () { CopyBlock(); });
    m_panel->SetOnHint         ([this] (const std::wstring & hint) { m_hint = hint; m_status.SetHint (hint); m_isDirty = true; });

    FocusControl (m_panel->GetHexView());
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
    UpdateEditing();
    DxuiPanel::Tick ((int64_t) GetTickCount64());
    m_menus.Tick ((int64_t) GetTickCount64());
    m_status.Tick (GetHwnd());
    m_tooltip.Tick ((int64_t) GetTickCount64());

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
    m_tooltip.SetDpi          (scaler.GetDpi());
    m_tooltip.SetViewportSize (boundsPx.right - boundsPx.left, boundsPx.bottom - boundsPx.top);
    m_menus.Layout  ({ boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + menus }, boundsPx, scaler);
    m_status.Layout ({ boundsPx.left, boundsPx.bottom - status, boundsPx.right, boundsPx.bottom }, scaler);

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
//  The mouse's Back and Forward buttons go back and forward; Ctrl with the
//  wheel, or the wheel over the zoom, zooms the window.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::Back || ev.button == DxuiMouseButton::Forward))
    {
        (void) RunCommand (ev.button == DxuiMouseButton::Back ? m_commands.back : m_commands.forward);
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && (ev.ctrl || m_status.IsOverZoom (ev.positionDip)))
    {
        SetZoom (DxuiZoomControl::StepZoom (m_zoom, ev.wheelDelta > 0 ? 1 : -1));
        return true;
    }

    m_isDirty = true;

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (ev.positionDip);
    }
    else if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_tooltip.RequestHide ((int64_t) GetTickCount64());
        m_tooltipText.clear();
    }

    return DxuiPanel::OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::UpdateTooltip
//
//  Over the block map, the map's tip appears at once and follows the
//  pointer, as the File map grid's does in the disk inspector.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::UpdateTooltip (POINT pointPx)
{
    std::wstring               tip;
    RECT                       anchor = {};
    vector<DxuiTooltipSwatch>  swatches;
    int64_t                    now    = (int64_t) GetTickCount64();



    if (!m_worker.IsBusy() && m_panel->GetMap().GetTooltip (pointPx, tip, anchor))
    {
        (void) m_panel->GetMap().GetTooltipSwatches (pointPx, swatches);
    }
    else if (!m_worker.IsBusy() && !m_panel->GetHexTooltip (pointPx, tip, anchor, swatches))
    {
        tip.clear();
    }

    if (tip.empty())
    {
        m_tooltip.RequestHide (now);
    }
    else
    {
        m_tooltip.SetSwatches    (swatches);
        m_tooltip.RequestShowNow (anchor, tip, now);
    }

    m_tooltipText = tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnKey
//
//  The menus first, then the window's shortcuts, then the focused control.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::OnKey (const DxuiKeyEvent & ev)
{
    bool  isDown = ev.kind == DxuiKeyEventKind::Down;
    bool  isZoom = ev.vk == '0' || ev.vk == VK_NUMPAD0 || ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD || ev.vk == VK_OEM_MINUS || ev.vk == VK_SUBTRACT;



    m_isDirty = true;

    if (m_menus.OnKey (ev))
    {
        return true;
    }

    if (!isDown)
    {
        return DxuiPanel::OnKey (ev);
    }

    if (ev.ctrl && !ev.alt && isZoom)
    {
        SetZoom ((ev.vk == '0' || ev.vk == VK_NUMPAD0) ? 1.0 : DxuiZoomControl::StepZoom (m_zoom, (ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD) ? 1 : -1));
        return true;
    }

    if (ev.alt && !ev.ctrl && !ev.shift && (ev.vk == VK_LEFT || ev.vk == VK_RIGHT))
    {
        return RunCommand (ev.vk == VK_LEFT ? m_commands.back : m_commands.forward);
    }

    //  Text being typed takes its keys first: Ctrl+A, the clipboard, undo,
    //  Escape and the arrows.
    if (m_panel->GetNavigator().IsTyping (GetFocusManager().GetFocusedControl()))
    {
        return DxuiPanel::OnKey (ev);
    }

    //  While editing, the hex view takes Ctrl+Z and Ctrl+Y for the pending
    //  edits, and Escape stops editing, as the Sector data tab does.
    if (m_panel->IsEditing() && GetFocusManager().GetFocusedControl() == (IDxuiControl *) m_panel->GetHexView() && HandleEditKey (ev))
    {
        return true;
    }

    if (!ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        return RunCommand (m_commands.editSector);
    }

    if (ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
            case 'G': return RunCommand (m_commands.goTo);
            case 'C': return ev.shift ? RunCommand (m_commands.copySector) : DxuiPanel::OnKey (ev);
            case 'S': return RunCommand (m_commands.save);
            case 'Z': return RunCommand (m_commands.undoApplied);
            case 'Y': return RunCommand (m_commands.redoApplied);
            default:  break;
        }
    }

    if (!ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
            case VK_PRIOR:  return RunCommand (m_commands.previousSector);
            case VK_NEXT:   return RunCommand (m_commands.nextSector);
            case VK_F8:     return RunCommand (ev.shift ? m_commands.previousDifference : m_commands.nextDifference);
            case VK_ESCAPE: m_worker.Cancel(); break;
            default:        break;
        }
    }

    return DxuiPanel::OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockInspectorWindow::GetCursorForPoint (POINT clientPx) const
{
    return DxuiPanel::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Save
//
//  The unsaved changes are counted in the status bar and marked in the
//  title, so a save writes them without a question.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Save()
{
    if (!m_worker.IsBusy() && m_model.IsOpen() && !m_model.GetEdits().IsEmpty())
    {
        StartSave();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::StartSave
//
//  When an image file changed outside Casso after it was opened, the edits
//  go to a preserved copy beside it, as the disk inspector saves them, and
//  the file is left as it is.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::StartSave()
{
    if (!m_session.IsOnCard() && !m_model.GetFile().IsFileUnchanged())
    {
        SaveToPreservedCopy();
    }
    else
    {
        QueueSave();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::SaveToPreservedCopy
//
//  A copy of the file as it is now, with the edited blocks written into it.
//  The edits are then saved, so the window reads the changed file again.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::SaveToPreservedCopy()
{
    auto                   result = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model  = &m_model;
    IDiskFileIo          * fileIo = &m_session.GetFileIo();
    fs::path               image  = m_session.GetPath();
    std::string            stamp  = PreservedCopy::MakeStamp (time (nullptr));
    fs::path               target;
    fs::path               candidate;
    std::error_code        error;



    for (int attempt = 0; attempt < PreservedCopy::kMaxAttempts && target.empty(); attempt++)
    {
        candidate = TextEncoding::Utf8ToWide (PreservedCopy::MakePath (TextEncoding::WideToUtf8 (image.wstring()), stamp, attempt));
        target    = fs::exists (candidate, error) ? fs::path() : candidate;
    }

    if (target.empty())
    {
        m_isCloseAfterSave = false;
        m_editNote         = L"Not saved: the file changed outside Casso, and a preserved copy can't be named.";
        m_panel->Refresh();
        return;
    }

    m_editNote = L"Saving...";
    m_panel->SetEditing (false);

    m_panel->QueueJob ({ L"Saving a preserved copy", false,
                         [result, model, fileIo, image, target] (BlockJobProgress & progress)
                         {
                             (void) progress;

                             if (!CopyFileW (image.c_str(), target.c_str(), TRUE))
                             {
                                 result->hr = HRESULT_FROM_WIN32 (GetLastError());
                                 return;
                             }

                             result->hr = model->WriteEditsTo (*fileIo, TextEncoding::WideToUtf8 (target.wstring()), result->message);
                         },
                         [this, result, target] ()
                         {
                             bool  isClosing = m_isCloseAfterSave && SUCCEEDED (result->hr);

                             m_isCloseAfterSave = false;
                             m_editNote         = SUCCEEDED (result->hr) ? L"Saved to a preserved copy, since the file changed outside Casso: " + target.filename().wstring()
                                                                         : L"Not saved: the copy could not be written to " + target.filename().wstring();

                             if (SUCCEEDED (result->hr))
                             {
                                 m_model.ClearEdits();
                                 m_session.Reload();
                             }

                             m_panel->Refresh();

                             if (isClosing)
                             {
                                 CloseWindow();
                             }
                         } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::QueueSave
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::QueueSave()
{
    QueueWrite (L"Saving the edits", L"Saved to", [] (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)
                { return model.ApplyEdits (outError, &progress); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::UndoApplied
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::UndoApplied()
{
    if (m_worker.IsBusy() || !m_model.CanUndoApplied())
    {
        return;
    }

    QueueWrite (L"Undoing the last save", L"Undid the save to", [] (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)
                { return model.UndoApplied (outError, &progress); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::RedoApplied
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::RedoApplied()
{
    if (m_worker.IsBusy() || !m_model.CanRedoApplied())
    {
        return;
    }

    QueueWrite (L"Saving the edits", L"Saved to", [] (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)
                { return model.RedoApplied (outError, &progress); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::QueueWrite
//
//  A save, undo or redo on the worker's thread. Its result goes in the
//  edit bar's note, as "Saved to" the file. A save chosen when closing the
//  window closes it once the save succeeds.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::QueueWrite (const std::wstring & label, const std::wstring & done, WriteFn write)
{
    auto                   result = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model  = &m_model;



    m_editNote.clear();
    m_panel->SetEditing (false);

    m_panel->QueueJob ({ label, false,
                         [result, model, write] (BlockJobProgress & progress) { result->hr = write (*model, result->message, progress); },
                         [this, result, done] ()
                         {
                             bool  isClosing = m_isCloseAfterSave && SUCCEEDED (result->hr);

                             m_isCloseAfterSave = false;
                             m_editNote         = SUCCEEDED (result->hr) ? done + L" " + m_session.GetFileName()
                                                                         : (result->message.empty() ? L"The edits weren't saved." : result->message);
                             m_panel->Refresh();

                             if (isClosing)
                             {
                                 CloseWindow();
                             }
                         } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::SaveEditedCopy
//
//  The image with the pending edits saved to a new file the user chooses,
//  never the image's file. The pending edits stay pending, and the image's
//  file is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::SaveEditedCopy()
{
    auto                   result    = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model     = &m_model;
    IDiskFileIo          * fileIo    = &m_session.GetFileIo();
    fs::path               image     = m_session.GetPath();
    std::wstring           extension = image.has_extension() ? image.extension().wstring().substr (1) : std::wstring (L"po");
    FileDialogSpec         spec;
    fs::path               target;
    bool                   isPicked  = false;
    bool                   isTaken   = true;
    std::error_code        error;
    HRESULT                hr        = S_OK;



    BAIL_OUT_IF (m_worker.IsBusy() || !m_model.IsOpen() || m_model.GetEdits().IsEmpty() || m_session.IsOnCard(), S_OK);

    spec.filters         = { { L"Disk images", L"*." + extension } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = image.stem().wstring() + L" edited." + extension;
    spec.initialFolder    = image.parent_path();

    //  The Save dialog opens again until the file isn't the image's.
    while (isTaken)
    {
        hr = m_dialogs.PickFileToSave (GetHwnd(), spec, target, isPicked);
        CHR (hr);
        BAIL_OUT_IF (!isPicked, S_OK);

        isTaken = fs::equivalent (target, image, error) || _wcsicmp (fs::absolute (target, error).wstring().c_str(), fs::absolute (image, error).wstring().c_str()) == 0;

        if (isTaken)
        {
            ShowWarning (L"That file is the image this window opened. Choose another file for the copy.");
        }
    }

    m_editNote = L"Preparing the copy...";

    m_panel->QueueJob ({ L"Saving the edited copy", false,
                         [result, model, fileIo, image, target] (BlockJobProgress & progress)
                         {
                             (void) progress;

                             if (!CopyFileW (image.c_str(), target.c_str(), FALSE))
                             {
                                 result->hr = HRESULT_FROM_WIN32 (GetLastError());
                                 return;
                             }

                             result->hr = model->WriteEditsTo (*fileIo, TextEncoding::WideToUtf8 (target.wstring()), result->message);
                         },
                         [this, result, target] ()
                         {
                             m_editNote = SUCCEEDED (result->hr) ? L"Saved a copy with the pending edits to " + target.filename().wstring()
                                                                 : L"Not saved: the copy could not be written to " + target.filename().wstring();
                             m_panel->Refresh();
                         } });

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::InitEditBar
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::InitEditBar()
{
    EditBar  & bar = m_panel->GetEditBar();



    bar.SetOnEdit          ([this] () { m_panel->SetEditing (false); });
    bar.SetOnDiscardSector ([this] () { if (!m_worker.IsBusy()) { m_model.DiscardBlock (m_model.GetBlock()); m_panel->Refresh(); } });
    bar.SetOnDiscard       ([this] () { Discard(); });
    bar.SetOnApply         ([this] () { Save(); });
    bar.SetOnSaveCopy      ([this] () { SaveEditedCopy(); });
    bar.SetOnUndoApplied   ([this] () { UndoApplied(); });
    bar.SetOnRedoApplied   ([this] () { RedoApplied(); });
    bar.SetOnUseB          ([this] () { if (!m_worker.IsBusy() && m_model.UseOtherBlock (m_model.GetBlock())) { m_panel->Refresh(); } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::UpdateEditing
//
//  The edit bar and the title from the pending edits, once a frame.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::UpdateEditing()
{
    m_panel->SetEditState (BlockInspectorState::BuildEditBarState (m_model, m_worker.IsBusy(), m_panel->IsEditing(), m_session.IsOnCard(), m_editNote));
    m_session.UpdateTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::HandleEditKey
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::HandleEditKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = true;



    if (ev.ctrl && !ev.alt && ev.vk == 'Z')
    {
        m_editNote.clear();

        if (m_model.Undo())
        {
            m_panel->Refresh();
        }
    }
    else if (ev.ctrl && !ev.alt && ev.vk == 'Y')
    {
        m_editNote.clear();

        if (m_model.Redo())
        {
            m_panel->Refresh();
        }
    }
    else if (ev.ctrl && !ev.alt && ev.vk == 'V')
    {
        PasteIntoBlock();
    }
    else if (!ev.ctrl && !ev.alt && ev.vk == VK_ESCAPE)
    {
        m_panel->SetEditing (false);
    }
    else
    {
        isHandled = false;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Discard
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Discard()
{
    if (!m_worker.IsBusy() && m_model.IsOpen())
    {
        m_model.DiscardEdits();
        m_panel->Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::GetRepairVolume
//
//  The volume of the current place, or the image's only volume.
//
////////////////////////////////////////////////////////////////////////////////

int BlockInspectorWindow::GetRepairVolume() const
{
    int  volume = m_panel->GetNavigator().GetPlace().volume;



    if (volume < 0 && m_model.GetVolumeCount() == 1)
    {
        volume = 0;
    }

    return volume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Repair
//
//  The confirmation lists every change the volume can have, and the checked
//  ones are written to a repaired copy, as the track inspector writes one,
//  where BlockRepairTarget chooses. The image isn't written.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Repair()
{
    int                     volume = GetRepairVolume();
    RepairPlan              full   = m_model.PlanRepair (volume, {});
    RepairDialog            dialog;
    BlockRepairSource       source;
    BlockRepairDestination  destination;
    std::wstring            intro;
    int                     lines  = 0;
    std::set<std::wstring>  chosen;
    HRESULT                 hr     = S_OK;



    if (m_worker.IsBusy() || !full.canRepair)
    {
        ShowWarning (L"Repair isn't available. " + (m_worker.IsBusy() ? std::wstring (L"Wait for the current job to finish.") : full.reason));
        return;
    }

    intro = BlockRepairTarget::BuildIntro (m_session.IsOnCard(), m_session.GetFileName(), m_session.GetHostFileName(), full.before == FileSystemVerdict::Suspect, m_model.IsSectorVolume (volume), lines);

    //  Blocks in no file can be another file system's on a hybrid disk or a
    //  larger DOS image, so freeing them starts unchecked.
    chosen = DiskRepair::GetKeys (full);
    chosen.erase (L"unowned");

    dialog.Configure (m_theme, intro, lines, full, [this, volume] (const std::set<std::wstring> & keys) { return m_model.PlanRepair (volume, keys); }, chosen);

    hr = dialog.ShowModal (GetHwnd(), m_theme);

    if (FAILED (hr) || !dialog.IsConfirmed())
    {
        return;
    }

    source.isCard         = m_session.IsOnCard();
    source.imagePath      = m_session.GetPath();
    source.hostFileName   = m_session.GetHostFileName();
    source.volumeLabel    = m_model.GetVolumeMap (volume).volumeName;
    source.isSectorVolume = m_model.IsSectorVolume (volume);
    source.volumeBytes    = (uint64_t) m_model.GetVolumeSize (volume) * BlockInspectorModel::kBlockBytes;
    destination           = BlockRepairTarget::ChooseDestination (source, m_session.GetFileIo(), m_drives);

    if (destination.isUserChoice && !PickRepairTarget (destination.why, destination.bytes, destination.host))
    {
        return;
    }

    QueueRepairCopy (volume, dialog.GetPlan(), source.isCard, destination.host.wstring() + destination.rest);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::PickRepairTarget
//
//  The Save dialog opens again until the file isn't the image and its
//  drive has room for the copy. False when the user cancels.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::PickRepairTarget (const std::wstring & why, uint64_t bytes, fs::path & ioTarget)
{
    std::wstring     extension = ioTarget.has_extension() ? ioTarget.extension().wstring().substr (1) : std::wstring (L"po");
    FileDialogSpec   spec;
    fs::path         picked;
    bool             isPicked  = false;
    bool             isChosen  = false;
    std::error_code  error;
    HRESULT          hr        = S_OK;



    if (!why.empty())
    {
        ShowWarning (why);
    }

    spec.title            = L"Save the repaired copy";
    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = ioTarget.filename().wstring();
    spec.initialFolder    = ioTarget.parent_path();

    while (!isChosen)
    {
        hr = m_dialogs.PickFileToSave (GetHwnd(), spec, picked, isPicked);

        if (FAILED (hr) || !isPicked)
        {
            return false;
        }

        if (!m_session.IsOnCard() && (fs::equivalent (picked, m_session.GetPath(), error) || _wcsicmp (fs::absolute (picked, error).wstring().c_str(), fs::absolute (m_session.GetPath(), error).wstring().c_str()) == 0))
        {
            ShowWarning (L"That file is the image this window opened. Choose another file for the copy.");
        }
        else if (!m_drives.HasRoomFor (picked.parent_path(), bytes))
        {
            ShowWarning (L"That drive doesn't have room for the repaired copy. Choose another folder.");
        }
        else
        {
            isChosen = true;
        }
    }

    ioTarget = picked;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::QueueRepairCopy
//
//  The image copied and the plan's blocks written into the copy, or a card's
//  partition written whole, on the worker's thread. A copy that fails
//  partway is deleted.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::QueueRepairCopy (int volume, const RepairPlan & plan, bool isCard, const fs::path & target)
{
    auto                   result = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model  = &m_model;
    IDiskFileIo          * fileIo = &m_session.GetFileIo();
    fs::path               image  = m_session.GetPath();



    m_editNote = L"Writing the repaired copy...";

    m_panel->QueueJob ({ L"Writing the repaired copy", false,
                         [result, model, fileIo, image, target, volume, plan, isCard] (BlockJobProgress & progress)
                         {
                             std::string   path       = TextEncoding::WideToUtf8 (target.wstring());
                             std::wstring  targetHost = TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (path));

                             //  An image in a container is copied with its host file.
                             if (!isCard && !CopyFileW (TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (image.wstring()))).c_str(), targetHost.c_str(), FALSE))
                             {
                                 result->hr      = HRESULT_FROM_WIN32 (GetLastError());
                                 result->message = L"The image can't be copied.";
                             }
                             else
                             {
                                 result->hr = model->WriteRepairTo (*fileIo, path, volume, plan, isCard, result->message, &progress);
                             }

                             if (FAILED (result->hr) && fileIo->Exists (TextEncoding::WideToUtf8 (targetHost)))
                             {
                                 (void) fileIo->Remove (TextEncoding::WideToUtf8 (targetHost));
                             }
                         },
                         [this, result, target] ()
                         {
                             m_editNote = SUCCEEDED (result->hr) ? L"Repaired copy written: " + fs::path (TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (target.wstring())))).filename().wstring()
                                                                 : L"Repair failed: " + (result->message.empty() ? std::wstring (L"The repaired copy can't be written.") : result->message);
                             m_panel->Refresh();
                         } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::CopyBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::CopyBlock()
{
    std::wstring  text = m_worker.IsBusy() ? std::wstring() : m_panel->BuildBlockText();



    if (!text.empty())
    {
        (void) m_clipboard.SetText (GetHwnd(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::PasteIntoBlock
//
//  Ctrl+V while editing: the clipboard's hex digits, or its text in the
//  text column, over the block from the caret, as the sector editor
//  pastes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::PasteIntoBlock()
{
    BlockHexView  * hex      = m_panel->GetHexView();
    bool            isText   = hex != nullptr && hex->GetActiveColumn() == DxuiHexView::Column::Text;
    std::wstring    text;
    vector<Byte>    values;
    int             overflow = 0;
    int             written  = 0;



    if (hex == nullptr || m_worker.IsBusy() || !m_model.IsOpen() || !m_model.IsLoaded (m_model.GetBlock()) || !m_clipboard.GetText (GetHwnd(), text))
    {
        return;
    }

    written    = m_model.Paste ((int) hex->GetCaret(), text, isText, overflow);
    m_editNote = (written == 0) ? (isText ? std::wstring (L"Nothing to paste: the clipboard has no printable text") : InspectorClipboard::GetHexPasteNote (InspectorClipboard::ParseHex (text, values)))
               : (overflow > 0) ? InspectorText::FormatCount (overflow, L"byte", L"bytes") + L" did not fit before the end of the block"
                                : std::wstring();

    hex->ResetNibble();
    m_panel->Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ShowLevel
//
//  The View menu's places: the card, the volume of the current place or
//  block, and the current block.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ShowLevel (BlockPlace::Kind kind)
{
    BlockNavigator  & navigator = m_panel->GetNavigator();
    int               volume    = navigator.GetPlace().volume >= 0 ? navigator.GetPlace().volume : std::max (0, m_model.FindVolume (m_model.GetBlock()));



    if (kind == BlockPlace::Kind::Block)
    {
        m_panel->SelectBlock (m_model.GetBlock());
        return;
    }

    navigator.GoTo ({ kind, kind == BlockPlace::Kind::Card ? -1 : volume, -1 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::GoToBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::GoToBlock()
{
    uint32_t  block = 0;



    if (BlockGoToDialog::Choose (GetHwnd(), m_theme, m_model.GetBlockCount(), m_model.GetBlock(), block))
    {
        m_panel->GoToBlock (block);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::StepDifference
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::StepDifference (bool isForward)
{
    uint32_t  block = 0;



    if (m_model.FindDifference (isForward, block))
    {
        m_panel->SelectBlock (block);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ConfirmClose
//
//  A job that can be canceled is canceled; while one that can't be is
//  running, the window stays open. With unsaved changes, the user picks
//  Save, Discard or Cancel.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::ConfirmClose()
{
    static constexpr int  kCancel  = 0;
    static constexpr int  kSave    = 1;
    static constexpr int  kDiscard = 2;



    int           blocks    = 0;
    int           choice    = kCancel;
    bool          isClosing = true;
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

    blocks = m_model.IsOpen() ? (int) m_model.GetEdits().GetChangedBlocks().size() : 0;

    if (blocks > 0)
    {
        choice = InspectorDialogs::ShowChoice (GetHwnd(), m_theme, L"Unsaved changes",
                                               { InspectorText::FormatUnsavedChanges (blocks, L"block", L"blocks", m_session.GetFileName()) },
                                               { { L"Save", kSave, true, false }, { L"Discard", kDiscard, false, false }, { L"Cancel", kCancel, false, true } });

        //  The window closes once the save succeeds.
        if (choice == kSave)
        {
            m_isCloseAfterSave = true;
            StartSave();
        }

        isClosing = choice == kDiscard;
    }

    return isClosing;
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
    m_tooltip.SetTheme (*theme);

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
