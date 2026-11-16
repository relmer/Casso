#include "Pch.h"

#include "Core/NumberFormat.h"
#include "Ui/DiskInspector/Blocks/BlockFilesLevel.h"
#include "Ui/DiskInspector/Blocks/BlockLevelHost.h"
#include "Ui/DiskInspector/Blocks/BlockLevels.h"
#include "Ui/DiskInspector/Blocks/BlockSectionsView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::BlockLevelHost
//
////////////////////////////////////////////////////////////////////////////////

BlockLevelHost::BlockLevelHost()  = default;
BlockLevelHost::~BlockLevelHost() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::Init
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::Init (BlockInspectorModel * model, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text)
{
    vector<DxuiListView::Column>  columns;



    m_model  = model;
    m_crumbs = CreateChild<DxuiAddressBar>();
    m_card   = CreateChild<DxuiListView>();
    m_browse = CreateChild<DxuiButton> (L"Browse the files");
    m_volume = CreateChild<BlockSectionsView>();
    m_files  = CreateChild<BlockFilesLevel>();
    m_file   = CreateChild<BlockSectionsView>();

    m_crumbs->SetTextRenderer (text);
    m_crumbs->SetOnSegment    ([this] (int index)
    {
        if (index < 0 || index >= (int) m_targets.size())
        {
            return;
        }

        if (m_targets[index].level == Level::Block && m_onBlock)
        {
            m_onBlock (m_targets[index].block);
            return;
        }

        GoTo (m_targets[index]);
    });

    for (const std::wstring & title : BlockLevels::GetCardColumns())
    {
        columns.push_back ({ title, columns.size() == 2 ? 180 : 100 });
    }

    m_card->SetTheme              (theme);
    m_card->SetShowHeader         (true);
    m_card->SetOwnerWindow        (hwnd);
    m_card->SetColumns            (std::move (columns));
    m_card->SetOnSelectionChanged ([this] (int row)
    {
        vector<uint32_t>  blocks;

        if (row < 0 || row >= (int) m_cardVolumes.size())
        {
            return;
        }

        blocks.resize (m_model->GetVolumeSize (m_cardVolumes[row]));
        std::iota (blocks.begin(), blocks.end(), m_model->GetVolumeFirst (m_cardVolumes[row]));
        m_model->HighlightBlocks (blocks);
        NotifyChanged();
    });
    m_card->SetOnActivateRow ([this] (int row)
    {
        if (row >= 0 && row < (int) m_cardVolumes.size())
        {
            GoTo ({ Level::Volume, m_cardVolumes[row] });
        }
    });

    m_browse->SetVariant (DxuiButton::Variant::Link);
    m_browse->SetOnClick ([this] () { GoTo ({ Level::Files, m_here.volume, -1 }); });

    m_volume->SetOnFollow ([this] (uint32_t block) { if (m_onBlock) { m_onBlock (block); } });
    m_file->SetOnFollow   ([this] (uint32_t block) { if (m_onBlock) { m_onBlock (block); } });

    m_files->Init        (theme, hwnd);
    m_files->SetOnSelect ([this] (int entry) { HighlightEntry (entry); NotifyChanged(); });
    m_files->SetOnOpen   ([this] (int entry) { GoTo ({ Level::File, m_here.volume, entry }); });
    m_files->SetOnDirectory ([this] (int entry) { GoTo ({ Level::Files, m_here.volume, entry }); });

    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::HasCard
//
//  Whether the image has more than one partition with a volume of blocks.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockLevelHost::HasCard() const
{
    size_t  count = 0;



    if (m_model == nullptr || !m_model->IsOpen())
    {
        return false;
    }

    for (const BlockBand & band : m_model->GetBands())
    {
        count += (!band.isMap && band.volume >= 0) ? 1 : 0;
    }

    return count > 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::Open
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::Open()
{
    Location  start;



    m_back.clear();
    m_forward.clear();
    m_filesVolume = -1;

    if (HasCard())
    {
        start.level = Level::Card;
    }
    else if (m_model->IsOpen() && m_model->GetVolumeCount() > 0 && !m_model->IsSectorVolume (0))
    {
        start.level  = Level::Volume;
        start.volume = 0;
    }
    else
    {
        start.level = Level::Block;
    }

    m_here = start;
    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::Sync
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::Sync()
{
    uint32_t  generation = m_model->GetMapGeneration();
    bool      isFirst    = m_generation == 0;



    if (generation == m_generation)
    {
        return;
    }

    m_generation  = generation;
    m_filesVolume = -1;

    if (isFirst)
    {
        Open();
    }
    else
    {
        Show();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::GoTo
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::GoTo (const Location & where)
{
    if (where == m_here)
    {
        return;
    }

    m_back.push_back (m_here);
    m_forward.clear();
    m_here = where;

    Show();
    NotifyChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::ShowBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::ShowBlock (uint32_t block)
{
    Location  where;



    where.level  = Level::Block;
    where.volume = m_model->FindVolume (block);
    where.block  = block;

    if (where == m_here)
    {
        return;
    }

    m_back.push_back (m_here);
    m_forward.clear();
    m_here = where;

    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::ReplaceBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::ReplaceBlock (uint32_t block)
{
    m_here.level  = Level::Block;
    m_here.volume = m_model->FindVolume (block);
    m_here.block  = block;

    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool BlockLevelHost::GoBack()
{
    if (m_back.empty())
    {
        return false;
    }

    m_forward.push_back (m_here);
    m_here = m_back.back();
    m_back.pop_back();

    Show();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool BlockLevelHost::GoForward()
{
    if (m_forward.empty())
    {
        return false;
    }

    m_back.push_back (m_here);
    m_here = m_forward.back();
    m_forward.pop_back();

    Show();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::Show
//
//  The current level's view made visible and filled, the others hidden, the
//  breadcrumb rebuilt, and the grid's highlight set for the level.
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::Show()
{
    Level                               level     = m_here.level;
    bool                                isOpen    = m_model != nullptr && m_model->IsOpen();
    bool                                hasVolume = isOpen && m_here.volume >= 0 && m_here.volume < m_model->GetVolumeCount();
    vector<BlockPartitionRow>           rows;
    vector<vector<DxuiListView::Cell>>  cells;



    m_card->SetVisible   (isOpen && level == Level::Card);
    m_browse->SetVisible (isOpen && level == Level::Volume);
    m_volume->SetVisible (isOpen && level == Level::Volume);
    m_files->SetVisible  (isOpen && level == Level::Files);
    m_file->SetVisible   (isOpen && level == Level::File);

    m_targets.clear();
    m_crumbs->SetSegments (isOpen ? BuildCrumbs (*m_model, m_here, GetCardLabel(), HasCard(), m_targets) : vector<std::wstring>());

    if (!isOpen || (!hasVolume && level != Level::Card && level != Level::Block))
    {
        return;
    }

    switch (level)
    {
        case Level::Card:
            m_cardVolumes.clear();

            for (BlockPartitionRow & row : BlockLevels::BuildCard (*m_model))
            {
                vector<DxuiListView::Cell>  line;

                for (std::wstring & text : row.cells)
                {
                    line.push_back ({ std::move (text) });
                }

                m_cardVolumes.push_back (row.volume);
                cells.push_back (std::move (line));
            }

            m_card->SetRows (std::move (cells));
            m_model->ClearHighlight();
            break;

        case Level::Volume:
            m_volume->SetSections (BlockLevels::BuildVolume (*m_model, m_here.volume));
            m_model->ClearHighlight();
            break;

        case Level::Files:
            if (m_filesVolume != m_here.volume)
            {
                m_files->SetVolume (&m_model->GetVolumeMap (m_here.volume), m_model->GetVolumeFirst (m_here.volume));
                m_filesVolume = m_here.volume;
            }

            m_files->ShowDirectory (m_here.entry);
            HighlightEntry (m_here.entry);
            break;

        case Level::File:
            if (m_here.entry < 0 || (size_t) m_here.entry >= m_model->GetVolumeMap (m_here.volume).entries.size())
            {
                m_file->SetSections ({});
                break;
            }

            m_file->SetSections (BlockLevels::BuildFile (*m_model, m_here.volume, m_here.entry));
            HighlightEntry (m_here.entry);
            break;

        case Level::Block:
            break;
    }

    if (m_scaler.GetDpi() != 0)
    {
        Layout (GetBounds(), m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::HighlightEntry
//
//  A directory's blocks with every file's under it; a file's blocks; nothing
//  for the volume directory.
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::HighlightEntry (int entry)
{
    const BlockMap  * map = m_here.volume >= 0 ? &m_model->GetVolumeMap (m_here.volume) : nullptr;



    if (map == nullptr || entry < 0 || (size_t) entry >= map->entries.size())
    {
        m_model->ClearHighlight();
        return;
    }

    m_model->HighlightFiles (m_here.volume, map->entries[entry].isDirectory ? BlockLevels::GetDescendants (*map, entry) : vector<int> { entry });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::NotifyChanged
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::NotifyChanged()
{
    if (m_onChanged)
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::GetCardLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockLevelHost::GetCardLabel() const
{
    return fs::path (m_model->GetFile().GetPath()).filename().wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::BuildCrumbs
//
//  The card, the volume, then for a file's location "Files" and each
//  directory down to the file, then the block. A block in a file has the
//  file's path before it.
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> BlockLevelHost::BuildCrumbs (const BlockInspectorModel & model, const Location & here, const std::wstring & cardLabel, bool hasCard,
                                                  vector<Location> & outTargets)
{
    vector<std::wstring>  segments;
    vector<int>           chain;
    int                   entry  = here.entry;
    int                   volume = here.volume;



    outTargets.clear();

    auto  add = [&segments, &outTargets] (const std::wstring & text, const Location & target)
    {
        segments.push_back (text);
        outTargets.push_back (target);
    };

    if (hasCard)
    {
        add (cardLabel, { Level::Card });
    }

    if (here.level == Level::Card || volume < 0 || volume >= model.GetVolumeCount())
    {
        if (here.level == Level::Block)
        {
            add (L"Block " + NumberFormat::FormatInteger (here.block), here);
        }

        return segments;
    }

    const BlockMap  & map = model.GetVolumeMap (volume);

    add (BlockLevels::GetVolumeName (model, volume), { Level::Volume, volume });

    if (here.level == Level::Block)
    {
        uint32_t  inner = here.block - model.GetVolumeFirst (volume);

        entry = inner < map.GetBlockCount() && !map.blocks[inner].owners.empty() ? map.blocks[inner].owners[0] : -1;
    }

    if (here.level == Level::Volume || (here.level == Level::Block && entry < 0))
    {
        if (here.level == Level::Block)
        {
            add (L"Block " + NumberFormat::FormatInteger (here.block), here);
        }

        return segments;
    }

    add (L"Files", { Level::Files, volume, -1 });

    for (int at = entry; at >= 0 && (size_t) at < map.entries.size() && chain.size() <= map.entries.size(); at = map.entries[at].parent)
    {
        chain.push_back (at);
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        add (map.entries[*it].name, { map.entries[*it].isDirectory ? Level::Files : Level::File, volume, *it });
    }

    if (here.level == Level::Block)
    {
        add (L"Block " + NumberFormat::FormatInteger (here.block), here);
    }

    return segments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::Layout
//
//  The breadcrumb across the top. At the Block level that is all, as the
//  panel lays out the block under it; otherwise the level's view fills the
//  rest.
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   crumb  = scaler.ToPx (kCrumbDip);
    int   gap    = scaler.ToPx (kGapDip);
    int   browse = scaler.ToPx (kBrowseDip);
    RECT  body   = { boundsPx.left, boundsPx.top + crumb + gap, boundsPx.right, boundsPx.bottom };



    SetBounds (boundsPx);
    m_scaler = scaler;

    m_crumbs->Layout ({ boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + crumb }, scaler);
    m_card->SetDpi   (scaler.GetDpi());
    m_card->Layout   (body, scaler);
    m_browse->Layout ({ body.left, body.top, body.right, body.top + browse }, scaler);
    m_volume->Layout ({ body.left, body.top + browse + gap, body.right, body.bottom }, scaler);
    m_files->Layout  (body, scaler);
    m_file->Layout   (body, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLevelHost::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockLevelHost::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (IsVisible())
    {
        DxuiPanel::Paint (painter, text, theme);
    }
}
