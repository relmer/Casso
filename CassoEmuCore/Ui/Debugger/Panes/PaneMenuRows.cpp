#include "Pch.h"

#include "Ui/Debugger/Panes/PaneMenuRows.h"
#include "Ui/Debugger/WatchActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PaneMenuRows::AddBookmark
//
////////////////////////////////////////////////////////////////////////////////

void PaneMenuRows::AddBookmark (const LatestSnapshot & latest, IDebuggerPaneHost & host, Word address, PaneMenuItems & items)
{
    auto  found = (latest.Get() == nullptr) ? std::vector<DebuggerViewSnapshot::WatchLine>::const_iterator()
                                          : std::ranges::find_if (latest.Get()->bookmarks, [address] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == address; });



    if (latest.Get() != nullptr && found != latest.Get()->bookmarks.end())
    {
        items.push_back ({ L"Remove bookmark", [&latest, &host, id = found->id] { host.RunAction (WatchActions::GetClearBookmark (id, latest.GetMode())); } });
    }
    else
    {
        items.push_back ({ L"Add bookmark", [&latest, &host, address] { host.RunAction (WatchActions::GetAddBookmark (address, latest.GetMode())); } });
    }
}
