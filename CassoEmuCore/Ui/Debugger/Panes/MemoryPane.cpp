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

    //  A find's hits, the selected one in the current-match color.
    m_view->SetMatchFill ([this] (uint64_t offset) -> uint32_t
    {
        Word  address   = m_model.GetAddressOf (offset);
        bool  isCurrent = m_findCurrent >= 0 && (size_t) m_findCurrent < m_findHits.size() &&
                          (Word) (address - m_findHits[(size_t) m_findCurrent]) < m_findLength;



        //  Hits can overlap, as A0 A0 does in a run of A0s, so the selected
        //  hit is tested first: all of its bytes are in its color.
        if (isCurrent)
        {
            return m_currentArgb;
        }

        for (Word hit : m_findHits)
        {
            if ((Word) (address - hit) < m_findLength)
            {
                return m_matchArgb;
            }
        }

        return 0;
    });

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
//  MemoryPane::GetHeatTip
//
////////////////////////////////////////////////////////////////////////////////

HeatStrip::Tip MemoryPane::GetHeatTip (uint64_t offset) const
{
    if (!m_isHeatOn || m_heatLevels == nullptr)
    {
        return {};
    }

    return HeatStrip::GetTip (*m_heatLevels, m_model.GetAddressOf (offset), 1, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetHeatFill
//
//  Halfway from the page to the heat color. On a light page, the dark heat
//  colors are mixed in less, until the fill is within kMostContrast of the
//  page, so the dark byte text reads over it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t MemoryPane::GetHeatFill (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word address, const HeatMapView::Palette & palette)
{
    constexpr float  kTint         = 0.5f;
    constexpr float  kStep         = 0.05f;
    constexpr float  kLeastTint    = 0.1f;
    constexpr float  kMostContrast = 2.0f;
    constexpr float  kLightPage    = 0.5f;
    uint32_t         page          = palette.background | 0xFF000000u;
    uint32_t         heat          = 0;
    uint32_t         fill          = 0;
    float            tint          = kTint;
    Byte             execute       = 0;
    Byte             read          = 0;
    Byte             write         = 0;



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

    heat = HeatMapView::GetColor (HeatMapView::Mode::All, execute, read, write, palette);
    fill = DxuiColor::Mix (page, heat, tint) | 0xFF000000u;

    if (DxuiColor::ComputeRelativeLuminance (page) < kLightPage)
    {
        return fill;
    }

    while (tint > kLeastTint && DxuiColor::ComputeContrastRatio (fill, page) > kMostContrast)
    {
        tint -= kStep;
        fill  = DxuiColor::Mix (page, heat, tint) | 0xFF000000u;
    }

    return fill;
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
    m_changedArgb = colors.changed;
    m_romArgb     = colors.rom;
    m_ioArgb      = colors.io;
    m_unreadArgb  = colors.muted;
    m_differsArgb = colors.result;

    m_model.SetRegionColors (MemoryRegionKeys::GetColors (colors));
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
//  MemoryPane::SyncFromView
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SyncFromView()
{
    m_grouping = m_view->GetGrouping();
    m_columns  = m_view->GetColumns();
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
//  boundary, and stays inside the 64K so the last one ends at $FFFF. None
//  when that is the read the window has, as at the foot of memory, where the
//  rows can end past $FFFF, or for more rows than fit in one read.
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

    if (start == readFirst)
    {
        return std::nullopt;
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::FindHits
//
//  A run of one- or two-digit hex numbers, each with or without a dollar
//  sign, is bytes; anything else is text. Text is compared with each byte's
//  low seven bits, so normal and high-bit characters both match.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> MemoryPane::FindHits (const DebugMemoryImage & image, const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, size_t & outLength)
{
    static constexpr Byte  kLowSeven = 0x7F;
    std::vector<Word>      hits;
    std::vector<Byte>      bytes;
    std::wistringstream    words (needle);
    std::wstring           word;
    bool                   isBytes   = !isRegex && !needle.empty();
    std::wstring           text;



    outLength = 0;

    while (isBytes && words >> word)
    {
        std::wstring  digits = word.starts_with (L'$') ? word.substr (1) : word;

        isBytes = !digits.empty() && digits.size() <= 2 && std::ranges::all_of (digits, [] (wchar_t ch) { return iswxdigit (ch) != 0; });

        if (isBytes)
        {
            bytes.push_back ((Byte) std::stoul (digits, nullptr, 16));
        }
    }

    isBytes = isBytes && !bytes.empty();

    if (isBytes)
    {
        outLength = bytes.size();

        for (size_t at = 0; at + bytes.size() <= DebugMemoryImage::kBytes; at++)
        {
            if (std::equal (bytes.begin(), bytes.end(), image.bytes.begin() + (ptrdiff_t) at))
            {
                hits.push_back ((Word) at);
            }
        }

        return hits;
    }

    if (needle.empty())
    {
        return hits;
    }

    //  The text as the screen shows it, a character a byte; a control
    //  character matches nothing typed.
    text.resize (DebugMemoryImage::kBytes);

    for (size_t at = 0; at < DebugMemoryImage::kBytes; at++)
    {
        wchar_t  ch = (wchar_t) (image.bytes[at] & kLowSeven);

        text[at] = (ch < L' ') ? (wchar_t) 1 : ch;
    }

    if (isRegex)
    {
        try
        {
            std::wregex  pattern (needle, matchCase ? std::regex::ECMAScript : (std::regex::ECMAScript | std::regex::icase));

            for (auto match = std::wsregex_iterator (text.begin(), text.end(), pattern); match != std::wsregex_iterator(); ++match)
            {
                if (match->length() > 0)
                {
                    hits.push_back ((Word) match->position());
                    outLength = std::max (outLength, (size_t) match->length());
                }
            }
        }
        catch (const std::regex_error &)
        {
            hits.clear();
        }

        return hits;
    }

    outLength = needle.size();

    for (size_t at = 0; at + needle.size() <= text.size(); at++)
    {
        bool  isMatch = true;



        for (size_t each = 0; each < needle.size() && isMatch; each++)
        {
            wchar_t  have = text[at + each];
            wchar_t  want = needle[each];

            isMatch = matchCase ? have == want : towupper (have) == towupper (want);
        }

        if (isMatch && wholeWord)
        {
            isMatch = (at == 0 || !iswalnum (text[at - 1])) && (at + needle.size() == text.size() || !iswalnum (text[at + needle.size()]));
        }

        if (isMatch)
        {
            hits.push_back ((Word) at);
        }
    }

    return hits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetWordAt
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> MemoryPane::GetWordAt (const DebugMemoryImage & image, Word address)
{
    Word  high = (Word) (address + 1);



    if (!image.readable.test (address) || !image.readable.test (high))
    {
        return std::nullopt;
    }

    return (Word) (image.bytes[address] | (image.bytes[high] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetFindControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * MemoryPane::GetFindControl()
{
    return m_view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::GetFindBounds
//
////////////////////////////////////////////////////////////////////////////////

RECT MemoryPane::GetFindBounds() const
{
    return (m_view != nullptr) ? m_view->GetBounds() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::IsFindVisible
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryPane::IsFindVisible() const
{
    return m_view != nullptr && m_view->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SelectMatch
//
//  The hit after the caret, or the one before it, wrapping at the ends of
//  memory; it is brought into view with the caret on it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFindResult MemoryPane::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount)
{
    Word            caret  = m_model.GetAddressOf (m_view->GetCaret());
    int             chosen = -1;
    DxuiFindResult  result = DxuiFindResult::NotFound;



    m_findHits    = (m_image != nullptr) ? FindHits (*m_image, needle, matchCase, wholeWord, isRegex, m_findLength) : std::vector<Word>();
    m_findCurrent = -1;
    outIndex      = 0;
    outCount      = (int) m_findHits.size();

    if (m_findHits.empty())
    {
        SetFindMarks();
        return result;
    }

    for (int hit = 0; hit < outCount && forward && chosen < 0; hit++)
    {
        chosen = (m_findHits[(size_t) hit] > caret) ? hit : -1;
    }

    for (int hit = outCount - 1; hit >= 0 && !forward && chosen < 0; hit--)
    {
        chosen = (m_findHits[(size_t) hit] < caret) ? hit : -1;
    }

    result = (chosen < 0) ? DxuiFindResult::Wrapped : DxuiFindResult::Found;
    chosen = (chosen >= 0) ? chosen : (forward ? 0 : outCount - 1);

    m_findCurrent = chosen;
    outIndex      = chosen + 1;
    ShowFindHit (m_findHits[(size_t) chosen]);
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SelectMatchInView
//
//  As the needle is typed: the first hit at or after the window's first
//  byte, or the first in memory past the end.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFindResult MemoryPane::SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount)
{
    Word  top    = m_model.GetAddressOf (m_view->GetTopRow() * (uint64_t) m_view->GetBytesPerRow());
    int   chosen = 0;



    m_findHits    = (m_image != nullptr) ? FindHits (*m_image, needle, matchCase, wholeWord, isRegex, m_findLength) : std::vector<Word>();
    m_findCurrent = -1;
    outIndex      = 0;
    outCount      = (int) m_findHits.size();

    if (m_findHits.empty())
    {
        SetFindMarks();
        return DxuiFindResult::NotFound;
    }

    while (chosen < outCount && m_findHits[(size_t) chosen] < top)
    {
        chosen++;
    }

    chosen        = (chosen < outCount) ? chosen : 0;
    m_findCurrent = chosen;
    outIndex      = chosen + 1;
    ShowFindHit (m_findHits[(size_t) chosen]);
    return DxuiFindResult::Found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::ShowFindHit
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::ShowFindHit (Word address)
{
    uint64_t  offset  = (uint64_t) (Word) (address - m_model.GetAddressOf (0));
    RECT      byte    = {};
    int       covered = 0;
    int       rowH    = 0;
    uint64_t  rows    = 0;



    m_view->GoToOffset (offset);
    m_view->SelectByte (offset, DxuiHexViewTypes::Column::Hex);

    //  A hit under the find box is scrolled down below it.
    byte    = m_view->GetByteRect (offset, DxuiHexViewTypes::Column::Hex);
    covered = m_view->GetBounds().top + m_findCoveredPx;
    rowH    = byte.bottom - byte.top;

    if (m_findCoveredPx > 0 && rowH > 0 && byte.top < covered)
    {
        rows = (uint64_t) ((covered - byte.top + rowH - 1) / rowH);
        m_view->SetTopRow ((m_view->GetTopRow() > rows) ? m_view->GetTopRow() - rows : 0);
    }

    SetFindMarks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryPane::SetFindMarks
//
//  A mark down the scrollbar for each hit, where it is in the window's
//  bytes, the selected hit as the current one; none with no hits.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryPane::SetFindMarks()
{
    std::vector<DxuiScrollbar::Mark>  marks;
    uint64_t                          bytes = m_model.GetByteCount();



    if (m_view == nullptr)
    {
        return;
    }

    for (size_t hit = 0; hit < m_findHits.size() && bytes > 0; hit++)
    {
        uint64_t  offset = (uint64_t) (Word) (m_findHits[hit] - m_model.GetAddressOf (0));

        marks.push_back ({ ((float) offset + 0.5f) / (float) bytes, (int) hit == m_findCurrent });
    }

    m_view->SetScrollMarks (std::move (marks), m_matchArgb, m_currentArgb);
}
