#include "Pch.h"

#include "Ui/Debugger/Panes/CallStackPane.h"

#include "Debugger/CallStack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::CallStackPane
//
////////////////////////////////////////////////////////////////////////////////

CallStackPane::CallStackPane (DxuiListView * list, DxuiButton * modeButton, RunFn run, ShowFn showCode) :
    m_list       (list),
    m_modeButton (modeButton),
    m_run        (std::move (run)),
    m_showCode   (std::move (showCode))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CallStackPane::Configure()
{
    m_list->SetColumns ({ { L"Call site", 0, false, DxuiTextHAlign::Left },
                          { L"Routine",   0, false, DxuiTextHAlign::Left },
                          { L"Found by",  0, false, DxuiTextHAlign::Left } });

    m_list->SetActivateOnDoubleClick (true);

    m_list->SetOnActivateRow ([this] (int row)
    {
        if (row >= 0 && row < (int) m_rows.size())
        {
            m_showCode (m_rows[(size_t) row].address);
        }
    });

    m_modeButton->SetLabel   (GetModeLabel (m_mechanism));
    m_modeButton->SetOnClick ([this] { m_run (GetNextModeLine (m_mechanism)); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void CallStackPane::Apply (const CallStackData & data)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    m_rows = GetRows (data);

    for (const Row & row : m_rows)
    {
        cells.push_back ({ { row.site, row.isDim }, { row.routine, row.isDim }, { row.foundBy, row.isDim } });
    }

    m_list->SetRows (std::move (cells));

    if (data.mechanism != m_mechanism)
    {
        m_mechanism = data.mechanism;
        m_modeButton->SetLabel (GetModeLabel (m_mechanism));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetRows
//
//  A break is a separator row: dashes in the first column and what broke the
//  chain in the second. An unverified frame is dimmed, as is the note on the
//  last return, which is about a frame no longer on the stack.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CallStackPane::Row> CallStackPane::GetRows (const CallStackData & data)
{
    std::vector<Row>  rows;
    Row               row;
    auto              widen = [] (const std::string & text) { return std::wstring (text.begin(), text.end()); };
    auto              frameRow = [&widen] (const CallStackFrame & frame)
    {
        Row  made;

        made.site    = std::format (L"${:04X}", frame.callSite);
        made.routine = std::format (L"{} ${:04X}", widen (CallStack::GetKindName (frame.kind)), frame.target);
        made.foundBy = (frame.provenance == CallProvenance::Recorded) ? L"recorded as it ran" : L"found on the stack";
        made.isDim   = !frame.isVerified;
        made.address = frame.callSite;

        if (!frame.symbol.empty())
        {
            made.routine += L" " + widen (frame.symbol);
        }

        if (!frame.isVerified)
        {
            made.foundBy += L", unverified";
        }

        return made;
    };



    for (const CallStackRow & each : data.rows)
    {
        if (each.chainBreak.has_value())
        {
            row         = Row();
            row.site    = L"--";
            row.routine = widen (CallStack::DescribeBreak (*each.chainBreak));
            row.isBreak = true;
            row.address = each.chainBreak->pc;
            rows.push_back (row);
        }
        else if (each.frame.has_value())
        {
            rows.push_back (frameRow (*each.frame));
        }
    }

    if (data.lastReturn.has_value())
    {
        row         = frameRow (*data.lastReturn);
        row.foundBy = widen (data.lastReturn->note);
        row.isDim   = true;
        rows.push_back (row);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetNextModeLine
//
//  Hybrid, recorded, walk, and round again.
//
////////////////////////////////////////////////////////////////////////////////

std::string CallStackPane::GetNextModeLine (CallStackMechanism current)
{
    switch (current)
    {
    case CallStackMechanism::Hybrid:   return "CALLS MODE RECORDED";
    case CallStackMechanism::Recorded: return "CALLS MODE WALK";
    default:                           return "CALLS MODE HYBRID";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CallStackPane::GetModeLabel (CallStackMechanism mechanism)
{
    switch (mechanism)
    {
    case CallStackMechanism::Recorded: return L"Recorded";
    case CallStackMechanism::Walk:     return L"Stack walk";
    default:                           return L"Hybrid";
    }
}
