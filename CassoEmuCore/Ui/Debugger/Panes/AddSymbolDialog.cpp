#include "Pch.h"

#include "Ui/Debugger/Panes/AddSymbolDialog.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/TextDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AddSymbolDialog::Run
//
////////////////////////////////////////////////////////////////////////////////

void AddSymbolDialog::Run (IDebuggerPaneHost & host, Word address)
{
    std::optional<std::wstring>  typed;



    typed = host.RunTextDialog (L"Add symbol", L"Name", L"LOOP", [] (const std::wstring & text, std::wstring & error)
    {
        bool  isName = !text.empty() && iswalpha (text[0]) &&
                       std::ranges::all_of (text, [] (wchar_t ch) { return iswalnum (ch) || ch == L'_' || ch == L'.'; });



        error = isName ? std::wstring() : std::wstring (L"A symbol name starts with a letter and has letters, digits, _ and . after it.");
        return isName;
    });

    if (typed.has_value())
    {
        host.RunAction (DebuggerActions::GetAddSymbol (TextEncoding::WideToNarrow (*typed), address, host.GetMode()));
    }
}
