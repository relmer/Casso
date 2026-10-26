#include "Pch.h"

#include "Ui/Debugger/ViewMenuRows.h"

#include "Ui/Debugger/DebuggerMenuCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ViewMenuRows::GetPanes
//
//  Panes of equal rank are in the pane list's order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ViewMenuRows::GetPanes (const std::vector<std::unique_ptr<IDebuggerPane>> & panes, const DebuggerViewSnapshot * snapshot)
{
    std::vector<std::pair<int, std::wstring>>  ranked;
    std::vector<std::wstring>                  ids;



    for (const std::unique_ptr<IDebuggerPane> & pane : panes)
    {
        std::optional<int>  rank = pane->GetViewMenuRank (snapshot);

        if (rank.has_value())
        {
            ranked.push_back ({ *rank, pane->GetId() });
        }
    }

    std::ranges::stable_sort (ranked, {}, [] (const std::pair<int, std::wstring> & each) { return each.first; });

    for (const auto & [rank, id] : ranked)
    {
        ids.push_back (id);
    }

    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ViewMenuRows::Build
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ViewMenuRows::Build (const std::vector<std::unique_ptr<IDebuggerPane>> & panes,
                                                    const DebuggerViewSnapshot                         * snapshot,
                                                    const TextFn                                       & keyOf,
                                                    const PaneFn                                       & open,
                                                    const TestFn                                       & isOpen,
                                                    std::vector<std::shared_ptr<DxuiCommand>>          & commands)
{
    std::vector<DxuiPopupMenuItem>               view;
    std::vector<std::wstring>                    cascades;
    std::vector<std::vector<DxuiPopupMenuItem>>  cascadeRows;
    std::vector<size_t>                          cascadeAt;



    for (const std::wstring & id : GetPanes (panes, snapshot))
    {
        auto                           found = std::ranges::find_if (panes, [&id] (const std::unique_ptr<IDebuggerPane> & each) { return each->GetId() == id; });
        std::wstring                   group = (*found)->GetViewMenuGroup();
        std::shared_ptr<DxuiCommand>   row   = DebuggerMenuCommand::Make ((*found)->GetTitle(), false, [open, id] { open (id); });
        size_t                         index = 0;

        row->isChecked   = [isOpen, id] { return isOpen (id); };
        row->accelerator = keyOf (id);

        commands.push_back (row);

        if (group.empty())
        {
            view.push_back (DxuiPopupMenuItem::ForCommand (row));
            continue;
        }

        index = (size_t) (std::ranges::find (cascades, group) - cascades.begin());

        if (index == cascades.size())
        {
            cascades.push_back    (group);
            cascadeRows.push_back ({});
            cascadeAt.push_back   (view.size());
            view.push_back        (DxuiPopupMenuItem::ForSeparator());
        }

        cascadeRows[index].push_back (DxuiPopupMenuItem::ForCommand (row));
    }

    for (size_t i = 0; i < cascades.size(); i++)
    {
        std::shared_ptr<DxuiCommand>  cascade = DebuggerMenuCommand::Make (cascades[i], false, [] {});

        commands.push_back (cascade);
        view[cascadeAt[i]] = DxuiPopupMenuItem::ForSubmenu (cascade, std::move (cascadeRows[i]));
    }

    return view;
}
