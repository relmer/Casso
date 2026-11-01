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
//  The latest items read under a key remain until the next, so a folder read
//  again keeps its rows on screen meanwhile. A slow read can publish what it
//  has so far, which arrives as a listing does: Network has its devices in
//  seconds, and its read doesn't end for up to half a minute.
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

    //  What a read calls with its items so far, and how the reader's thread is
    //  given that for the read that follows.
    using Progress       = std::function<void (const Items & items)>;
    using ProgressSetter = std::function<void (Progress progress)>;

    CassoExplorerListings () : m_state (std::make_shared<State>()) {}

    void  SetTarget         (HWND hwnd, UINT message)  { m_hwnd = hwnd; m_message = message; }
    void  SetLister         (Lister lister)            { m_lister = std::move (lister); }
    bool  HasLister         () const                   { return (bool) m_lister; }
    void  SetProgressSetter (ProgressSetter setter) { m_progressSetter = std::move (setter); }

    //  The listing under the key when it has arrived, taken; false while it
    //  is being read, starting it when it is not yet asked for. A failed read
    //  arrives as its error, with no items.
    bool  TryTake (const std::wstring & key, const std::wstring & id, Items & outItems, HRESULT & outResult);

    //  Whether a read under the key has finished, untaken, or is under way. A
    //  progress message that outlives its read has neither, and acting on it
    //  would start the read again.
    bool  IsReadOrReading (const std::wstring & key);

    //  The latest items read under the key, while a read is under way.
    bool  TryGetLatest (const std::wstring & key, Items & outItems);

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
        std::map<std::wstring, Items>  latest;
        std::set<std::wstring>         pending;
        std::vector<std::wstring>      arrived;
    };

    std::shared_ptr<State>  m_state;
    Lister                  m_lister;
    ProgressSetter          m_progressSetter;
    HWND                    m_hwnd    = nullptr;
    UINT                    m_message = 0;

    //  A folder larger than this isn't kept twice to show during its next read.
    static constexpr size_t  s_kMaxLatestItems = 2000;
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
    ProgressSetter          setter  = m_progressSetter;



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

    std::thread ([state, lister, setter, key, id, hwnd, message]()
    {
        HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"Folder reader");
        Result   result;



        IGNORE_RETURN_VALUE (hrName, S_OK);

        if (setter)
        {
            setter ([state, key, hwnd, message] (const Items & items)
            {
                {
                    std::lock_guard<std::mutex>  guard (state->lock);

                    state->latest[key] = items;
                    state->arrived.push_back (key);
                }

                PostMessageW (hwnd, message, 0, 0);
            });
        }

        result.hr = lister (id, result.items);

        if (setter)
        {
            setter (nullptr);
        }

        {
            std::lock_guard<std::mutex>  guard (state->lock);

            if (SUCCEEDED (result.hr) && result.items.size() <= s_kMaxLatestItems)
            {
                state->latest[key] = result.items;
            }
            else
            {
                state->latest.erase (key);
            }

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
//  CassoExplorerListings::IsReadOrReading
//
////////////////////////////////////////////////////////////////////////////////

template <typename Item>
bool CassoExplorerListings<Item>::IsReadOrReading (const std::wstring & key)
{
    std::lock_guard<std::mutex>  guard (m_state->lock);



    return m_state->done.contains (key) || m_state->pending.contains (key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerListings::TryGetLatest
//
////////////////////////////////////////////////////////////////////////////////

template <typename Item>
bool CassoExplorerListings<Item>::TryGetLatest (const std::wstring & key, Items & outItems)
{
    std::lock_guard<std::mutex>  guard (m_state->lock);
    auto                         found = m_state->latest.find (key);



    if (found == m_state->latest.end())
    {
        return false;
    }

    outItems = found->second;

    return true;
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
