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
    m_modeButton->SetOnClick ([this]
    {
        std::string  mechanism = GetNextMechanism (m_mechanism);

        m_run ([mechanism] (CommandMode mode) { return DebuggerActions::GetCallStackMode (mechanism, mode); });
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::Apply
//
//  The columns fit the frames shown whenever they change, as the machine
//  pauses or steps.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackPane::Apply (const CallStackData & data)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;
    std::vector<Row>                              rows    = GetRows (data);
    bool                                          changed = !IsSameText (rows, m_rows);



    m_rows = std::move (rows);

    for (const Row & row : m_rows)
    {
        cells.push_back (GetCells (row, m_colors));
    }

    m_list->SetRows (std::move (cells));

    //  New frames size the columns to themselves again, narrower as well as
    //  wider, rather than keeping the widest text the pane has ever shown.
    if (changed)
    {
        m_list->ResetAutoFit();
    }

    if (data.mechanism != m_mechanism)
    {
        m_mechanism = data.mechanism;
        m_modeButton->SetLabel (GetModeLabel (m_mechanism));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetCells
//
//  The call site in the address color and the routine as an instruction is
//  colored: its kind, its target and its symbol. A dimmed row and a break
//  are left plain, so the dimming still reads as dimming.
//
//  A note is one muted cell spanning the row, which the columns are not
//  fitted to: a sentence that long would otherwise set the width of the
//  routine column and push the one after it off to the right.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> CallStackPane::GetCells (const Row & row, const DebuggerTextColors::Set & colors)
{
    std::vector<DxuiListView::Cell>  cells = { { row.site, row.isDim }, { row.routine, row.isDim }, { row.foundBy, row.isDim } };



    if (row.isNote)
    {
        cells.assign (1, DxuiListView::Cell { row.routine, true });
        cells[0].spansRow = true;
        return cells;
    }

    if (row.isDim || row.isBreak || colors.syntax.address == 0)
    {
        return cells;
    }

    cells[0].colorRanges.emplace_back (0, (int) row.site.size(), colors.operandAddress);
    cells[1].colorRanges = DebuggerTextColors::GetInstructionRanges (row.routine, colors);

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::IsSameText
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackPane::IsSameText (const std::vector<Row> & a, const std::vector<Row> & b)
{
    return std::equal (a.begin(), a.end(), b.begin(), b.end(), [] (const Row & x, const Row & y)
    {
        return x.site == y.site && x.routine == y.routine && x.foundBy == y.foundBy;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetRows
//
//  A break is a separator row: the instruction's address in the first column
//  and what broke the chain in the second. Where recording began is a note
//  across the row instead. An unverified frame is dimmed, as is the note on
//  the last return, which is about a frame no longer on the stack.
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
            //  The instruction's address goes in the call-site column, as a
            //  frame's does; the second column says what it did.
            row         = Row();
            row.site    = std::format (L"${:04X}", each.chainBreak->pc);
            row.routine = widen (CallStack::DescribeBreak (*each.chainBreak));
            row.isBreak = true;
            row.address = each.chainBreak->pc;

            if (each.chainBreak->kind == CallBreakKind::TrackingBegan)
            {
                row.site.clear();
                row.routine = GetUnrecordedNote (each.chainBreak->pc);
                row.isNote  = true;
            }

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
//  CallStackPane::GetUnrecordedNote
//
//  The pane's own sentence for where recording began, shorter than the
//  CALLS reply's and in sentence case, since it stands alone on its row.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CallStackPane::GetUnrecordedNote (Word pc)
{
    return std::format (L"Earlier calls weren't recorded (debugger opened at ${:04X})", pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetNextModeLine
//
////////////////////////////////////////////////////////////////////////////////

std::string CallStackPane::GetNextModeLine (CallStackMechanism current)
{
    return "CALLS MODE " + GetNextMechanism (current);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackPane::GetNextMechanism
//
//  Hybrid, recorded, walk, and round again.
//
////////////////////////////////////////////////////////////////////////////////

std::string CallStackPane::GetNextMechanism (CallStackMechanism current)
{
    switch (current)
    {
    case CallStackMechanism::Hybrid:   return "RECORDED";
    case CallStackMechanism::Recorded: return "WALK";
    default:                           return "HYBRID";
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
