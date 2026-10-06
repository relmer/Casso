#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/MemoryBarCommands.h"





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

MemoryPane::MemoryPane (int id, DxuiHexView * view, MoveFn move, ActionFn run, RunFn note) :
    m_id   (id),
    m_view (view),
    m_move (std::move (move)),
    m_note (std::move (note))
{
    m_model.SetOnPatch ([run = std::move (run)] (Word address, std::span<const Byte> bytes)
    {
        std::vector<Byte>  values (bytes.begin(), bytes.end());

        run ([address, values] (CommandMode mode) { return DebuggerActions::GetPatch (address, values, mode); });
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
    m_view->SetValueFormat  (m_format);
    m_view->SetShowValues   (m_showValues);
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
//  MemoryPane::SetValueFormat
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetValueFormat (DxuiHexView::ValueFormat format)
{
    m_format = format;
    m_view->SetValueFormat (format);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetShowValues
//
//  The text column alone holds more bytes a row, so the read may have to
//  follow, as it does for a new row width.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetShowValues (bool show)
{
    m_showValues = show;
    m_view->SetShowValues (show);
    FollowScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::FormatLayout
//
////////////////////////////////////////////////////////////////////////////////

std::string MemoryPane::FormatLayout() const
{
    const char  * format = (m_format == DxuiHexView::ValueFormat::Signed)   ? "signed"
                         : (m_format == DxuiHexView::ValueFormat::Unsigned) ? "unsigned"
                                                                            : "hex";
    bool          plain  = m_grouping == 1 && m_format == DxuiHexView::ValueFormat::Hex &&
                           m_columns == kDefaultColumns && m_showValues;



    if (plain)
    {
        return std::string();
    }

    return std::format ("memlayout{}={},{},{},{}", m_id, m_grouping, format, m_columns, m_showValues ? "values" : "text");
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::TryApplyLayout
//
//  All four parts or none: a token missing one, or with one out of range,
//  leaves the window as it is.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryPane::TryApplyLayout (const std::string & token)
{
    static constexpr int          kMaxColumns = 64;
    static constexpr size_t       kPartCount  = 4;
    static constexpr size_t       kFormatPart = 1;
    static constexpr size_t       kColumnPart = 2;
    static constexpr size_t       kShowPart   = 3;
    const std::vector<int>      & groupings   = MemoryBarCommands::GetGroupingChoices();
    std::string                   prefix      = std::format ("memlayout{}=", m_id);
    std::vector<std::string>      parts;
    std::istringstream            in;
    std::string                   part;
    int                           grouping    = 0;
    int                           columns     = -1;
    DxuiHexView::ValueFormat      format      = DxuiHexView::ValueFormat::Hex;
    std::from_chars_result        read;



    if (!token.starts_with (prefix))
    {
        return false;
    }

    in.str (token.substr (prefix.size()));

    while (std::getline (in, part, ','))
    {
        parts.push_back (part);
    }

    if (parts.size() != kPartCount || (parts[kShowPart] != "values" && parts[kShowPart] != "text"))
    {
        return false;
    }

    read = std::from_chars (parts[0].data(), parts[0].data() + parts[0].size(), grouping);

    if (read.ec != std::errc() || std::ranges::find (groupings, grouping) == groupings.end())
    {
        return false;
    }

    read = std::from_chars (parts[kColumnPart].data(), parts[kColumnPart].data() + parts[kColumnPart].size(), columns);

    if (read.ec != std::errc() || columns < 0 || columns > kMaxColumns)
    {
        return false;
    }

    if      (parts[kFormatPart] == "signed")   { format = DxuiHexView::ValueFormat::Signed;   }
    else if (parts[kFormatPart] == "unsigned") { format = DxuiHexView::ValueFormat::Unsigned; }
    else if (parts[kFormatPart] != "hex")      { return false;                                 }

    SetGrouping    (grouping);
    SetValueFormat (format);
    SetColumns     (columns);
    SetShowValues  (parts[kShowPart] == "values");

    return true;
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
