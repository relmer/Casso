#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ViewMenuRows
//
//  The View menu's rows, from the pane list: a row for every pane with a
//  place on the menu, in the panes' order there, each checked while its pane
//  is open. The panes in several instances, and the device panels, fold
//  into a cascade each, placed where the first of them would have been.
//
////////////////////////////////////////////////////////////////////////////////

class ViewMenuRows
{
public:
    using PaneFn = std::function<void (const std::wstring & pane)>;
    using TestFn = std::function<bool (const std::wstring & pane)>;
    using TextFn = std::function<std::wstring (const std::wstring & pane)>;

    //  The panes with a row now, in the menu's order.
    static std::vector<std::wstring>  GetPanes (const std::vector<std::unique_ptr<IDebuggerPane>> & panes, const DebuggerViewSnapshot * snapshot);

    //  The rows: each opens its pane, and has its pane's key in the scheme
    //  in force. The rows' commands are added to `commands`, which must last
    //  as long as the menu.
    static std::vector<DxuiPopupMenuItem>  Build (const std::vector<std::unique_ptr<IDebuggerPane>> & panes,
                                                  const DebuggerViewSnapshot                         * snapshot,
                                                  const TextFn                                       & keyOf,
                                                  const PaneFn                                       & open,
                                                  const TestFn                                       & isOpen,
                                                  std::vector<std::shared_ptr<DxuiCommand>>          & commands);
};
