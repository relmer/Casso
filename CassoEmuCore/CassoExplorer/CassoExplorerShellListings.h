#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"
#include "Core/ThreadName.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerListings
//
//  The items in a folder, read on a thread of its own, as Explorer reads
//  Network: the read can take as long as the network takes to answer, and the
//  window's thread is not blocked meanwhile. The first request for a listing
//  not yet read starts a read; when it arrives, a message is posted to the
//  window, which requests it again and takes it.
//
//  Each listing is taken once, by the key it was asked under, so the list and
//  the tree each read a folder afresh when they show it, as Explorer does.
//
//  Without a window to post to, or without a lister that can run on another
//  thread, the listing is read at once, on the caller's thread.
//
//  The thread shares its results only through state it has a reference to,
//  and calls a lister that belongs to no object, so it can outlive the
//  window: one waiting on the network when the window closes finishes
//  harmlessly, its message going nowhere.
//
////////////////////////////////////////////////////////////////////////////////

template <typename Item>
class CassoExplorerListings
{
public:
    using Items  = std::vector<Item>;
    using Lister = std::function<HRESULT (const std::wstring & id, Items & outItems)>;

    CassoExplorerListings () : m_state (std::make_shared<State>()) {}

    void  SetTarget (HWND hwnd, UINT message)  { m_hwnd = hwnd; m_message = message; }
    void  SetLister (Lister lister)            { m_lister = std::move (lister); }
    bool  HasLister () const                   { return (bool) m_lister; }

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

    std::shared_ptr<State>  m_state;
    Lister                  m_lister;
    HWND                    m_hwnd    = nullptr;
    UINT                    m_message = 0;
};

using CassoExplorerShellListings = CassoExplorerListings<IShellItemVerbs::ShellFolderItem>;
using CassoExplorerHostListings  = CassoExplorerListings<FileSystemEntry>;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerListings::TryTake
//
////////////////////////////////////////////////////////////////////////////////

template <typename Item>
bool CassoExplorerListings<Item>::TryTake (const std::wstring & key, const std::wstring & id, Items & outItems, HRESULT & outResult)
{
    std::shared_ptr<State>  state   = m_state;
    Lister                  lister  = m_lister;
    HWND                    hwnd    = m_hwnd;
    UINT                    message = m_message;



    outItems.clear();
    outResult = S_OK;

    if (!lister)
    {
        outResult = E_NOTIMPL;
        return true;
    }

    //  Nothing to post to when it arrives: read it now.
    if (hwnd == nullptr)
    {
        outResult = lister (id, outItems);
        return true;
    }

    {
        std::lock_guard<std::mutex>  guard (state->lock);
        auto                         found = state->done.find (key);

        if (found != state->done.end())
        {
            outItems  = std::move (found->second.items);
            outResult = found->second.hr;
            state->done.erase (found);
            return true;
        }

        if (state->pending.contains (key))
        {
            return false;
        }

        state->pending.insert (key);
    }

    std::thread ([state, lister, key, id, hwnd, message]()
    {
        HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"Folder reader");
        Result   result;



        IGNORE_RETURN_VALUE (hrName, S_OK);

        result.hr = lister (id, result.items);

        {
            std::lock_guard<std::mutex>  guard (state->lock);

            state->done[key] = std::move (result);
            state->pending.erase (key);
            state->arrived.push_back (key);
        }

        PostMessageW (hwnd, message, 0, 0);

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    }).detach();

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerListings::TakeArrivedKeys
//
////////////////////////////////////////////////////////////////////////////////

template <typename Item>
std::vector<std::wstring> CassoExplorerListings<Item>::TakeArrivedKeys()
{
    std::lock_guard<std::mutex>  guard (m_state->lock);
    std::vector<std::wstring>    keys = std::move (m_state->arrived);



    m_state->arrived.clear();

    return keys;
}
