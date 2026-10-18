#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryPane.h"





//  Rows kept above the ones on screen when a new read is asked for, so a short
//  scroll back does not ask again.
static constexpr int       s_kLeadRows = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::MemoryPane
//
////////////////////////////////////////////////////////////////////////////////

MemoryPane::MemoryPane (int id, DxuiHexView * view, MoveFn move, ActionFn run, RunFn note) :
    m_id   (id),
    m_view (view),
    m_move (std::move (move)),
    m_note (std::move (note))
{
    m_model.SetOnPatch ([run = std::move (run)] (Word address, std::span<const Byte> bytes, std::span<const Byte> required)
    {
        std::vector<Byte>  values (bytes.begin(), bytes.end());
        std::vector<Byte>  before (required.begin(), required.end());

        run ([address, values, before] (CommandMode mode) { return DebuggerActions::GetPatch (address, values, mode, before); });
    });
    m_view->SetSource (&m_model);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::Configure
//
//  Sixteen values a row, Apple text in the text column, and editing on.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::Configure (HWND hwnd)
{
    m_view->SetOwnerWindow  (hwnd);
    (void) m_view->SetGrouping (m_grouping);
    m_view->SetColumns      (m_columns);
    m_view->SetShowValues   (true);
    m_view->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_view->SetEditable     (true);
    m_view->SetShowRegions  (true);

    //  The CPU's 64K wraps, so rows started partway along it keep four digits.
    //  The addresses are labels, in the proportional face; the bytes are a
    //  grid, in the fixed-width one.
    m_view->SetAddressSpace        (m_model.GetByteCount());
    m_view->SetProportionalOffsets (true);

    m_view->SetMarkColor ([this] (uint8_t mark, uint32_t & outArgb)
    {
        if (mark == MemoryEditModel::kMarkChanged)
        {
            outArgb = m_changedArgb;
        }
        else if (mark == MemoryEditModel::kMarkIo)
        {
            outArgb = m_ioArgb;
        }
        else if (mark == MemoryEditModel::kMarkRom)
        {
            outArgb = m_romArgb;
        }
        else if (mark == MemoryEditModel::kMarkUnread)
        {
            outArgb = m_unreadArgb;
        }
        else if (mark == MemoryEditModel::kMarkDiffers)
        {
            outArgb = m_differsArgb;
        }

        return mark != MemoryEditModel::kMarkNone;
    });

    m_view->SetOnWriteRefused ([this] (uint64_t offset) { NoteRefusal (offset); });

    m_view->SetByteFill ([this] (uint64_t offset) -> uint32_t
    {
        if (!m_isHeatOn || m_heatLevels == nullptr)
        {
            return 0;
        }

        return GetHeatFill (*m_heatLevels, m_model.GetAddressOf (offset), m_heatPalette);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetHeat
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetHeat (std::shared_ptr<const DebuggerViewSnapshot::HeatColumnLevels> levels, const HeatMapView::Palette & palette)
{
    m_heatLevels  = std::move (levels);
    m_heatPalette = palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetHeatFill
//
//  Halfway, so the byte's own ink stays readable over it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t MemoryPane::GetHeatFill (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word address, const HeatMapView::Palette & palette)
{
    constexpr float  kTint   = 0.5f;
    Byte             execute = 0;
    Byte             read    = 0;
    Byte             write   = 0;



    if (levels.execute.size() != 0x10000 || levels.read.size() != 0x10000 || levels.write.size() != 0x10000)
    {
        return 0;
    }

    execute = levels.execute[address];
    read    = levels.read[address];
    write   = levels.write[address];

    if (execute == 0 && read == 0 && write == 0)
    {
        return 0;
    }

    return DxuiColor::Mix (palette.background | 0xFF000000u, HeatMapView::GetColor (HeatMapView::Mode::All, execute, read, write, palette), kTint) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetTextColors
//
//  ROM, slot ROM and I/O are outlined in their bytes' colors, the language
//  card's banks and aux RAM in the memory map's.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetTextColors (const DebuggerTextColors::Set & colors)
{
    MemoryEditModel::RegionColors  regions;



    m_changedArgb = colors.changed;
    m_romArgb     = colors.rom;
    m_ioArgb      = colors.io;
    m_unreadArgb  = colors.muted;
    m_differsArgb = colors.result;

    regions.rom     = colors.rom;
    regions.io      = colors.io;
    regions.lcBank1 = colors.mapLcBank1;
    regions.lcBank2 = colors.mapLcBank2;
    regions.aux     = colors.mapAux;

    m_model.SetRegionColors (regions);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::Apply (const DebuggerViewSnapshot::MemoryWindow & window)
{
    m_model.SetContents (window.first, window.bytes, window.regions);
    m_readFirst = window.first;

    if (m_requested == window.first)
    {
        m_requested.reset();
    }

    if (!m_placed)
    {
        m_view->SetTopRow  ((uint64_t) window.first / (uint64_t) m_view->GetBytesPerRow());
        m_view->GoToOffset (window.first);
        m_placed = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::FollowScroll
//
//  A read already asked for is not asked for again while it is on its way.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::FollowScroll()
{
    std::optional<Word>  start = GetReadStartFor (m_readFirst, GetTopAddress(), m_view->GetRowCap(), m_view->GetBytesPerRow());



    if (start.has_value() && start != m_requested)
    {
        m_requested = start;
        m_move (m_id, *start);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetComparison
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetComparison (const std::vector<CompareDifference> & differences)
{
    std::map<Word, Byte>  others;



    for (const CompareDifference & each : differences)
    {
        others[each.address] = each.otherValue;
    }

    m_model.SetComparison (std::move (others), false);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SelectRange
//
//  The view's offsets start at the model's phase, so an address's offset is
//  its distance from it.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SelectRange (Word address, size_t count)
{
    uint64_t  offset = 0;



    GoTo (address);
    offset = (uint64_t) (Word) (address - m_model.GetPhase());

    m_view->SelectByte (offset, DxuiHexView::Column::Hex);

    if (count > 1)
    {
        m_view->ExtendSelectionTo (offset + count - 1);
    }

    m_view->EnsureByteVisible (offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GoTo
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::GoTo (Word address)
{
    Word  row   = (Word) m_view->GetBytesPerRow();
    Word  phase = (Word) (address % row);
    Word  first = (Word) (address - phase);



    //  The rows start at the address itself (FR-091): the view's offsets move
    //  by the address's distance from its row boundary, and its labels with
    //  them.
    m_model.SetPhase        (phase);
    m_view->SetOriginAddress (phase);
    m_view->SetTopRow        (0);
    m_view->SetTopRow        ((uint64_t) first / row);
    m_view->GoToOffset       ((uint64_t) first);

    //  Reads start on a sixteen-byte boundary, which a narrow row need not.
    m_requested = (Word) (first & ~(kBytesPerRow - 1));
    m_move (m_id, *m_requested);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::CycleGrouping
//
////////////////////////////////////////////////////////////////////////////////

int MemoryPane::CycleGrouping()
{
    m_grouping = (m_grouping >= 4) ? 1 : m_grouping * 2;
    (void) m_view->SetGrouping (m_grouping);

    return m_grouping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetGrouping
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetGrouping (int bytesPerValue)
{
    if (m_view->SetGrouping (bytesPerValue))
    {
        m_grouping = bytesPerValue;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetColumns
//
//  A new row width moves the rows on screen, so the read may have to follow.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetColumns (int valuesPerRow)
{
    m_columns = valuesPerRow;
    m_view->SetColumns (valuesPerRow);
    FollowScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::Refresh()
{
    m_requested = m_readFirst;
    m_move (m_id, m_readFirst);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetReadStartFor
//
//  The new read starts a few rows above the ones on screen, on a sixteen-byte
//  boundary, and is kept inside the 64K so the last one ends at $FFFF.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> MemoryPane::GetReadStartFor (Word readFirst, uint64_t topOffset, int visibleRows, int bytesPerRow)
{
    static constexpr uint64_t  kAddressSpace = 0x10000;
    static constexpr uint64_t  kReadBytes    = DebuggerViewState::kMemoryWindowBytes;
    uint64_t                   visibleEnd    = topOffset + (uint64_t) (std::max) (visibleRows, 1) * (uint64_t) bytesPerRow;
    uint64_t                   lead          = (uint64_t) s_kLeadRows * (uint64_t) bytesPerRow;
    uint64_t                   start         = 0;



    if (topOffset >= readFirst && visibleEnd <= (uint64_t) readFirst + kReadBytes)
    {
        return std::nullopt;
    }

    start = (topOffset > lead) ? (topOffset - lead) : 0;
    start = (std::min) (start, kAddressSpace - kReadBytes);
    start = start & ~(uint64_t) (kBytesPerRow - 1);

    return (Word) start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::UndoOrRedo
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryPane::UndoOrRedo (bool redo)
{
    std::optional<Word>  changed;
    bool                 done = false;



    if (!m_view->IsEditable())
    {
        return false;
    }

    done = redo ? m_model.Redo (changed) : m_model.Undo (changed);

    if (changed.has_value() && m_note)
    {
        m_note (std::format ("${:04X} has changed since the edit, so it was not {}.", *changed, redo ? "redone" : "undone"));
    }

    return done;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::NoteRefusal
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::NoteRefusal (uint64_t offset) const
{
    Word                         address = m_model.GetAddressOf (offset);
    std::optional<MemoryRegion>  region  = m_model.TryGetRegion (address);



    if (!m_note)
    {
        return;
    }

    if (region == MemoryRegion::Io)
    {
        m_note (std::format ("${:04X} is I/O. Use OUT to write it.", address));
    }
    else
    {
        m_note (std::format ("${:04X} cannot be edited until the window has read it.", address));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetAddressAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> MemoryPane::GetAddressAt (POINT clientDip) const
{
    DxuiHexView::HitResult  hit = m_view->HitTestPoint (clientDip);



    if (!hit.hit)
    {
        return std::nullopt;
    }

    return m_model.GetAddressOf (hit.offset);
}





