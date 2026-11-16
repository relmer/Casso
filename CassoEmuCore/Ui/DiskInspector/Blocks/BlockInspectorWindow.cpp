#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Blocks/BlockGoToDialog.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"
#include "Ui/DiskInspector/Blocks/BlockLevelHost.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Core/NumberFormat.h"





static constexpr LPCWSTR  s_kpszGlyphGoTo = L"\xE8AD";
static constexpr LPCWSTR  s_kpszGlyphCopy = L"\xE8C8";
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
    HRESULT                   hr = S_OK;
    DxuiWindow::CreateParams  params;



    m_fileIo = &fileIo;
    m_path   = path;
    m_theme  = theme;

    outError.clear();
    BuildCommands();

    params.title                    = L"Disk inspector - " + GetFileName();
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
    CHRAF (hr, outError = L"The disk inspector's window can't be created.");

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
    c.close = make (L"&Close", L"Alt+F4", [this] () { OnWindowClose(); });

    //  Edit
    c.undoApplied                = make (L"&Undo", L"Ctrl+Z", [this] () { if (m_model.Undo()) { m_panel->Refresh(); } });
    c.undoApplied->isEnabled     = hasEdits;
    c.redoApplied                = make (L"&Redo", L"Ctrl+Y", [this] () { if (m_model.Redo()) { m_panel->Refresh(); } });
    c.redoApplied->isEnabled     = isOpen;
    c.copySector                 = make (L"Copy &block", L"Ctrl+Shift+C", [this] () { CopyBlock(); });
    c.copySector->isEnabled      = isOpen;
    c.copySector->glyph          = s_kpszGlyphCopy;
    c.copySector->tip            = L"Copy the whole block as a hex dump (Ctrl+Shift+C)";
    c.copySector->isGlyphInMenus = true;
    c.applyEdits                 = make (L"A&pply edits", L"", [this] () { Apply(); });
    c.applyEdits->isEnabled      = hasEdits;
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

    for (const auto & [label, level] : { std::pair<LPCWSTR, BlockLevelHost::Level> (L"&Card",   BlockLevelHost::Level::Card),
                                         std::pair<LPCWSTR, BlockLevelHost::Level> (L"&Volume", BlockLevelHost::Level::Volume),
                                         std::pair<LPCWSTR, BlockLevelHost::Level> (L"Fi&les",  BlockLevelHost::Level::Files),
                                         std::pair<LPCWSTR, BlockLevelHost::Level> (L"&Block",  BlockLevelHost::Level::Block) })
    {
        c.views.push_back (make (label, L"", [this, level] () { ShowLevel (level); }));
        c.views.back()->radio     = true;
        c.views.back()->isChecked = [this, level] () { return m_panel->GetLevels().GetLevel() == level; };
        c.views.back()->isEnabled = [this, level] ()
        {
            return m_model.IsOpen() && !m_worker.IsBusy() && (level == BlockLevelHost::Level::Card ? m_panel->GetLevels().HasCard() : level == BlockLevelHost::Level::Block || m_model.GetVolumeCount() > 0);
        };
    }

    c.legend            = make (L"Le&gend", L"", [this] () { m_panel->ToggleLegend(); });
    c.legend->isEnabled = isOpen;

    //  Go
    c.back                          = make (L"&Back", L"Alt+Left", [this] () { m_panel->GoBack(); });
    c.back->isEnabled               = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && m_panel->GetLevels().CanGoBack(); };
    c.forward                       = make (L"&Forward", L"Alt+Right", [this] () { m_panel->GoForward(); });
    c.forward->isEnabled            = [this] () { return m_model.IsOpen() && !m_worker.IsBusy() && m_panel->GetLevels().CanGoForward(); };
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
        SetTitle (L"Disk inspector - " + GetFileName());
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
    m_panel->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });
    m_panel->SetOnChanged      ([this] () { m_isDirty = true; });
    m_panel->SetOnJobQueued    ([this] () { m_isDirty = true; });
    m_panel->SetOnApply        ([this] () { Apply(); });
    m_panel->SetOnDiscard      ([this] () { Discard(); });
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
                                 ShowMessage (result->message.empty() ? L"The image can't be opened in block mode." : result->message);
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
    DxuiPanel::Tick ((int64_t) GetTickCount64());
    m_menus.Tick ((int64_t) GetTickCount64());
    m_status.Tick (GetHwnd());
    m_tooltip.Tick ((int64_t) GetTickCount64());

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
//  The status band's left end: the comparison's result while comparing, the
//  volume's chips, "Opening" while the file opens, and the selection.
//
////////////////////////////////////////////////////////////////////////////////

