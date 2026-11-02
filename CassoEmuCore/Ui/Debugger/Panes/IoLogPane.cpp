#include "Pch.h"

#include "Ui/Debugger/Panes/IoLogPane.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerStatusText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::SetLaneShown
//
////////////////////////////////////////////////////////////////////////////////

void IoLogPane::SetLaneShown (Lane lane, bool isShown)
{
    if (lane < kLaneCount && m_lanes[lane] != isShown)
    {
        m_lanes[lane] = isShown;
        MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::GetOptionsText
//
////////////////////////////////////////////////////////////////////////////////

std::string IoLogPane::GetOptionsText() const
{
    std::string  lanes;



    for (bool isShown : m_lanes)
    {
        lanes += isShown ? '1' : '0';
    }

    return std::format ("lanes={};headings={};columnLayout={}", lanes, kHeadingsVersion, GetColumnsText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::SetOptionsText
//
////////////////////////////////////////////////////////////////////////////////

void IoLogPane::SetOptionsText (const std::string & text)
{
    size_t       start    = 0;
    size_t       end      = 0;
    size_t       equal    = 0;
    size_t       i        = 0;
    std::string  field;
    std::string  key;
    std::string  value;
    int          headings = 0;



    while (start < text.size())
    {
        end   = text.find (';', start);
        end   = (end == std::string::npos) ? text.size() : end;
        field = text.substr (start, end - start);
        equal = field.find ('=');
        key   = field.substr (0, equal);
        value = (equal == std::string::npos) ? std::string() : field.substr (equal + 1);

        if (key == "lanes")
        {
            for (i = 0; i < value.size() && i < kLaneCount; i++)
            {
                m_lanes[i] = value[i] == '1';
            }
        }
        else if (key == "headings")
        {
            headings = std::atoi (value.c_str());
        }
        else if (key == "columnLayout")
        {
            SetColumnsText (value);
        }

        start = end + 1;
    }

    //  The video log's Line and H columns became Scanline and Line cycle; a
    //  width saved for the old headings is too narrow for the new ones.
    if (headings < kHeadingsVersion)
    {
        GetList()->SetColumnOverrideWidthPx (kLine,       -1);
        GetList()->SetColumnOverrideWidthPx (kHorizontal, -1);
    }

    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::GetLaneLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * IoLogPane::GetLaneLabel (Lane lane)
{
    static constexpr const wchar_t * kLabels[kLaneCount] = { L"Speaker", L"Mockingboard 6522", L"Mockingboard AY-3-8910", L"Mockingboard SSI-263" };



    return (lane < kLaneCount) ? kLabels[lane] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::GetLane
//
////////////////////////////////////////////////////////////////////////////////

IoLogPane::Lane IoLogPane::GetLane (const IoEvent & event)
{
    switch (event.kind)
    {
    case IoEventKind::ViaWrite:    return kVia;
    case IoEventKind::AyWrite:     return kAy;
    case IoEventKind::SpeechWrite: return kSpeech;
    default:                       return kSpeaker;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::GetSince
//
////////////////////////////////////////////////////////////////////////////////

std::optional<uint64_t> IoLogPane::GetSince (size_t index) const
{
    return (index < m_since.size()) ? m_since[index] : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::GetColumnSpecs
//
//  The beam's columns for the video log, the gap between writes for the
//  sound log; the rest in both.
//
////////////////////////////////////////////////////////////////////////////////

IoLogPane::ColumnSpecs IoLogPane::GetColumnSpecs() const
{
    return { { L"Time",       DxuiTextHAlign::Left,  kFit,  true,       L"The host's clock at the entry" },
             { L"Cycle",      DxuiTextHAlign::Right, 110,   true,       L"The CPU's cycle count since the last power cycle" },
             { L"Frame",      DxuiTextHAlign::Right, 110,   false,      L"The cycle the video frame began at" },
             { L"Scanline",   DxuiTextHAlign::Right,  70,   m_isVideo,  L"The scanline the beam was on, 0 at the top of the frame" },
             { L"Line cycle", DxuiTextHAlign::Right,  90,   m_isVideo,  L"The CPU cycle within the scanline, 0 to 64: the count Understanding the Apple II calls the horizontal counter" },
             { L"Since",      DxuiTextHAlign::Right,  80,   !m_isVideo, L"The cycles since the speaker's last toggle, or since the last write to a board" },
             { L"Address",    DxuiTextHAlign::Left,   70,   true,       L"The address the CPU read or wrote" },
             { L"Source",     DxuiTextHAlign::Left,  110,   !m_isVideo, L"The speaker, a Mockingboard's chips, or the SSI-263" },
             { L"Event",      DxuiTextHAlign::Left,    0,   true,       L"What the access did" } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::PrepareView
//
//  The cycles since the last entry of the same, as SOUNDLOG has them: the
//  speaker's since the speaker's last toggle, a board's since the last
//  write to any board. A gap, or a stretch that cannot be rebuilt, starts
//  both again. Rows before `from` are as they were, so the walk starts from
//  the last of each before it.
//
////////////////////////////////////////////////////////////////////////////////

void IoLogPane::PrepareView (
    const std::vector<Row>  & rows,
    size_t                    from)
{
    std::optional<uint64_t>  lastSpeaker;
    std::optional<uint64_t>  lastBoard;
    size_t                   i        = 0;
    bool                     isFound  = false;



    from = std::min (from, rows.size());
    m_since.resize (rows.size());

    //  Back from `from` for the last of each, to a gap at most.
    for (i = from; i > 0 && !isFound; i--)
    {
        const Row  & row = rows[i - 1];

        if (row.kind == DebugLogRowKind::Gap || row.kind == DebugLogRowKind::Unavailable)
        {
            break;
        }

        if (row.kind != DebugLogRowKind::Event || row.event.kind == IoEventKind::VideoMode)
        {
            continue;
        }

        if (row.event.kind == IoEventKind::Speaker && !lastSpeaker.has_value())
        {
            lastSpeaker = row.event.cycle;
        }
        else if (row.event.kind != IoEventKind::Speaker && !lastBoard.has_value())
        {
            lastBoard = row.event.cycle;
        }

        isFound = lastSpeaker.has_value() && lastBoard.has_value();
    }

    for (i = from; i < rows.size(); i++)
    {
        const Row                & row     = rows[i];
        bool                       isSpeak = row.event.kind == IoEventKind::Speaker;
        std::optional<uint64_t>  & last    = isSpeak ? lastSpeaker : lastBoard;

        m_since[i].reset();

        if (row.kind == DebugLogRowKind::Gap || row.kind == DebugLogRowKind::Unavailable)
        {
            lastSpeaker.reset();
            lastBoard.reset();
            continue;
        }

        if (row.kind != DebugLogRowKind::Event || row.event.kind == IoEventKind::VideoMode)
        {
            continue;
        }

        if (last.has_value() && row.event.cycle >= *last)
        {
            m_since[i] = row.event.cycle - *last;
        }

        last = row.event.cycle;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::Matches
//
//  The video log has no filter; the sound log's is by source.
//
////////////////////////////////////////////////////////////////////////////////

bool IoLogPane::Matches (
    const Row  & row,
    size_t       index) const
{
    (void) index;

    return m_isVideo || m_lanes[GetLane (row.event)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::MakeCells
//
//  The base made the rows' own time and cycle cells; the row's index in the
//  rows, for its gap since the last, is where it lies in them.
//
////////////////////////////////////////////////////////////////////////////////

void IoLogPane::MakeCells (
    const Row  & row,
    Cells      & out) const
{
    const IoEvent                  & event  = row.event;
    const std::vector<Row>         & rows   = GetRows().GetRows();
    std::optional<uint64_t>          since;
    size_t                           index  = 0;



    if (!rows.empty() && &row >= rows.data() && &row < rows.data() + rows.size())
    {
        index = (size_t) (&row - rows.data());
        since = GetSince (index);
    }

    if (event.kind == IoEventKind::VideoMode)
    {
        out[kFrame].text      = DebuggerStatusText::FormatCount (event.cycle - event.cycleInFrame);
        out[kLine].text       = std::format (L"{}", event.scanline);
        out[kHorizontal].text = std::format (L"{}", event.cycleInLine);
        out[kSource].text     = L"Display";
    }
    else if (event.kind == IoEventKind::SpeechWrite)
    {
        out[kSource].text = L"SSI-263";
    }
    else
    {
        out[kSource].text = (event.kind == IoEventKind::Speaker) ? L"Speaker" : std::format (L"Mockingboard {}", event.chip + 1);
    }

    if (event.kind != IoEventKind::AyWrite)
    {
        out[kAddress].text = std::format (L"${:04X}", event.address);
    }

    if (since.has_value())
    {
        out[kSince].text = L"+" + DebuggerStatusText::FormatCount (*since);
    }

    out[kEvent].text = TextEncoding::NarrowToWide (IoEventLog::FormatEvent (event));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogPane::GetSortNumber
//
//  The frame, the beam's line and cycle, and the gap since the last of the
//  same sort by number.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<uint64_t> IoLogPane::GetSortNumber (const Row & row, size_t index, size_t column) const
{
    bool  isVideo = row.kind == DebugLogRowKind::Event && row.event.kind == IoEventKind::VideoMode;



    switch (column)
    {
    case kFrame:      return isVideo ? std::optional<uint64_t> (row.event.cycle - row.event.cycleInFrame) : std::nullopt;
    case kLine:       return isVideo ? std::optional<uint64_t> (row.event.scanline)                       : std::nullopt;
    case kHorizontal: return isVideo ? std::optional<uint64_t> (row.event.cycleInLine)                    : std::nullopt;
    case kSince:      return (row.kind == DebugLogRowKind::Event) ? GetSince (index) : std::nullopt;
    default:          return std::nullopt;
    }
}
