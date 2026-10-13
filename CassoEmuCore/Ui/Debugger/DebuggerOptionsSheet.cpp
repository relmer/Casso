#include "Pch.h"

#include "Ui/Debugger/DebuggerOptionsSheet.h"
#include "Ui/Debugger/DisassemblyOptionsPage.h"
#include "Ui/Debugger/HistoryOptionsPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheet::OnBuildPages
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerOptionsSheet::OnBuildPages()
{
    m_history     = CreatePage<HistoryOptionsPage>     (L"History");
    m_disassembly = CreatePage<DisassemblyOptionsPage> (L"Disassembly");

    m_history->SetOptions     (m_current.reverse, m_range);
    m_disassembly->SetOptions (m_current.disassembly);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheet::BuildForTest
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerOptionsSheet::BuildForTest (const DebuggerOptions & current, const ReverseBudgetRange & range)
{
    m_current = current;
    m_range   = range;

    OnBuildPages();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheet::GetOptions
//
////////////////////////////////////////////////////////////////////////////////

DebuggerOptions DebuggerOptionsSheet::GetOptions() const
{
    DebuggerOptions  options = m_current;



    if (m_history != nullptr)
    {
        options.reverse = m_history->GetOptions();
    }

    if (m_disassembly != nullptr)
    {
        options.disassembly = m_disassembly->GetOptions();
    }

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheet::OnOk
//
//  Every page's settings are valid by construction, so OK only collects
//  them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerOptionsSheet::OnOk()
{
    m_chosen = GetOptions();

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOptionsSheet::Ask
//
//  The memory available is read as the sheet opens, which sets the History
//  slider's range.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerOptions> DebuggerOptionsSheet::Ask (HWND owner, const IDxuiTheme * theme, const DebuggerOptions & current)
{
    HRESULT                   hr     = S_OK;
    DebuggerOptionsSheet      sheet;
    DxuiWindow::CreateParams  params;



    sheet.m_current = current;
    sheet.m_range   = ReverseBudget::GetRange (ReverseBudget::GetAvailableBytes());

    sheet.SetApplyVisible   (false);
    sheet.SetDesignWidthDip (kSheetWidthDip);

    params.title                    = L"Options";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kSheetWidthDip, kSheetHeightDip };
    params.minSizeDip               = { kSheetWidthDip, kSheetHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = sheet.Create (params);
    CHR (hr);

    sheet.SetTheme (theme);
    sheet.ShowModalDialog (IDOK);

Error:
    return sheet.m_chosen;
}





