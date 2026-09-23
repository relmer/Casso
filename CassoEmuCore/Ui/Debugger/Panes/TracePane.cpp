#include "Pch.h"

#include "Ui/Debugger/Panes/TracePane.h"

#include "Debugger/AppleWinFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TracePane::Configure
//
//  The list follows the newest entry while it sits at the end, as the
//  console does.
//
////////////////////////////////////////////////////////////////////////////////

void TracePane::Configure()
{
    m_list->SetColumns ({ { L"Entry",       0, false, DxuiTextHAlign::Right },
                          { L"Cycles",      0, false, DxuiTextHAlign::Right },
                          { L"PC",          0, false, DxuiTextHAlign::Left  },
                          { L"Bytes",       0, false, DxuiTextHAlign::Left  },
                          { L"Label",       0, false, DxuiTextHAlign::Left  },
                          { L"Instruction", 0, false, DxuiTextHAlign::Left  },
                          { L"Registers",   0, false, DxuiTextHAlign::Left  },
                          { L"Access",      0, false, DxuiTextHAlign::Left  } });

    m_list->SetRowProvider   (0, [this] (int row, std::vector<DxuiListView::Cell> & out) { ProvideRow (row, out); });
    m_list->EnableStickyTail (true);
    m_list->SetPreciseAutoFit (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void TracePane::Apply (const DebuggerViewSnapshot::TraceState & trace)
{
    static constexpr uint64_t  kMaxRows = INT_MAX;
    bool                       resized  = trace.total != m_total;



    m_first   = trace.first;
    m_total   = std::min (trace.total, kMaxRows);
    m_entries = trace.entries;

    if (resized)
    {
        m_list->SetVirtualRowCount ((int) m_total);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePane::FollowScroll
//
//  A read already asked for is not asked for again while it is on its way.
//
////////////////////////////////////////////////////////////////////////////////

void TracePane::FollowScroll()
{
    std::optional<uint64_t>  start;



    if (!m_list->IsVisible())
    {
        return;
    }

    start = GetReadStartFor (m_first, m_entries.size(), (uint64_t) m_list->GetTopRow(),
                             m_list->GetVisibleRowCapacity(), m_list->IsAtBottom());

    if (start.has_value() && start != m_requested)
    {
        m_requested = start;
        m_move ((*start == kFollowEnd) ? std::nullopt : start);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePane::GetReadStartFor
//
////////////////////////////////////////////////////////////////////////////////

std::optional<uint64_t> TracePane::GetReadStartFor (uint64_t first, uint64_t held, uint64_t top, int visible, bool isAtEnd)
{
    uint64_t  shown = (visible > 0) ? (uint64_t) visible : 0;



    if (isAtEnd)
    {
        return kFollowEnd;
    }

    if (top >= first && top + shown <= first + held)
    {
        return std::nullopt;
    }

    return (top > kLeadRows) ? top - kLeadRows : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TracePane::ProvideRow
//
//  A row outside the window read last is blank until its read arrives.
//
////////////////////////////////////////////////////////////////////////////////

void TracePane::ProvideRow (int row, std::vector<DxuiListView::Cell> & out) const
{
    static constexpr size_t  kColumns = 8;
    uint64_t                 index    = (uint64_t) row;
    const TraceRecord      * record   = nullptr;
    std::string              access;
    std::string              bytes;



    out.assign (kColumns, DxuiListView::Cell());

    if (index < m_first || index - m_first >= m_entries.size())
    {
        return;
    }

    record = &m_entries[(size_t) (index - m_first)];

    if (record->hasAccess)
    {
        access = std::format ("{} {:04X}={:02X} {}", record->accessIsWrite ? 'W' : 'R',
                              record->accessAddress, record->accessData, record->accessSymbol);
    }

    bytes = AppleWinFormatter::FormatTraceBytes (*record);

    out[0].text = std::format (L"{}", record->index);
    out[1].text = std::format (L"{}", record->cycles);
    out[2].text = std::format (L"{:04X}", record->pc);
    out[3].text = std::wstring (bytes.begin(), bytes.end());
    out[4].text = std::wstring (record->symbol.begin(), record->symbol.end());
    out[5].text = std::wstring (record->instruction.begin(), record->instruction.end());
    out[6].text = std::format (L"A={:02X} X={:02X} Y={:02X} SP={:02X} ", record->a, record->x, record->y, record->sp);
    out[7].text = std::wstring (access.begin(), access.end());

    for (char ch : AppleWinFormatter::FormatFlags (record->p))
    {
        out[6].text += (wchar_t) ch;
    }
}
