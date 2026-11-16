#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Ui/DiskInspector/Blocks/BlockFilesView.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockLegendView.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"
#include "Ui/DiskInspector/FloatingGroup.h"
#include "Ui/DiskInspector/OverlayChip.h"
#include "Core/NumberFormat.h"





static constexpr int      s_kGroupDip       = 36;
static constexpr int      s_kGroupPadDip    = 4;
static constexpr int      s_kGroupGapDip    = 6;
static constexpr int      s_kIconDip        = 28;
static constexpr int      s_kInsetDip       = 4;
static constexpr LPCWSTR  s_kpszGlyphLegend = L"\xE790";
static constexpr LPCWSTR  s_kpszGlyphMinus  = L"\xE738";
static constexpr LPCWSTR  s_kpszGlyphPlus   = L"\xE710";
static constexpr LPCWSTR  s_kpszGlyphPrev   = L"\xE76B";
static constexpr LPCWSTR  s_kpszGlyphNext   = L"\xE76C";
static constexpr uint32_t s_kChangedColor   = 0xFFE0A030;
static constexpr uint32_t s_kDiffersColor   = 0xFFF06060;
static constexpr uint32_t s_kFieldColor     = 0xFF60B0F0;
static constexpr uint32_t s_kLinkColor      = 0xFF60D090;





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
    (void) theme;
    (void) hwnd;

    m_model  = model;
    m_worker = worker;
    m_text   = text;

    if (commands != nullptr && commands->goTo != nullptr)
    {
        m_tools = CreateChild<DxuiToolbar>();
        m_tools->SetTextRenderer (text);
        m_tools->SetEntries      ({ { commands->goTo, DxuiToolbar::Kind::Command, 1 } });
    }

    m_map = CreateChild<BlockMapView>();
    m_map->SetModel    (model);
    m_map->SetZoomable (true);
    m_map->SetOnSelect ([this] (uint32_t block) { SelectBlock (block); });
    m_map->SetOnHover  ([this] (const std::wstring & value)
    {
        if (m_onHint)
        {
            m_onHint (value);
        }
    });

    InitCorners();
    InitRightColumn();

    m_hex->SetOwnerWindow (hwnd);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::InitCorners
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::InitCorners()
{
    m_legendGroup  = CreateChild<FloatingGroup>();
    m_chipsGroup   = CreateChild<FloatingGroup>();
    m_zoomGroup    = CreateChild<FloatingGroup>();
    m_legendButton = m_legendGroup->CreateChild<DxuiIconButton>();
    m_filesChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Files");
    m_diffsChip    = m_chipsGroup->CreateChild<OverlayChip> (L"Differences");
    m_zoomOut      = m_zoomGroup->CreateChild<DxuiIconButton>();
    m_zoomIn       = m_zoomGroup->CreateChild<DxuiIconButton>();
    m_legend       = CreateChild<BlockLegendView>();

    m_legend->SetVisible    (false);
    m_diffsChip->SetVisible (false);

    m_legendButton->SetGlyph          (s_kpszGlyphLegend);
    m_legendButton->SetRound          (true);
    m_legendButton->SetAccessibleName (L"Legend");
    m_legendButton->SetOnClick        ([this] () { ToggleLegend(); });

    m_filesChip->SetOnChange ([this] (bool isOn) { SetFilesOn (isOn); });
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

void BlockInspectorPanel::InitRightColumn()
{
    m_views    = CreateChild<DxuiSegmentedControl>();
    m_previous = CreateChild<DxuiIconButton>();
    m_next     = CreateChild<DxuiIconButton>();
    m_copy     = CreateChild<DxuiButton> (L"Copy block");
    m_editNote = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_discard  = CreateChild<DxuiButton> (L"Discard edits");
    m_apply    = CreateChild<DxuiButton> (L"Apply edits");
    m_cancel   = CreateChild<DxuiButton> (L"Cancel");
    m_hex      = CreateChild<BlockHexView>();
    m_fields   = CreateChild<BlockFieldsView>();
    m_files    = CreateChild<BlockFilesView>();

    m_views->SetSegments ({ L"Files", L"Blocks" });
    m_views->SetSelected (kViewFiles);
    m_views->SetOnChange ([this] (int index) { ShowView (static_cast<View> (index)); });

    m_previous->SetGlyph          (s_kpszGlyphPrev);
    m_previous->SetAccessibleName (L"Previous block");
    m_previous->SetOnClick        ([this] () { Step (-1); });
    m_next->SetGlyph              (s_kpszGlyphNext);
    m_next->SetAccessibleName     (L"Next block");
    m_next->SetOnClick            ([this] () { Step (1); });

    m_copy->SetOnClick    ([this] () { if (m_onCopy) { m_onCopy(); } });
    m_discard->SetOnClick ([this] () { if (m_onDiscard) { m_onDiscard(); } });
    m_apply->SetOnClick   ([this] () { if (m_onApply) { m_onApply(); } });
    m_cancel->SetOnClick  ([this] () { if (m_worker != nullptr) { m_worker->Cancel(); } });

    m_editNote->SetVisible (false);
    m_discard->SetVisible  (false);
    m_apply->SetVisible    (false);
    m_cancel->SetVisible   (false);
    m_files->SetVisible    (false);

    m_hex->SetBytesPerRow  (kBytesPerRow);
    m_hex->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hex->SetOnType       ([this] (uint64_t offset, bool isHex, bool isHigh, wchar_t ch) { return OnType (offset, isHex, isHigh, ch); });
    m_hex->SetOnNavigate   ([this] (const DxuiKeyEvent & ev) { return OnNavigateKey (ev); });
    m_hex->SetOnClick      ([this] (uint64_t offset)
    {
        if (!IsBusy() && m_view == kViewBlocks && m_model->IsLoaded (m_model->GetBlock()) && m_model->TryFollowLink ((int) offset))
        {
            Refresh();
        }
    });

    m_hex->SetMarkColor ([] (uint8_t mark, uint32_t & outArgb)
    {
        outArgb = mark == BlockHexSource::kMarkChanged ? s_kChangedColor
                : mark == BlockHexSource::kMarkDiffers ? s_kDiffersColor
                : mark == BlockHexSource::kMarkField   ? s_kFieldColor
                :                                        s_kLinkColor;

        return mark != BlockHexSource::kMarkNone;
    });

    m_fields->SetOnHover ([this] (const BlockField * field)
    {
        if (!IsBusy())
        {
            MarkField (field);

            if (m_onChanged)
            {
                m_onChanged();
            }
        }
    });

    m_fields->SetOnFollow ([this] (const BlockField * field)
    {
        if (field != nullptr && field->link != BlockField::kNoLink && m_view == kViewBlocks && !IsBusy())
        {
            SelectBlock ((uint32_t) field->link);
        }
    });

    m_files->SetOnSelect ([this] (const BlockFilesView::Row & row)
    {
        if (IsBusy())
        {
            return;
        }

        //  The file's blocks are highlighted on the grid. The tab doesn't
        //  switch to Blocks, so another file can be picked.
        m_isFilesOn = true;
        SelectBlock (row.first);
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

    m_model->SelectBlock (block);
    Refresh();
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

    m_model->GoBack();
    Refresh();
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

    m_model->GoForward();
    Refresh();
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
//  The header isn't edited here, and a block is edited only once it is
//  loaded, so typing reads the block from memory.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnType (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)
{
    bool  isChanged = false;



    if (m_view != kViewBlocks || IsBusy() || !m_model->IsOpen() || !m_model->IsLoaded (m_model->GetBlock()))
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
//  The Files overlay highlights the blocks of the current block's file, and
//  follows the current block.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SetFilesOn (bool isOn)
{
    if (IsBusy())
    {
        return;
    }

    m_isFilesOn = isOn;
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
//  BlockInspectorPanel::HasHeader
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::HasHeader() const
{
    return m_model != nullptr && m_model->IsOpen() && !m_model->GetFile().GetHeader().empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::ShowView
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::ShowView (View view)
{
    if (IsBusy() || (view == kViewHeader && !HasHeader()))
    {
        m_views->SetSelected (m_view);
        return;
    }

    m_view       = view;
    m_shownBlock = -1;
    m_views->SetSelected (view);

    if (m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::ToggleLegend
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::ToggleLegend()
{
    m_legend->SetVisible (!m_legend->IsVisible());

    if (m_onChanged)
    {
        m_onChanged();
    }
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
        QueueJob ({ L"", false,
                    [model, block] (BlockJobProgress & progress) { (void) progress; model->LoadBlock (block); },
                    [this] () { Refresh(); } });
        return;
    }

    Present();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::QueueJob
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::QueueJob (BlockInspectorWorker::Job job)
{
    BlockJobProgress  progress;



    if (m_worker == nullptr)
    {
        job.work (progress);
        job.finish();
        return;
    }

    m_worker->Queue (std::move (job));
    UpdateBusy();

    if (m_onQueued)
    {
        m_onQueued();
    }
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
    bool  canCancel = false;
    bool  wasLong   = m_isLongJob;



    if (m_worker == nullptr)
    {
        return;
    }

    m_isLongJob = m_worker->TryGetLongJob (m_jobLabel, canCancel, m_jobDone, m_jobTotal);

    m_map->SetFrozen     (IsBusy());
    m_cancel->SetVisible (m_isLongJob && canCancel);

    for (IDxuiControl * control : { (IDxuiControl *) m_views, (IDxuiControl *) m_copy, (IDxuiControl *) m_discard, (IDxuiControl *) m_apply,
                                    (IDxuiControl *) m_filesChip, (IDxuiControl *) m_diffsChip })
    {
        control->SetEnabled (!m_isLongJob);
    }

    if (wasLong != m_isLongJob && m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }

    if (m_onChanged && (m_isLongJob || wasLong))
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
    bool  hasEdits    = !m_model->GetEdits().IsEmpty();
    bool  isRelayout  = hasEdits != m_hasEdits || isComparing != m_diffsChip->IsVisible();



    ApplyHighlight();
    ShowBytes();
    BuildFieldLine();
    FillFiles();

    //  The Header tab is offered only for an image with a container header.
    if (HasHeader() != m_hasHeader)
    {
        m_hasHeader = HasHeader();
        m_views->SetSegments (m_hasHeader ? vector<std::wstring> { L"Files", L"Blocks", L"Header" } : vector<std::wstring> { L"Files", L"Blocks" });
        m_views->SetSelected (m_view);
    }

    m_previous->SetEnabled (m_model->GetBlock() > 0);
    m_next->SetEnabled     (m_model->GetBlock() + 1 < m_model->GetBlockCount());
    m_filesChip->SetOn   (m_isFilesOn);
    m_diffsChip->SetOn   (m_isDiffsOn);
    m_diffsChip->SetVisible (isComparing);
    m_editNote->SetText  (NumberFormat::FormatInteger (m_model->GetEdits().CountChangedBytes()) + (m_model->GetEdits().CountChangedBytes() == 1 ? L" byte changed, not written" : L" bytes changed, not written"));

    m_hasEdits = hasEdits;

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
//  BlockInspectorPanel::BuildCapsules
//
//  Free or Used, then the block's role for a block in use, drawn red for a
//  damaged role, then each file the block is in.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::BuildCapsules()
{
    uint32_t      block  = m_model->GetBlock();
    SectorRole    role   = m_model->GetRole (block);
    LPCWSTR       label  = m_model->GetMapLabel (block);
    bool          isFree = role == SectorRole::Free && label == nullptr;
    bool          isBad  = role == SectorRole::AllocatedUnowned || role == SectorRole::OwnedMarkedFree || role == SectorRole::CrossLinked;
    std::wstring  owners = m_model->GetOwners (block);
    size_t        start  = 0;
    size_t        end    = 0;



    m_capsules.clear();
    m_capsules.push_back ({ isFree ? L"Free" : L"Used", false });

    if (!isFree)
    {
        m_capsules.push_back ({ label != nullptr ? label : BlockMapGrid::GetRoleName (role), isBad });
    }

    while (start < owners.size())
    {
        end = owners.find (L'\n', start);
        end = end == std::wstring::npos ? owners.size() : end;

        m_capsules.push_back ({ owners.substr (start, end - start), false });
        start = end + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::ShowBytes
//
//  The block's bytes, or the header's, with their marks and fields. The hex
//  view starts over at the top only when the block changes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::ShowBytes()
{
    BlockInspectorModel::Block  bytes  = {};
    BlockInspectorModel::Block  other  = {};
    vector<BlockField>          fields;
    uint32_t                    block  = m_model->GetBlock();
    bool                        header = m_view == kViewHeader;
    int64_t                     shown  = header ? -2 : (int64_t) block;
    HRESULT                     hr     = S_OK;
    size_t                      i      = 0;



    m_title = L"Block " + NumberFormat::FormatInteger (block);
    BuildCapsules();

    if (header)
    {
        m_source.SetBytes (m_model->GetFile().GetHeader());
        fields = m_model->GetHeaderFields();
        m_fields->SetHeading   (L"Fields in the container header");
        m_fields->SetEmptyText (L"There are no fields in the container header.");
    }
    else
    {
        hr = m_model->ReadBlock (block, bytes);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_source.SetBytes (bytes);
        m_model->GetFields (block, bytes, fields);
        m_fields->SetHeading   (L"Fields in block " + NumberFormat::FormatInteger (block));
        m_fields->SetEmptyText (L"There are no fields in this block.");

        for (i = 0; i < bytes.size(); i++)
        {
            if (m_model->GetEdits().IsChanged (block, (int) i))
            {
                m_source.SetMark (i, BlockHexSource::kMarkChanged);
            }
        }

        if (m_model->IsDifferent (block))
        {
            hr = m_model->ReadOtherBlock (block, other);
            IGNORE_RETURN_VALUE (hr, S_OK);

            for (i = 0; i < bytes.size(); i++)
            {
                if (bytes[i] != other[i])
                {
                    m_source.SetMark (i, BlockHexSource::kMarkDiffers);
                }
            }
        }
    }

    for (const BlockField & field : fields)
    {
        if (field.link == BlockField::kNoLink)
        {
            continue;
        }

        for (i = field.offset; i < (size_t) field.offset + field.length; i++)
        {
            m_source.SetMark (i, BlockHexSource::kMarkLink);
        }

        if (field.pairOffset >= 0)
        {
            m_source.SetMark ((uint64_t) field.pairOffset, BlockHexSource::kMarkLink);
        }
    }

    m_fields->SetFields (std::move (fields));

    if (shown != m_shownBlock)
    {
        m_shownBlock = shown;
        m_hex->SetSource (&m_source);
        m_hex->ResetNibble();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::MarkField
//
//  The field under the pointer, marked over the link marks.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::MarkField (const BlockField * field)
{
    uint32_t  i = 0;



    ShowBytes();

    if (field == nullptr)
    {
        return;
    }

    for (i = field->offset; i < field->offset + field->length; i++)
    {
        m_source.SetMark (i, BlockHexSource::kMarkField);
    }

    if (field->pairOffset >= 0)
    {
        m_source.SetMark ((uint64_t) field->pairOffset, BlockHexSource::kMarkField);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::BuildFieldLine
//
//  The field line: the block's Disk Copy tag bytes, and whether the block
//  differs from the other image.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::BuildFieldLine()
{
    uint32_t                    block  = m_model->GetBlock();
    BlockInspectorModel::Tags   tags   = {};
    size_t                      differ = m_model->IsComparing() ? m_model->CountDifferent() : 0;
    std::wstring                other  = m_model->IsComparing() ? fs::path (m_model->GetOther()->GetPath()).filename().wstring() : std::wstring();
    vector<std::wstring>        parts;
    wchar_t                     hex[4] = {};
    std::wstring                tagText;



    if (m_model->TryGetTags (block, tags))
    {
        for (Byte tag : tags)
        {
            swprintf_s (hex, L" %02X", tag);
            tagText += hex;
        }

        parts.push_back (L"Tag bytes:" + tagText);
    }

    if (m_model->IsComparing())
    {
        parts.push_back ((m_model->IsDifferent (block) ? L"Differs from " : L"Same as ") + other
                         + (differ == 0 ? std::wstring (L" (no blocks differ)") : differ == 1 ? std::wstring (L" (1 block differs)") : std::format (L" ({} blocks differ)", NumberFormat::FormatInteger (differ))));
    }

    m_fieldLine.clear();

    for (const std::wstring & part : parts)
    {
        m_fieldLine += (m_fieldLine.empty() ? L"" : L"    ") + part;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::FillFiles
//
//  The files and directories of the current block's volume, in the map's
//  order. A row is a directory when its first block is a directory's. The
//  highlighted file's row is selected.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::FillFiles()
{
    uint32_t                     inner = 0;
    uint32_t                     block = m_model->GetBlock();
    const BlockMap             * map   = m_model->FindMap (block, inner);
    vector<BlockFilesView::Row>  rows;
    vector<uint32_t>             used;
    uint32_t                     first = 0;
    SectorRole                   role  = SectorRole::Free;
    int                          i     = 0;



    for (i = 0; map != nullptr && i < (int) map->files.size(); i++)
    {
        used = map->GetBlocksOf (i);

        if (!used.empty())
        {
            first = block - inner + used[0];
            role  = m_model->GetRole (first);

            rows.push_back ({ map->files[i], role == SectorRole::CatalogOrDirectory || role == SectorRole::Subdirectory, (uint32_t) used.size(), first });
        }
    }

    m_files->SetRows     (std::move (rows));
    m_files->SetSelected (m_model->HasHighlight() ? m_model->GetHighlightedPath() : std::wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::BuildBlockText
//
//  Sixteen bytes a line: the offset, the bytes in hex, and their text.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorPanel::BuildBlockText() const
{
    constexpr Byte  kHighBit    = 0x80;
    constexpr Byte  kFirstGlyph = 0x20;
    constexpr Byte  kLastGlyph  = 0x7E;



    BlockInspectorModel::Block  bytes = {};
    std::wstring                text;
    size_t                      i     = 0;
    size_t                      j     = 0;
    Byte                        ch    = 0;
    HRESULT                     hr    = S_OK;



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return text;
    }

    hr = m_model->ReadBlock (m_model->GetBlock(), bytes);

    if (FAILED (hr))
    {
        return text;
    }

    for (i = 0; i < bytes.size(); i += kBytesPerRow)
    {
        text += std::format (L"{:04X} ", i);

        for (j = 0; j < (size_t) kBytesPerRow; j++)
        {
            text += std::format (L" {:02X}", bytes[i + j]);
        }

        text += L"  ";

        for (j = 0; j < (size_t) kBytesPerRow; j++)
        {
            ch    = (Byte) (bytes[i + j] & ~kHighBit);
            text += (ch >= kFirstGlyph && ch <= kLastGlyph) ? (wchar_t) ch : L'.';
        }

        text += L"\r\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GetSelectionLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockInspectorPanel::GetSelectionLine() const
{
    return m_model != nullptr && m_model->IsOpen() ? L"Selected: block " + NumberFormat::FormatInteger (m_model->GetBlock()) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Layout
//
//  The toolbar row across the top; under it the grid's column, the
//  splitter, and the right column, as in the track inspector.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.ToPx ((int) kPadDip);
    int   toolbar  = m_tools != nullptr ? scaler.ToPx (m_tools->GetBandDp()) : 0;
    int   splitter = scaler.ToPx ((int) kSplitterDip);
    int   top      = boundsPx.top + toolbar;
    int   tools    = 0;
    int   splitX   = 0;



    SetBounds (boundsPx);
    m_scaler = scaler;

    if (m_tools != nullptr)
    {
        (void) m_tools->PlanForWidth (std::max (0, (int) (boundsPx.right - boundsPx.left) - 2 * pad), scaler);
        tools = m_tools->GetPlannedWidthPx();
        m_tools->Layout ({ boundsPx.right - pad - tools, boundsPx.top, boundsPx.right - pad, top }, scaler);
    }

    m_split      = std::clamp (m_split, kMinSplit, kMaxSplit);
    splitX       = boundsPx.left + (int) ((boundsPx.right - boundsPx.left) * m_split);
    m_splitterPx = { splitX, top, splitX + splitter, boundsPx.bottom };

    LayoutGrid  ({ boundsPx.left + pad, top + pad, splitX - pad, boundsPx.bottom - pad }, scaler);
    LayoutRight ({ m_splitterPx.right + pad, top + pad, boundsPx.right - pad, boundsPx.bottom - pad }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::LayoutGrid
//
//  The grid fills its column between a band at the top, for the legend, and
//  a band at the bottom, for the overlays and the zoom, so the groups don't
//  cover blocks.
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
    int                  top    = column.top + inset;
    int                  right  = column.right - inset;
    int                  bottom = column.bottom - inset;
    int                  zoomW  = 3 * pad + 2 * icon;
    int                  chipsW = 2 * pad;
    int                  x      = 0;
    int                  w      = 0;
    SIZE                 legend = m_legend->GetSizePx (scaler);
    IDxuiTextRenderer  * text   = m_text;



    for (OverlayChip * chip : { m_filesChip, m_diffsChip })
    {
        chipsW += (chip->IsVisible() && text != nullptr) ? chip->GetWidthPx (*text, scaler) + gap : 0;
    }

    m_map->Layout ({ column.left, column.top + band, column.right, column.bottom - band }, scaler);

    m_legendGroup->Layout  ({ left, top, left + group, top + group }, scaler);
    m_legendButton->Layout ({ left + pad, top + pad, left + group - pad, top + group - pad }, scaler);
    m_legend->Layout       ({ left, top + group + inset, left + legend.cx, top + group + inset + legend.cy }, scaler);

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
//  BlockInspectorPanel::LayoutRight
//
//  From the top: the title row with Copy block at its right end, the files
//  line, the view tabs, the field line, the edit or progress row when there
//  is one, then the hex view over the fields, or the Files list.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::LayoutRight (const RECT & column, const DxuiDpiScaler & scaler)
{
    int   pad     = scaler.ToPx ((int) kPadDip);
    int   row     = scaler.ToPx ((int) kRowDip);
    int   line    = scaler.ToPx ((int) kLineDip);
    int   button  = scaler.ToPx ((int) kButtonDip);
    int   y       = column.top;
    bool  isRow   = m_isLongJob || m_hasEdits;
    bool  isFiles = m_view == kViewFiles;
    int   hexEnd  = 0;



    m_previous->Layout ({ column.left, y, column.left + row, y + row }, scaler);
    m_next->Layout     ({ column.left + row, y, column.left + 2 * row, y + row }, scaler);
    m_titleRect = { column.left + 2 * row + pad, y, column.right - button - pad, y + row };
    m_copy->Layout ({ column.right - button, y, column.right, y + row }, scaler);
    y += row + pad;

    m_views->Layout ({ column.left, y, column.left + scaler.ToPx ((int) kViewsDip), y + row }, scaler);
    y += row + pad;

    m_fieldRect = { column.left, y, column.right, y + line };
    y += line + pad;

    m_rowRect = isRow ? RECT { column.left, y, column.right, y + row } : RECT { column.left, y, column.right, y };

    m_editNote->SetVisible (m_hasEdits && !m_isLongJob);
    m_discard->SetVisible  (m_hasEdits && !m_isLongJob);
    m_apply->SetVisible    (m_hasEdits && !m_isLongJob);

    m_editNote->Layout ({ column.left, y, column.right - 2 * button - 2 * pad, y + row }, scaler);
    m_discard->Layout  ({ column.right - 2 * button - pad, y, column.right - button - pad, y + row }, scaler);
    m_apply->Layout    ({ column.right - button, y, column.right, y + row }, scaler);
    m_cancel->Layout   ({ column.right - button, y, column.right, y + row }, scaler);
    y = m_rowRect.bottom + (isRow ? pad : 0);

    hexEnd = y + (int) ((column.bottom - y) * kHexShare);

    m_hex->SetVisible    (!isFiles);
    m_fields->SetVisible (!isFiles);
    m_files->SetVisible  (isFiles);
    m_copy->SetVisible   (!isFiles);

    m_hex->Layout    ({ column.left, y, column.right, hexEnd }, scaler);
    m_fields->Layout ({ column.left, hexEnd + pad, column.right, column.bottom }, scaler);
    m_files->Layout  ({ column.left, y, column.right, column.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    painter.FillRect ((float) m_splitterPx.left, (float) m_splitterPx.top, (float) (m_splitterPx.right - m_splitterPx.left), (float) (m_splitterPx.bottom - m_splitterPx.top),
                      m_isSplitting ? theme.Accent() : theme.Divider());

    DxuiPanel::Paint (painter, text, theme);
    PaintHeader (painter, text, theme);

    if (m_isLongJob)
    {
        PaintProgress (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::PaintHeader
//
//  "Block 2" and its capsules after it, in the status band's chip style:
//  Free or Used, the role, and the files. Capsules that don't fit are left
//  out. Then the field line.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::PaintHeader (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float     kChipPadDip    = 8.0f;
    constexpr float     kChipHeightDip = 20.0f;
    constexpr uint32_t  kBadColor      = 0xFFE03C3C;



    float    font   = m_scaler.ToPxf (kFontDip);
    float    title  = m_scaler.ToPxf (kTitleDip);
    float    chipH  = m_scaler.ToPxf (kChipHeightDip);
    float    chipP  = m_scaler.ToPxf (kChipPadDip);
    float    gap    = m_scaler.ToPxf (kPadDip);
    float    top    = (float) m_titleRect.top;
    float    height = (float) (m_titleRect.bottom - m_titleRect.top);
    float    chipY  = top + (height - chipH) / 2.0f;
    float    x      = (float) m_titleRect.left;
    float    w      = 0.0f;
    float    h      = 0.0f;
    HRESULT  hr     = S_OK;



    if (m_model == nullptr || !m_model->IsOpen() || m_title.empty())
    {
        return;
    }

    hr = text.MeasureStringWeighted (m_title.c_str(), title, DxuiTheme::kBodyFace, DxuiFontWeight::SemiBold, w, h);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (m_title.c_str(), x, top, w + 1.0f, height, theme.Foreground(), title, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    x += w + gap;

    for (const Capsule & capsule : m_capsules)
    {
        text.MeasureString (capsule.text.c_str(), font, DxuiTheme::kBodyFace, w, h);
        w += 2 * chipP;

        if (x + w > m_titleRect.right)
        {
            break;
        }

        painter.FillRoundedRect    (x, chipY, w, chipH, chipH / 2, theme.ButtonIdle());
        painter.OutlineRoundedRect (x, chipY, w, chipH, chipH / 2, m_scaler.ToPxf (1.0f), capsule.isBad ? kBadColor : theme.ButtonBorder());

        hr = text.DrawString (capsule.text.c_str(), x, chipY, w, chipH, capsule.isBad ? kBadColor : theme.Foreground(), font, DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += w + gap / 2;
    }

    hr = text.DrawString (m_fieldLine.c_str(), (float) m_fieldRect.left, (float) m_fieldRect.top, (float) (m_fieldRect.right - m_fieldRect.left),
                          (float) (m_fieldRect.bottom - m_fieldRect.top), theme.Foreground(), font, DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::PaintProgress
//
//  A long job's bar and label in the row above the hex view. A job with no
//  count yet has an empty bar.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::PaintProgress (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  kBarHighDip = 8.0f;
    constexpr int    kPercent    = 100;



    float         left   = (float) m_rowRect.left;
    float         top    = (float) m_rowRect.top;
    float         height = (float) (m_rowRect.bottom - m_rowRect.top);
    float         barW   = m_scaler.ToPxf (kBarDip);
    float         barH   = m_scaler.ToPxf (kBarHighDip);
    float         barTop = top + (height - barH) / 2.0f;
    double        share  = m_jobTotal > 0 ? std::min (1.0, (double) m_jobDone / (double) m_jobTotal) : 0.0;
    std::wstring  label  = m_jobTotal > 0 ? std::format (L"{} ({}%)", m_jobLabel, (int) (share * kPercent)) : m_jobLabel;
    float         textX  = left + barW + m_scaler.ToPxf (kPadDip);
    float         textW  = (float) m_rowRect.right - textX - m_scaler.ToPxf (kButtonDip + kPadDip);
    HRESULT       hr     = S_OK;



    painter.FillRoundedRect    (left, barTop, barW, barH, barH / 2, theme.ButtonIdle());
    painter.FillRoundedRect    (left, barTop, (float) (barW * share), barH, barH / 2, theme.Accent());
    painter.OutlineRoundedRect (left, barTop, barW, barH, barH / 2, 1.0f, theme.ButtonBorder());

    hr = text.DrawString (label.c_str(), textX, top, std::max (0.0f, textW), height, theme.Foreground(), m_scaler.ToPxf (kFontDip), s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
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
    RECT  cancel = m_cancel->GetBounds();
    RECT  hex    = m_hex->GetBounds();
    RECT  map    = m_map->GetBounds();
    RECT  bounds = GetBounds();



    if (IsBusy())
    {
        return m_cancel->IsVisible() && PtInRect (&cancel, ev.positionDip) ? DxuiPanel::OnMouse (ev) : true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && PtInRect (&m_splitterPx, ev.positionDip))
    {
        m_isSplitting = true;
        return true;
    }

    if (m_isSplitting && ev.kind == DxuiMouseEventKind::Move && bounds.right > bounds.left)
    {
        m_split = std::clamp ((float) (ev.positionDip.x - bounds.left) / (float) (bounds.right - bounds.left), kMinSplit, kMaxSplit);
        Layout (bounds, m_scaler);

        if (m_onChanged)
        {
            m_onChanged();
        }

        return true;
    }

    if (m_isSplitting && ev.kind == DxuiMouseEventKind::Up)
    {
        m_isSplitting = false;
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && m_onPressed && m_hex->IsVisible() && PtInRect (&hex, ev.positionDip))
    {
        m_onPressed (m_hex);
    }

    //  The wheel over the grid zooms it, as the wheel over the platter does,
    //  whichever control has the keyboard.
    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.ctrl && PtInRect (&map, ev.positionDip))
    {
        return m_map->OnMouse (ev);
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
    return PtInRect (&m_splitterPx, clientPx) || m_isSplitting ? IDC_SIZEWE : DxuiPanel::GetCursorForPoint (clientPx);
}
