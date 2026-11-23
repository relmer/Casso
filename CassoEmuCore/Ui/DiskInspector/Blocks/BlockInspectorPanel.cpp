#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderText.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderView.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"
#include "Ui/DiskInspector/FloatingGroup.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/OverlayChip.h"
#include "Core/NumberFormat.h"





static constexpr int      s_kGroupDip       = 36;
static constexpr int      s_kGroupPadDip    = 4;
static constexpr int      s_kGroupGapDip    = 6;
static constexpr int      s_kIconDip        = 28;
static constexpr int      s_kInsetDip       = 4;
static constexpr LPCWSTR  s_kpszGlyphMinus  = L"\xE738";
static constexpr LPCWSTR  s_kpszGlyphPlus   = L"\xE710";





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::BlockInspectorPanel
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorPanel::BlockInspectorPanel()  = default;
BlockInspectorPanel::~BlockInspectorPanel() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Init
//
//  The grid is created before its corner groups, so the groups are in front
//  of it and take the pointer first.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Init (BlockInspectorModel * model, BlockInspectorWorker * worker, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text,
                                const InspectorCommands * commands)
{
    m_model = model;
    m_text  = text;
    m_theme = theme;

    m_jobs.SetWorker   (worker);
    m_jobs.SetOnQueued ([this] ()
    {
        UpdateBusy();

        if (m_onQueued)
        {
            m_onQueued();
        }
    });

    if (commands != nullptr && commands->goTo != nullptr)
    {
        m_tools = CreateChild<DxuiToolbar>();
        m_tools->SetTextRenderer (text);
        m_tools->SetEntries      ({ { commands->goTo, DxuiToolbar::Kind::Command, 1 } });
    }

    m_map = CreateChild<BlockMapView>();
    m_map->SetModel    (model);
    m_map->SetZoomable (true);
    m_map->SetOnSelect ([this] (uint32_t block) { OnMapSelect (block); });
    m_map->SetOnOpen   ([this] (uint32_t block) { OpenBlock (block); });
    m_map->SetOnHover  ([this] (const std::wstring & value)
    {
        if (m_onHint)
        {
            m_onHint (value);
        }
    });

    InitCorners();
    InitRightColumn (theme, hwnd);

    m_hexPane.GetHexView().SetOwnerWindow (hwnd);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::InitCorners
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::InitCorners()
{
    m_chipsGroup = CreateChild<FloatingGroup>();
    m_zoomGroup  = CreateChild<FloatingGroup>();
    m_filesChip  = m_chipsGroup->CreateChild<OverlayChip> (L"Files");
    m_diffsChip  = m_chipsGroup->CreateChild<OverlayChip> (L"Differences");
    m_zoomOut    = m_zoomGroup->CreateChild<DxuiIconButton>();
    m_zoomIn     = m_zoomGroup->CreateChild<DxuiIconButton>();

    m_diffsChip->SetVisible (false);

    m_filesChip->SetOnChange ([this] (bool isOn) { SetFilesOn (isOn); });
    m_map->SetFilesColored   (m_isFilesOn);
    m_diffsChip->SetOnChange ([this] (bool isOn) { SetDiffsOn (isOn); });

    m_zoomOut->SetGlyph          (s_kpszGlyphMinus);
    m_zoomOut->SetRound          (true);
    m_zoomOut->SetAccessibleName (L"Zoom out");
    m_zoomOut->SetOnClick        ([this] () { m_map->StepZoom (-1); });
    m_zoomIn->SetGlyph           (s_kpszGlyphPlus);
    m_zoomIn->SetRound           (true);
    m_zoomIn->SetAccessibleName  (L"Zoom in");
    m_zoomIn->SetOnClick         ([this] () { m_map->StepZoom (1); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::InitRightColumn
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::InitRightColumn (const IDxuiTheme * theme, HWND hwnd)
{
    m_header.Create (*this, [this] (int64_t delta) { Step (delta); }, [this] () { if (m_onCopy) { m_onCopy(); } }, [this] () { m_jobs.CancelJobs(); });

    m_hexPane.Create (*this);

    m_nav.Create         (*this, m_model, theme, hwnd, m_text);
    m_nav.SetOnPlace     ([this] () { OnLevelChanged(); });
    m_nav.SetOnOpenBlock ([this] (uint32_t block) { OpenBlock (block); });
    m_nav.SetOnHighlight ([this] ()
    {
        const BlockListRow  * row = m_nav.GetSelectedRow();

        //  The selected row's first block is drawn below the list.
        if (row != nullptr && !m_isMapSelecting && !IsBusy() && row->first < m_model->GetBlockCount() && row->first != m_model->GetBlock())
        {
            m_model->ReplaceBlock (row->first);
            Refresh();
        }

        m_map->Refresh();

        //  A new highlight chosen anywhere but on the map starts a flight to
        //  it, as a sector chosen off the platter does. A block clicked on
        //  the map or gone to starts one too when its row covers several
        //  blocks, such as the volume directory's for block 2.
        if (m_isMapSelecting && !m_map->HasSeveralHighlighted())
        {
            m_map->StopFlight();
        }
        else
        {
            m_map->StartFlight (InspectorMarquee::GetClockMs());
        }

        if (m_onChanged)
        {
            m_onChanged();
        }
    });

    BlockHexView     & hex    = m_hexPane.GetHexView();
    BlockFieldsView  & fields = m_hexPane.GetFields();

    hex.SetOnType       ([this] (uint64_t offset, bool isHex, bool isHigh, wchar_t ch) { return OnType (offset, isHex, isHigh, ch); });
    hex.SetOnNavigate   ([this] (const DxuiKeyEvent & ev) { return OnNavigateKey (ev); });
    hex.SetOnClick      ([this] (uint64_t offset)
    {
        if (!IsBusy() && m_model->IsLoaded (m_model->GetBlock()) && m_model->TryFollowLink ((int) offset))
        {
            m_isRecording = true;
            Refresh();
        }
    });

    //  A double-click on a byte starts editing with the caret on it.
    hex.SetOnOpen ([this] (uint64_t offset)
    {
        if (!m_isEditing && !IsBusy() && m_model->IsOpen())
        {
            SetEditing (true);
            m_hexPane.GetHexView().GoToOffset (offset);
        }
    });

    fields.SetOnHover ([this] (const BlockField * field)
    {
        if (!IsBusy())
        {
            m_hexPane.MarkField (*m_model, field);

            if (m_onChanged)
            {
                m_onChanged();
            }
        }
    });

    fields.SetOnFollow ([this] (const BlockField * field)
    {
        if (field != nullptr && field->link != BlockField::kNoLink && !IsBusy())
        {
            SelectBlock ((uint32_t) field->link);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::TryParseBlock
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::TryParseBlock (const std::wstring & text, uint32_t & outBlock)
{
    constexpr int  kHex     = 16;
    constexpr int  kDecimal = 10;



    std::wstring  trimmed = text;
    wchar_t     * end     = nullptr;
    int           base    = kDecimal;
    uint64_t      value   = 0;



    trimmed.erase (0, trimmed.find_first_not_of (L' '));
    trimmed.erase (trimmed.find_last_not_of (L' ') + 1);

    if (!trimmed.empty() && trimmed[0] == L'$')
    {
        trimmed.erase (0, 1);
        base = kHex;
    }
    else
    {
        //  A decimal block number may have thousands separators.
        trimmed = NumberFormat::RemoveThousandsSeparators (trimmed);
    }

    if (trimmed.empty() || !iswxdigit (trimmed[0]))
    {
        return false;
    }

    value = wcstoull (trimmed.c_str(), &end, base);

    if (end == nullptr || *end != L'\0' || value > UINT32_MAX)
    {
        return false;
    }

    outBlock = (uint32_t) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SelectBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SelectBlock (uint32_t block)
{
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    m_isRecording = true;
    m_model->SelectBlock (block);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OpenBlock
//
//  A double-click on a block, on the map or on a row of blocks: the block
//  is drawn below the list, which stays where it is, and the hex view gets
//  the keyboard.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::OpenBlock (uint32_t block)
{
    if (IsBusy() || !m_model->IsOpen() || block >= m_model->GetBlockCount())
    {
        return;
    }

    m_model->ReplaceBlock (block);
    Refresh();

    if (m_onPressed)
    {
        m_onPressed (&m_hexPane.GetHexView());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SetEditing
//
//  Starting to edit moves the keys to the hex view, as Edit sector does for
//  a sector.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SetEditing (bool isEditing)
{
    m_isEditing = isEditing && m_model->IsOpen();

    if (m_isEditing && m_onPressed)
    {
        m_onPressed (&m_hexPane.GetHexView());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SetEditState
//
//  The edit controls, and the layout again when the controls in the
//  header's second row change.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SetEditState (const BlockEditState & state)
{
    if (m_header.SetEditState (state) && m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Step
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Step (int64_t delta)
{
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    m_isStepping = true;
    m_model->StepBlock (delta);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GoBack
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::GoBack()
{
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    if (m_nav.GoBack())
    {
        OnLevelChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GoForward
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::GoForward()
{
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    if (m_nav.GoForward())
    {
        OnLevelChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnNavigateKey
//
//  Page Up and Page Down step a block; Alt+Left and Alt+Right go back and
//  forward.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnNavigateKey (const DxuiKeyEvent & ev)
{
    if (ev.vk == VK_PRIOR)
    {
        Step (-1);
    }
    else if (ev.vk == VK_NEXT)
    {
        Step (1);
    }
    else if (ev.alt && ev.vk == VK_LEFT)
    {
        GoBack();
    }
    else if (ev.alt && ev.vk == VK_RIGHT)
    {
        GoForward();
    }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnType
//
//  Typing changes the block only while editing, as Edit block turns on.
//  The header isn't edited here, and a block is edited only once it is
//  loaded, so typing reads the block from memory.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnType (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)
{
    bool  isChanged = false;



    if (!m_isEditing || IsBusy() || !m_model->IsOpen() || !m_model->IsLoaded (m_model->GetBlock()))
    {
        return false;
    }

    isChanged = isHexColumn ? m_model->TypeHexDigit ((int) offset, isHighNibble, ch)
                            : m_model->TypeCharacter ((int) offset, ch);

    if (isChanged)
    {
        Refresh();
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SetFilesOn
//
//  The Files overlay draws each file's blocks in a different color, and
//  at a block, highlights the blocks of the block's file.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SetFilesOn (bool isOn)
{
    if (IsBusy())
    {
        return;
    }

    m_isFilesOn = isOn;
    m_map->SetFilesColored (isOn);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SetDiffsOn
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SetDiffsOn (bool isOn)
{
    m_isDiffsOn = isOn;
    m_map->SetDiffsShown (isOn);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::IsBlockLevel
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::IsBlockLevel() const
{
    return m_nav.IsBlockPlace();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnLevelChanged
//
//  The levels went to another location: at the Block level, its block is
//  selected and drawn once loaded; at any other, the grid is drawn with
//  the level's highlight, and the block drawn there before, after Back or
//  Forward, is drawn below the list again.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::OnLevelChanged()
{
    uint32_t  drawn = m_nav.GetBrowser().GetDrawnBlock();



    if (IsBlockLevel() && m_model->GetBlock() != m_nav.GetPlace().block)
    {
        m_model->SelectBlock (m_nav.GetPlace().block);
    }
    else if (!IsBlockLevel() && m_model->GetBlock() != drawn && drawn < m_model->GetBlockCount())
    {
        m_model->ReplaceBlock (drawn);
    }

    if (m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnMapSelect
//
//  A click on the map: at a block, the clicked block opens in its place;
//  anywhere else, the row that covers the block is selected, and the block
//  becomes the current one, marked on the map.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::OnMapSelect (uint32_t block)
{
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    if (IsBlockLevel())
    {
        SelectBlock (block);
        return;
    }

    m_isMapSelecting = true;
    m_nav.SelectForBlock (block);
    m_isMapSelecting = false;
    m_model->ReplaceBlock (block);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GoToBlock
//
//  Go to block: the block becomes the current one, and the list and the
//  address bar move to the file or structure that uses it, as for a click
//  on the map.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::GoToBlock (uint32_t block)
{
    if (IsBusy() || !m_model->IsOpen() || block >= m_model->GetBlockCount())
    {
        return;
    }

    m_isMapSelecting = true;
    m_nav.SelectForBlock (block);
    m_isMapSelecting = false;

    m_model->SelectBlock (block);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Refresh
//
//  A block not loaded yet is loaded by a job, and drawn when it ends. The
//  last block drawn stays on screen until then.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Refresh()
{
    BlockInspectorModel  * model = m_model;
    uint32_t               block = 0;



    if (m_model == nullptr || IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    block = m_model->GetBlock();

    if (!m_model->IsLoaded (block))
    {
        m_jobs.QueueJob ({ L"", false,
                    [model, block] (BlockJobProgress & progress) { (void) progress; model->LoadBlock (block); },
                    [this] () { Refresh(); } });
        return;
    }

    Present();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::UpdateBusy
//
//  While a long job is queued or running, every control but Cancel is
//  disabled and the grid isn't laid out again from the model. A short job,
//  such as loading a block, changes nothing on screen.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::UpdateBusy()
{
    bool  wasLong = m_jobs.IsLongJob();
    bool  isLong  = false;



    if (!m_jobs.HasWorker())
    {
        return;
    }

    m_jobs.Poll();
    isLong = m_jobs.IsLongJob();

    m_map->SetFrozen     (IsBusy());
    m_header.SetBusy     (isLong, m_jobs.CanCancel());

    for (IDxuiControl * control : { (IDxuiControl *) &m_nav.GetList(), (IDxuiControl *) &m_nav.GetTree(), (IDxuiControl *) m_filesChip, (IDxuiControl *) m_diffsChip })
    {
        control->SetEnabled (!isLong);
    }

    if (wasLong != isLong && m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }

    if (m_onChanged && (isLong || wasLong))
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::ApplyHighlight
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::ApplyHighlight()
{
    if (!IsBlockLevel())
    {
        return;
    }

    if (m_isFilesOn)
    {
        (void) m_model->HighlightFileAt (m_model->GetBlock());
    }
    else
    {
        m_model->ClearHighlight();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Present
//
//  The current block, loaded, and the controls set from the model.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Present()
{
    bool  isComparing = m_model->IsComparing();
    bool  isRelayout  = isComparing != m_diffsChip->IsVisible();



    //  The levels follow an image that opened or maps read again, then a
    //  block selected on the grid or by a link goes in the Back history.
    m_nav.Sync();

    if (m_isRecording)
    {
        m_nav.ShowBlock (m_model->GetBlock());
    }
    else if (m_isStepping)
    {
        m_nav.ReplaceBlock (m_model->GetBlock());
    }

    isRelayout    = isRelayout || m_isRecording || m_isStepping || (int) m_nav.GetPlace().kind != m_laidOutLevel;
    m_isRecording = false;
    m_isStepping  = false;

    m_nav.GetBrowser().SetDrawnBlock (m_model->GetBlock());

    ApplyHighlight();
    ShowBytes();
    m_header.SetFieldLine (BlockHeaderText::BuildFieldLine (*m_model, m_model->GetBlock()));
    m_header.SetSteps     (m_model->GetBlock() > 0, m_model->GetBlock() + 1 < m_model->GetBlockCount());
    m_filesChip->SetOn   (m_isFilesOn);
    m_diffsChip->SetOn   (m_isDiffsOn);
    m_diffsChip->SetVisible (isComparing);

    if (isRelayout && m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }

    m_map->Refresh();

    if (m_onChanged)
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::ShowBytes
//
//  The block's number, its capsules and its bytes. The fields list is
//  fitted again for each new block.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::ShowBytes()
{
    m_header.ShowBlock (m_model->GetBlock(), BlockHeaderText::BuildCapsules (*m_model, m_model->GetBlock()));

    if (m_hexPane.ShowBytes (*m_model))
    {
        FitFields();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::BuildBlockText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorPanel::BuildBlockText() const
{
    BlockInspectorModel::Block  bytes = {};
    HRESULT                     hr    = S_OK;



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return std::wstring();
    }

    hr = m_model->ReadBlock (m_model->GetBlock(), bytes);

    return SUCCEEDED (hr) ? BlockHeaderText::BuildBlockText (bytes, BlockHexPane::kBytesPerRow) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GetSelectionLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorPanel::GetSelectionLine() const
{
    return m_nav.GetSelectionLine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Layout
//
//  The address bar, the search box and Go to across the top; under them
//  the tree, a splitter, the list or the block, a splitter, and the map.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.ToPx ((int) kPadDip);
    int   row      = scaler.ToPx ((int) BlockHeaderView::kRowDip);
    int   toolbar  = std::max (m_tools != nullptr ? scaler.ToPx (m_tools->GetBandDp()) : 0, row + pad);
    int   splitter = scaler.ToPx ((int) kSplitterDip);
    int   search   = scaler.ToPx ((int) kSearchDip);
    int   top      = boundsPx.top + toolbar;
    int   width    = (int) (boundsPx.right - boundsPx.left);
    int   tools    = 0;
    int   treeX    = 0;
    int   mapX     = 0;
    int   barTop   = boundsPx.top + (toolbar - row) / 2;



    SetBounds (boundsPx);
    m_scaler = scaler;

    if (m_tools != nullptr)
    {
        (void) m_tools->PlanForWidth (std::max (0, width - 2 * pad), scaler);
        tools = m_tools->GetPlannedWidthPx();
        m_tools->Layout ({ boundsPx.right - pad - tools, boundsPx.top, boundsPx.right - pad, top }, scaler);
    }

    m_nav.GetCrumbs().Layout ({ boundsPx.left + pad, barTop, boundsPx.right - 2 * pad - tools - search - pad, barTop + row }, scaler);
    m_nav.GetSearch().SetDpi (scaler.GetDpi());
    m_nav.GetSearch().Layout ({ boundsPx.right - 2 * pad - tools - search, barTop, boundsPx.right - 2 * pad - tools, barTop + row }, scaler);

    treeX = m_splitters.PlaceColumns (boundsPx, mapX);

    m_splitters.SetBarRect (BlockPanelSplitters::Bar::Tree, { treeX, top, treeX + splitter, boundsPx.bottom });
    m_splitters.SetBarRect (BlockPanelSplitters::Bar::Map,  { mapX, top, mapX + splitter, boundsPx.bottom });

    m_nav.GetTree().SetDpi (scaler.GetDpi());
    m_nav.GetTree().Layout ({ boundsPx.left + pad, top + pad, treeX, boundsPx.bottom - pad }, scaler);
    m_nav.GetList().SetDpi (scaler.GetDpi());

    LayoutMiddle ({ treeX + splitter + pad, top + pad, mapX - pad, boundsPx.bottom - pad }, scaler);
    LayoutGrid   ({ mapX + splitter + pad, top + pad, boundsPx.right - pad, boundsPx.bottom - pad }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::LayoutGrid
//
//  The grid fills its column above a band at the bottom, for the overlays
//  and the zoom, so the groups don't cover blocks.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::LayoutGrid (const RECT & column, const DxuiDpiScaler & scaler)
{
    int                  inset  = scaler.ToPx (s_kInsetDip);
    int                  group  = scaler.ToPx (s_kGroupDip);
    int                  pad    = scaler.ToPx (s_kGroupPadDip);
    int                  gap    = scaler.ToPx (s_kGroupGapDip);
    int                  icon   = scaler.ToPx (s_kIconDip);
    int                  band   = group + 2 * inset;
    int                  left   = column.left + inset;
    int                  right  = column.right - inset;
    int                  bottom = column.bottom - inset;
    int                  zoomW  = 3 * pad + 2 * icon;
    int                  chipsW = 2 * pad;
    int                  x      = 0;
    int                  w      = 0;
    IDxuiTextRenderer  * text   = m_text;



    for (OverlayChip * chip : { m_filesChip, m_diffsChip })
    {
        chipsW += (chip->IsVisible() && text != nullptr) ? chip->GetWidthPx (*text, scaler) + gap : 0;
    }

    m_map->Layout ({ column.left, column.top + inset, column.right, column.bottom - band }, scaler);

    m_chipsGroup->Layout ({ left, bottom - group, left + chipsW, bottom }, scaler);
    x = left + pad;

    for (OverlayChip * chip : { m_filesChip, m_diffsChip })
    {
        w = (chip->IsVisible() && text != nullptr) ? chip->GetWidthPx (*text, scaler) : 0;
        chip->Layout ({ x, bottom - group + pad, x + w, bottom - pad }, scaler);
        x += (w > 0) ? w + gap : 0;
    }

    x = right - zoomW;
    m_zoomGroup->Layout ({ x, bottom - group, right, bottom }, scaler);
    m_zoomOut->Layout   ({ x + pad, bottom - group + pad, x + pad + icon, bottom - pad }, scaler);
    m_zoomIn->Layout    ({ right - pad - icon, bottom - group + pad, right - pad, bottom - pad }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::LayoutMiddle
//
//  The list at the top, then a splitter, then the current block: its title
//  row with Copy block at its right end, the field line, the edit bar or
//  a long job's progress row, then the hex view beside the fields, with a
//  splitter between them.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::LayoutMiddle (const RECT & column, const DxuiDpiScaler & scaler)
{
    int  pad      = scaler.ToPx ((int) kPadDip);
    int  splitter = scaler.ToPx ((int) kSplitterDip);
    int  listEnd  = m_splitters.PlaceListEnd (column);
    int  y        = 0;
    int  hexEnd   = 0;



    m_laidOutLevel = (int) m_nav.GetPlace().kind;

    m_nav.GetList().SetVisible (true);
    m_nav.GetList().Layout ({ column.left, column.top, column.right, listEnd }, scaler);

    m_splitters.SetBarRect (BlockPanelSplitters::Bar::List, { column.left, listEnd + pad / 2, column.right, listEnd + pad / 2 + splitter });

    y      = m_header.Layout (column, listEnd + pad / 2 + splitter + pad / 2, m_jobs.IsLongJob(), scaler);
    hexEnd = m_splitters.PlaceHexEnd (column);

    m_splitters.SetBarRect (BlockPanelSplitters::Bar::Hex, { hexEnd + pad / 2 - splitter / 2, y, hexEnd + pad / 2 + splitter / 2 + 1, std::max (y, (int) column.bottom) });

    m_hexPane.Layout ({ column.left, y, hexEnd, std::max (y, (int) column.bottom) }, { hexEnd + pad, y, column.right, std::max (y, (int) column.bottom) }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::FitFields
//
//  For each new block, the splitter is moved so the fields list is as wide
//  as its rows, within the splitter's range, with room left in the hex view
//  for at least BlockHexPane::kMinHexBytes in a row.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::FitFields()
{
    RECT  list   = m_nav.GetList().GetBounds();
    int   width  = (int) (list.right - list.left);
    int   pad    = m_scaler.ToPx ((int) kPadDip);
    int   hexMin = 0;



    if (m_text == nullptr || width <= 0 || m_scaler.GetDpi() == 0)
    {
        return;
    }

    hexMin = m_hexPane.MeasureMinHexPx (m_scaler);

    m_splitters.FitHex ((float) (width - pad - m_hexPane.GetFields().MeasureWidthPx (*m_text)) / (float) width, (float) hexMin / (float) width);
    Layout (GetBounds(), m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GetHexTooltip
//
//  A byte's offset, then its field and the field's value, then what the
//  byte's color marks.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::GetHexTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    return m_hexPane.GetTooltip (pointPx, *m_model, outText, outAnchorPx, outSwatches);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_splitters.Paint (painter, theme);

    m_hexPane.SetEditLook (m_isEditing, m_hexPane.GetHexView().HasFocus(), { theme.Accent(), theme.ContentBackground(), theme.Foreground() });
    m_hexPane.PaintFrame  (painter);

    DxuiPanel::Paint (painter, text, theme);
    m_header.Paint (painter, text, theme, m_scaler, m_jobs, m_model != nullptr && m_model->IsOpen());

    if (m_hexPane.RefitRows (m_scaler))
    {
        if (m_onChanged)
        {
            m_onChanged();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnMouse
//
//  The splitter moves between the columns. A press on the hex view focuses
//  it. While a job is queued or running, only Cancel gets mouse events, so
//  pointing at the grid or a field doesn't read the model.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  cancel  = m_header.GetCancel().GetBounds();
    RECT  map     = m_map->GetBounds();
    RECT  bounds  = GetBounds();
    bool  isMoved = false;



    if (IsBusy())
    {
        return m_header.GetCancel().IsVisible() && PtInRect (&cancel, ev.positionDip) ? DxuiPanel::OnMouse (ev) : true;
    }

    m_hexPane.UpdateFieldTip (ev, m_text, m_theme);

    if (m_splitters.OnMouse (ev, bounds, m_nav.GetList().GetBounds(), (int) bounds.bottom - m_scaler.ToPx ((int) kPadDip), isMoved))
    {
        if (isMoved)
        {
            Layout (bounds, m_scaler);

            if (m_onChanged)
            {
                m_onChanged();
            }
        }

        return true;
    }

    //  A press anywhere else ends an edit of the address bar, as in Casso
    //  Explorer, rather than going to the address bar's text field.
    if (ev.kind == DxuiMouseEventKind::Down && m_nav.GetCrumbs().IsEditing())
    {
        RECT  crumbs = m_nav.GetCrumbs().GetBounds();

        if (!PtInRect (&crumbs, ev.positionDip))
        {
            m_nav.GetCrumbs().EndEdit();
        }
    }

    //  A right-click on the address bar, while it isn't being edited, opens
    //  its menu, as Casso Explorer's does.
    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right && !m_nav.GetCrumbs().IsEditing())
    {
        RECT  crumbs = m_nav.GetCrumbs().GetBounds();

        if (PtInRect (&crumbs, ev.positionDip))
        {
            m_nav.ShowAddressMenu (ev.positionDip.x, ev.positionDip.y, [this] ()
            {
                if (m_onPressed)
                {
                    m_onPressed (&m_nav.GetCrumbs());
                }
            });

            return true;
        }
    }

    //  A press moves the keyboard focus to the control under it, so the
    //  address bar, the search box, the tree, the list and the hex view
    //  each get keys once clicked, until a click elsewhere.
    if (ev.kind == DxuiMouseEventKind::Down && m_onPressed)
    {
        for (IDxuiControl * child : { (IDxuiControl *) &m_hexPane.GetHexView(), (IDxuiControl *) &m_nav.GetCrumbs(), (IDxuiControl *) &m_nav.GetSearch(),
                                      (IDxuiControl *) &m_nav.GetTree(), (IDxuiControl *) &m_nav.GetList() })
        {
            RECT  r = child->GetBounds();

            if (child->IsVisible() && PtInRect (&r, ev.positionDip))
            {
                m_onPressed (child);
                break;
            }
        }
    }

    //  The wheel over the grid zooms it, as the wheel over the platter does,
    //  whichever control has the keyboard. Over any other control, the wheel
    //  scrolls that control alone.
    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.ctrl && PtInRect (&map, ev.positionDip))
    {
        return m_map->OnMouse (ev);
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.ctrl)
    {
        for (IDxuiControl * child : { (IDxuiControl *) &m_hexPane.GetHexView(), (IDxuiControl *) &m_hexPane.GetFields(), (IDxuiControl *) &m_nav.GetList(), (IDxuiControl *) &m_nav.GetTree() })
        {
            RECT  r = child->GetBounds();

            if (child->IsVisible() && PtInRect (&r, ev.positionDip))
            {
                return child->OnMouse (ev);
            }
        }

        return false;
    }

    return DxuiPanel::OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockInspectorPanel::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_splitters.GetCursor (clientPx);



    return cursor != nullptr ? cursor : DxuiPanel::GetCursorForPoint (clientPx);
}
