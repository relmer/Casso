#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryPane.h"





//  I/O and ROM bytes are colored, so a window shows at a glance which bytes an
//  edit cannot reach and which it patches rather than writes.
static constexpr uint32_t  s_kIoArgb  = 0xFF808080;
static constexpr uint32_t  s_kRomArgb = 0xFF7FB2E5;

//  Rows kept above the ones on screen when a new read is asked for, so a short
//  scroll back does not ask again.
static constexpr int       s_kLeadRows = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::MemoryPane
//
////////////////////////////////////////////////////////////////////////////////

MemoryPane::MemoryPane (int id, DxuiHexView * view, MoveFn move, RunFn run, RunFn note) :
    m_id   (id),
    m_view (view),
    m_move (std::move (move)),
    m_note (std::move (note))
{
    m_model.SetOnCommand (std::move (run));
    m_view->SetSource (&m_model);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::Configure
//
//  Sixteen bytes a row, Apple text in the text column, and editing on.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::Configure (HWND hwnd)
{
    m_view->SetOwnerWindow  (hwnd);
    m_view->SetBytesPerRow  (kBytesPerRow);
    (void) m_view->SetGrouping (m_grouping);
    m_view->SetShowValues   (true);
    m_view->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_view->SetEditable     (true);

    m_view->SetMarkColor ([this] (uint8_t mark, uint32_t & outArgb)
    {
        if (mark == MemoryEditModel::kMarkChanged)
        {
            outArgb = m_changedArgb;
        }
        else if (mark == MemoryEditModel::kMarkIo)
        {
            outArgb = s_kIoArgb;
        }
        else if (mark == MemoryEditModel::kMarkRom)
        {
            outArgb = s_kRomArgb;
        }

        return mark != MemoryEditModel::kMarkNone;
    });

    m_view->SetOnWriteRefused ([this] (uint64_t offset) { NoteRefusal (offset); });
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
        m_view->SetTopRow  ((uint64_t) window.first / kBytesPerRow);
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
    std::optional<Word>  start = GetReadStartFor (m_readFirst, GetTopAddress(), m_view->GetRowCap());



    if (start.has_value() && start != m_requested)
    {
        m_requested = start;
        m_move (m_id, *start);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GoTo
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::GoTo (Word address)
{
    Word  first = (Word) (address & ~(kBytesPerRow - 1));
    Word  phase = (Word) (address - first);



    //  The rows start at the address itself (FR-091): the view's offsets move
    //  by the address's distance from its row boundary, and its labels with
    //  them.
    m_model.SetPhase        (phase);
    m_view->SetOriginAddress (phase);
    m_view->SetTopRow        (0);
    m_view->SetTopRow        ((uint64_t) first / kBytesPerRow);
    m_view->GoToOffset       ((uint64_t) first);

    m_requested = first;
    m_move (m_id, first);
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
//  MemoryPane::GetReadStartFor
//
//  The new read starts a few rows above the ones on screen, on a row
//  boundary, and is kept inside the 64K so the last one ends at $FFFF.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> MemoryPane::GetReadStartFor (Word readFirst, uint64_t topOffset, int visibleRows)
{
    static constexpr uint64_t  kAddressSpace = 0x10000;
    static constexpr uint64_t  kReadBytes    = DebuggerViewState::kMemoryWindowBytes;
    uint64_t                   visibleEnd    = topOffset + (uint64_t) (std::max) (visibleRows, 1) * kBytesPerRow;
    uint64_t                   lead          = (uint64_t) s_kLeadRows * kBytesPerRow;
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
        m_note (std::format ("${:04X} is I/O, which an edit does not write; use OUT.", address));
    }
    else
    {
        m_note (std::format ("${:04X} cannot be edited until the window has read it.", address));
    }
}
