#pragma once

#include "Pch.h"

#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/ReverseBudget.h"

class HistoryOptionsPage;
class DisassemblyOptionsPage;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptions
//
//  What the debugger's Options sheet sets: reverse execution's settings and
//  the disassembly views' options.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerOptions
{
    ReverseOptions      reverse;
    DisassemblyOptions  disassembly;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheet
//
//  Tools > Options in the debugger: tabs, as Casso's Settings sheet has,
//  with a History page and a Disassembly page. OK returns both pages'
//  settings and the host applies them; Cancel and the close box change
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerOptionsSheet : public DxuiPropertySheet
{
public:
    //  Runs the sheet modally over `owner`; the new options when OK.
    static std::optional<DebuggerOptions>  Ask (HWND owner, const IDxuiTheme * theme, const DebuggerOptions & current);

    //  Builds the pages without a window, for tests.
    void  BuildForTest (const DebuggerOptions & current, const ReverseBudgetRange & range);

    HistoryOptionsPage      * GetHistoryPage     () const { return m_history;     }
    DisassemblyOptionsPage  * GetDisassemblyPage () const { return m_disassembly; }
    DebuggerOptions           GetOptions         () const;

protected:
    void     OnBuildPages () override;
    HRESULT  OnOk         () override;

private:
    static constexpr int  kSheetWidthDip  = 560;
    static constexpr int  kSheetHeightDip = 420;

    DebuggerOptions                 m_current;
    ReverseBudgetRange              m_range;
    std::optional<DebuggerOptions>  m_chosen;
    HistoryOptionsPage            * m_history     = nullptr;
    DisassemblyOptionsPage        * m_disassembly = nullptr;
};
