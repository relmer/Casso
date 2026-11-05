#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerMenuCommand
//
//  A menu row's command for the debugger's menus and drop-downs.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerMenuCommand
{
public:
    //  A row with a label, a check as it is now, and what choosing it does.
    static std::shared_ptr<DxuiCommand>  Make (const std::wstring & label, bool checked, std::function<void()> chosen);

    //  A row that can't be checked; a menu of these has no check gutter.
    static std::shared_ptr<DxuiCommand>  Make (const std::wstring & label, std::function<void()> chosen);
};
