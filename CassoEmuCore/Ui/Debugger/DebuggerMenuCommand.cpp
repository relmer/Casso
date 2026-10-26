#include "Pch.h"

#include "Ui/Debugger/DebuggerMenuCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerMenuCommand::Make
//
//  The check is fixed when the row is built, so a menu is built again before
//  it opens.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> DebuggerMenuCommand::Make (const std::wstring & label, bool checked, std::function<void()> chosen)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label     = label;
    command->isChecked = [checked] { return checked; };
    command->dispatch  = std::move (chosen);

    return command;
}
