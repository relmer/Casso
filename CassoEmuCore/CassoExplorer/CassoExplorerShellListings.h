#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShellListings
//
//  What a shell folder holds, read on a thread of its own, as Explorer reads
//  Network: the shell can take as long as the network takes to answer, and
//  the window keeps working meanwhile. A listing asked for and not yet read
//  starts one; when it arrives, the window is told by a posted message and
//  asks again, taking it.
//
//  Each listing is taken once, by the key it was asked under, so the list and
//  the tree each read a folder afresh when they show it, as Explorer does.
//
//  Without a window to tell, or without a lister that can run on another
//  thread, the listing is read at once, on the caller's thread.
//
//  The thread shares its results only through state it holds a reference to,
//  and calls a lister that belongs to no object, so it can outlive the
//  window: one still waiting on the network when the window closes finishes
//  harmlessly, its message going nowhere.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerShellListings
{
public:
    using Items = std::vector<IShellItemVerbs::ShellFolderItem>;

    CassoExplorerShellListings ();

    void  SetTarget (HWND hwnd, UINT message)                  { m_hwnd = hwnd; m_message = message; }
    void  SetLister (IShellItemVerbs::ShellLister lister)      { m_lister = std::move (lister); }

    //  The listing under the key when it has arrived, taken; false while it
    //  is being read, starting it when it is not yet asked for. A failed read
    //  arrives as its error, with no items.
    bool  TryTake (const std::wstring & key, const std::wstring & id, Items & outItems, HRESULT & outResult);

    //  The keys whose listings arrived since the last call.
    std::vector<std::wstring>  TakeArrivedKeys ();

private:
    struct Result
    {
        HRESULT  hr = S_OK;
        Items    items;
    };

    struct State
    {
        std::mutex                     lock;
        std::map<std::wstring, Result> done;
        std::set<std::wstring>         pending;
        std::vector<std::wstring>      arrived;
    };

    std::shared_ptr<State>        m_state;
    IShellItemVerbs::ShellLister  m_lister;
    HWND                          m_hwnd    = nullptr;
    UINT                          m_message = 0;
};