vector<InspectorStatusBar::LeadItem> BlockInspectorWindow::BuildLead() const
{
    using LeadKind = InspectorStatusBar::LeadKind;

    vector<InspectorStatusBar::LeadItem>  items;
    size_t                                differ = 0;



    if (!m_model.IsOpen())
    {
        items.push_back ({ L"Opening", LeadKind::Note });
        return items;
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

    if (ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
            case 'G': return RunCommand (m_commands.goTo);
            case 'C': return ev.shift ? RunCommand (m_commands.copySector) : DxuiPanel::OnKey (ev);
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
//  BlockInspectorWindow::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Apply()
{
    auto                   result = std::make_shared<JobResult>();
    BlockInspectorModel  * model  = &m_model;



    if (m_worker.IsBusy() || !m_model.IsOpen() || m_model.GetEdits().IsEmpty())
    {
        return;
    }

    m_panel->QueueJob ({ L"Writing the edits", false,
                         [result, model] (BlockJobProgress & progress) { result->hr = model->ApplyEdits (result->message, &progress); },
                         [this, result] ()
                         {
                             if (FAILED (result->hr))
                             {
                                 ShowMessage (result->message.empty() ? L"The edits weren't written." : result->message);
                             }

                             m_panel->Refresh();
                         } });
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
//  BlockInspectorWindow::ShowLevel
//
//  The View menu's levels: Volume and Files of the volume the current
//  location or block is in, and Block at the current block.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ShowLevel (BlockLevelHost::Level level)
{
    BlockLevelHost  & levels = m_panel->GetLevels();
    int               volume = levels.GetHere().volume >= 0 ? levels.GetHere().volume : std::max (0, m_model.FindVolume (m_model.GetBlock()));



    if (level == BlockLevelHost::Level::Block)
    {
        m_panel->SelectBlock (m_model.GetBlock());
        return;
    }

    levels.GoTo ({ level, level == BlockLevelHost::Level::Card ? -1 : volume, -1 });
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
        m_panel->SelectBlock (block);
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
//  running, the window stays open.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::ConfirmClose()
{
    int           bytes     = 0;
    int           answer    = IDYES;
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

    bytes = m_model.IsOpen() ? m_model.GetEdits().CountChangedBytes() : 0;

    if (bytes > 0)
    {
        answer = DxuiMessageBox (GetHwnd(), m_theme,
                                 (NumberFormat::FormatInteger (bytes) + (bytes == 1 ? L" changed byte isn't" : L" changed bytes aren't") + L" written. Close and discard the edits?").c_str(),
                                 L"Disk inspector", MB_YESNO | MB_ICONWARNING, { L"Discard edits", L"Keep editing" }, 0);
    }

    return answer == IDYES;
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
                             if (*outcome == Outcome::EditsPending)
                             {
                                 SetTitle (L"Disk inspector - " + GetFileName() + L" (the file changed; apply or discard to read it again)");
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
    SetTitle (L"Disk inspector - " + GetFileName() + L" (the file can't be read)");
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
    int  shown = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), L"Disk inspector", MB_OK | MB_ICONWARNING);



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
                                 SetTitle (L"Disk inspector - " + GetFileName() + L" compared with " + name);
                             }

                             m_panel->Refresh();
                         } });
}
