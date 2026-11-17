#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Ui/DiskInspector/Blocks/BlockGoToDialog.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Core/NumberFormat.h"





static constexpr LPCWSTR  s_kpszGlyphGoTo = L"\xE8AD";
static constexpr LPCWSTR  s_kpszGlyphCopy = L"\xE8C8";
static constexpr LPCWSTR  s_kpszGlyphEdit = L"\xE70F";
static constexpr LPCWSTR  s_kpszClassName = L"CassoBlockInspector";
static constexpr LPCWSTR  s_kpszFileChangedNote = L" (the file changed; save or discard to read it again)";





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
    HRESULT                   hr = S_OK;
    DxuiWindow::CreateParams  params;



    m_fileIo = &fileIo;
    m_path   = path;
    m_theme  = theme;

    outError.clear();
    BuildCommands();

    params.title                    = L"Block inspector - " + GetFileName();
    params.hInstance                = hInstance;
    params.ownerHwnd                = nullptr;
    params.placementAnchorHwnd      = owner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = s_kpszClassName;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.fitToWorkArea            = true;

    hr = Create (params);
    CHRAF (hr, outError = L"The block inspector window can't be created.");

    SetTheme (theme);
    m_tooltip.SetTheme (*theme);
    Show (true);
    QueueOpen();

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
    c.saveEditedCopy->isEnabled = [this, hasEdits] () { return hasEdits() && !IsOnCard(); };
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
    c.compareWith = make (L"&Compare with...", L"", [this] ()
    {
        FileDialogSpec         spec;
        std::filesystem::path  picked;
        bool                   isPicked = false;
        HRESULT                hr       = S_OK;

        spec.title         = L"Compare with";
        spec.initialFolder = fs::path (m_path).parent_path();
        spec.filters       = { { L"Images of blocks", L"*.po;*.hdv;*.2mg;*.2img;*.dc;*.dc42;*.image" }, { L"All files", L"*.*" } };

        hr = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, isPicked);

        if (SUCCEEDED (hr) && isPicked)
        {
            CompareWithFile (picked.wstring());
        }
    });
    c.compareWith->isEnabled   = isOpen;
    c.endComparison            = make (L"&End comparison", L"", [this] ()
    {
        m_model.EndComparison();
        m_titleNote.clear();
        UpdateTitle();
        m_panel->Refresh();
    });
    c.endComparison->isEnabled = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && m_model.IsComparing(); };

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

    m_icons.SetDpi    (GetDpiForWindow (GetHwnd()));
    m_icons.SetSizePx (MulDiv (DxuiTreeView::s_kIconDip, (int) GetDpiForWindow (GetHwnd()), USER_DEFAULT_SCREEN_DPI));
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
//  BlockInspectorWindow::QueueOpen
//
//  A file that doesn't open is reported, and the window closes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::QueueOpen()
{
    auto                   result = std::make_shared<JobResult>();
    IDiskFileIo          * fileIo = m_fileIo;
    std::string            path   = TextEncoding::WideToUtf8 (m_path);
    BlockInspectorModel  * model  = &m_model;



    m_panel->QueueJob ({ L"Opening " + GetFileName(), false,
                         [result, fileIo, path, model] (BlockJobProgress & progress)
                         {
                             std::unique_ptr<BlockImageFile>  file;

                             (void) progress;
                             result->hr = BlockImageFile::Open (*fileIo, path, file, result->message);

                             if (SUCCEEDED (result->hr))
                             {
                                 result->hr      = model->Open (std::move (file));
                                 result->message = FAILED (result->hr) ? L"The image's blocks can't be read." : L"";
                             }
                         },
                         [this, result] ()
                         {
                             if (FAILED (result->hr))
                             {
                                 ShowMessage (result->message.empty() ? L"The image can't be opened in the block inspector." : result->message);
                                 CloseWindow();
                                 return;
                             }

                             m_panel->Refresh();
                         } });
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

    if (m_isReloadPending && !m_worker.IsBusy())
    {
        m_isReloadPending = false;
        Reload();
    }

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
    m_status.Paint (painter, text, theme, BuildLead(), palette.colors.sectorBad);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::BuildLead
//
//  The status band's left end: the count of unsaved blocks, the
//  comparison's result while comparing, the volume's chips, "Opening" while
//  the file opens, and the selection.
//
////////////////////////////////////////////////////////////////////////////////

vector<InspectorStatusBar::LeadItem> BlockInspectorWindow::BuildLead() const
{
    using LeadKind = InspectorStatusBar::LeadKind;

    vector<InspectorStatusBar::LeadItem>  items;
    size_t                                differ  = 0;
    size_t                                unsaved = 0;



    if (!m_model.IsOpen())
    {
        items.push_back ({ L"Opening", LeadKind::Note });
        return items;
    }

    unsaved = m_model.GetEdits().GetChangedBlocks().size();

    if (unsaved > 0)
    {
        items.push_back ({ InspectorText::FormatCount ((int) unsaved, L"unsaved block", L"unsaved blocks"), LeadKind::Result });
    }

    if (m_worker.IsBusy())
    {
        return items;
    }

    if (m_model.IsComparing())
    {
        differ = m_model.CountDifferent();
        items.push_back ({ differ == 0 ? std::wstring (L"No blocks differ") : differ == 1 ? std::wstring (L"1 block differs") : std::format (L"{} blocks differ", NumberFormat::FormatInteger (differ)),
                           LeadKind::Result });
    }

    for (const SummaryChip & chip : InspectorText::BuildBlockChips (m_model))
    {
        items.push_back ({ chip.text, chip.isBad ? LeadKind::BadChip : LeadKind::Chip });
    }

    items.push_back ({ m_panel->GetSelectionLine(), LeadKind::Selection });

    return items;
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
    if (!IsOnCard() && !m_model.GetFile().IsFileUnchanged())
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
    auto                   result = std::make_shared<JobResult>();
    BlockInspectorModel  * model  = &m_model;
    IDiskFileIo          * fileIo = m_fileIo;
    fs::path               image  = m_path;
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
                                 Reload();
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
    auto                   result = std::make_shared<JobResult>();
    BlockInspectorModel  * model  = &m_model;



    m_editNote.clear();
    m_panel->SetEditing (false);

    m_panel->QueueJob ({ label, false,
                         [result, model, write] (BlockJobProgress & progress) { result->hr = write (*model, result->message, progress); },
                         [this, result, done] ()
                         {
                             bool  isClosing = m_isCloseAfterSave && SUCCEEDED (result->hr);

                             m_isCloseAfterSave = false;
                             m_editNote         = SUCCEEDED (result->hr) ? done + L" " + GetFileName()
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
    auto                   result    = std::make_shared<JobResult>();
    BlockInspectorModel  * model     = &m_model;
    IDiskFileIo          * fileIo    = m_fileIo;
    fs::path               image     = m_path;
    std::wstring           extension = image.has_extension() ? image.extension().wstring().substr (1) : std::wstring (L"po");
    FileDialogSpec         spec;
    fs::path               target;
    bool                   isPicked  = false;
    bool                   isTaken   = true;
    std::error_code        error;
    HRESULT                hr        = S_OK;



    BAIL_OUT_IF (m_worker.IsBusy() || !m_model.IsOpen() || m_model.GetEdits().IsEmpty() || IsOnCard(), S_OK);

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
            ShowMessage (L"That file is the image this window opened. Choose another file for the copy.");
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
    bool          isOpen = m_model.IsOpen() && !m_worker.IsBusy();
    EditBarState  state;



    state.canEdit        = isOpen;
    state.isEditing      = m_panel->IsEditing();
    state.hasSectorEdit  = isOpen && m_model.GetEdits().HasChanges (m_model.GetBlock());
    state.hasChecksum    = false;
    state.bytes          = m_model.IsOpen() ? m_model.GetEdits().CountChangedBytes() : 0;
    state.sectors        = m_model.IsOpen() ? (int) m_model.GetEdits().GetChangedBlocks().size() : 0;
    state.canApply       = isOpen;
    state.canSaveCopy    = !IsOnCard();
    state.canUndoApplied = isOpen && m_model.CanUndoApplied();
    state.canRedoApplied = isOpen && m_model.CanRedoApplied();
    state.canUseB        = isOpen && m_model.IsComparing() && m_model.IsDifferent (m_model.GetBlock());
    state.note           = m_editNote;

    m_panel->SetEditState (state);
    UpdateTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::UpdateTitle
//
//  "Block inspector - " and the file's name, with a red dot after the name
//  while edits are pending, then a note such as the image it is compared with.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::UpdateTitle()
{
    bool          hasEdits = m_model.IsOpen() && !m_model.GetEdits().IsEmpty();
    std::wstring  name     = L"Block inspector - " + GetFileName();
    std::wstring  title    = name + m_titleNote;
    size_t        markAt   = hasEdits ? name.size() : std::wstring::npos;



    if (title != m_title || markAt != m_titleMark)
    {
        m_title     = title;
        m_titleMark = markAt;
        SetTitle (title, markAt);
    }
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
//  BlockInspectorWindow::IsOnCard
//
//  A partition of a storage card is written in place; there is no image
//  file to copy for Save as.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::IsOnCard() const
{
    return PhysicalCards::IsOnCard (m_path);
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
        ShowMessage (label + L" hasn't finished. Close the window after it finishes.");
        return false;
    }

    m_worker.Cancel();

    blocks = m_model.IsOpen() ? (int) m_model.GetEdits().GetChangedBlocks().size() : 0;

    if (blocks > 0)
    {
        choice = InspectorDialogs::ShowChoice (GetHwnd(), m_theme, L"Unsaved changes",
                                               { L"There are unsaved changes to " + InspectorText::FormatCount (blocks, L"block", L"blocks") + L"." },
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
//  BlockInspectorWindow::Reload
//
//  A file this window wrote is not read again. A reload that comes in while
//  a job runs is made once the job ends.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Reload()
{
    enum class Outcome
    {
        Unchanged,
        EditsPending,
        Gone,
        Reopened,
    };

    auto                   outcome = std::make_shared<Outcome> (Outcome::Unchanged);
    IDiskFileIo          * fileIo  = m_fileIo;
    std::string            path    = TextEncoding::WideToUtf8 (m_path);
    BlockInspectorModel  * model   = &m_model;
    uint32_t               block   = 0;



    if (m_worker.IsBusy())
    {
        m_isReloadPending = true;
        return;
    }

    if (!m_model.IsOpen())
    {
        return;
    }

    block = m_model.GetBlock();

    m_panel->QueueJob ({ L"Reading the file again", false,
                         [outcome, fileIo, path, model, block] (BlockJobProgress & progress)
                         {
                             std::unique_ptr<BlockImageFile>  file;
                             std::wstring                     error;
                             HRESULT                          hr = S_OK;

                             (void) progress;

                             if (model->GetFile().IsFileUnchanged())
                             {
                                 return;
                             }

                             if (!model->GetEdits().IsEmpty())
                             {
                                 *outcome = Outcome::EditsPending;
                                 return;
                             }

                             hr = BlockImageFile::Open (*fileIo, path, file, error);

                             if (SUCCEEDED (hr))
                             {
                                 hr = model->Open (std::move (file));
                             }

                             if (SUCCEEDED (hr))
                             {
                                 model->SelectBlock (block);
                             }

                             *outcome = SUCCEEDED (hr) ? Outcome::Reopened : Outcome::Gone;
                         },
                         [this, outcome] ()
                         {
                             if (*outcome == Outcome::Reopened && m_titleNote == s_kpszFileChangedNote)
                             {
                                 m_titleNote.clear();
                                 UpdateTitle();
                             }
                             else if (*outcome == Outcome::EditsPending)
                             {
                                 m_titleNote = s_kpszFileChangedNote;
                                 UpdateTitle();
                             }
                             else if (*outcome == Outcome::Gone)
                             {
                                 NoteFileGone();
                             }

                             m_panel->Refresh();
                         } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::NoteFileGone
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::NoteFileGone()
{
    m_titleNote = L" (the file can't be read)";
    UpdateTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ChangeTheme
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ChangeTheme (const IDxuiTheme * theme)
{
    m_theme = theme;

    SetTheme (theme);
    m_tooltip.SetTheme (*theme);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ShowMessage
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ShowMessage (const std::wstring & text)
{
    int  shown = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), L"Block inspector", MB_OK | MB_ICONWARNING);



    IGNORE_RETURN_VALUE (shown, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::CompareWithFile
//
//  A comparison canceled from its progress has no message.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::CompareWithFile (const std::wstring & path)
{
    auto                   result = std::make_shared<JobResult>();
    IDiskFileIo          * fileIo = m_fileIo;
    std::string            other  = TextEncoding::WideToUtf8 (path);
    BlockInspectorModel  * model  = &m_model;
    std::wstring           name   = fs::path (path).filename().wstring();



    m_panel->QueueJob ({ L"Comparing with " + name, true,
                         [result, fileIo, other, model] (BlockJobProgress & progress)
                         {
                             std::unique_ptr<BlockImageFile>  file;

                             if (!BlockImageFile::IsBlockImage (*fileIo, other))
                             {
                                 result->hr      = E_INVALIDARG;
                                 result->message = L"Only another image of blocks can be compared with this one.";
                                 return;
                             }

                             result->hr = BlockImageFile::Open (*fileIo, other, file, result->message);

                             if (FAILED (result->hr))
                             {
                                 result->message = result->message.empty() ? L"The other image can't be read." : result->message;
                                 return;
                             }

                             result->hr      = model->CompareWith (std::move (file), &progress);
                             result->message = FAILED (result->hr) && result->hr != E_ABORT ? L"The other image's blocks can't be read." : L"";
                         },
                         [this, result, name] ()
                         {
                             if (!result->message.empty())
                             {
                                 ShowMessage (result->message);
                             }
                             else if (SUCCEEDED (result->hr))
                             {
                                 m_titleNote = L" compared with " + name;
                                 UpdateTitle();
                             }

                             m_panel->Refresh();
                         } });
}
