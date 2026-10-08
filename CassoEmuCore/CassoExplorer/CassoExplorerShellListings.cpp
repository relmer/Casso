#include "Pch.h"

#include "CassoExplorer/CassoExplorerShellListings.h"
#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShellListings::CassoExplorerShellListings
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerShellListings::CassoExplorerShellListings() :
    m_state (std::make_shared<State>())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShellListings::TryTake
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerShellListings::TryTake (const std::wstring & key, const std::wstring & id, Items & outItems, HRESULT & outResult)
{
    std::shared_ptr<State>        state   = m_state;
    IShellItemVerbs::ShellLister  lister  = m_lister;
    HWND                          hwnd    = m_hwnd;
    UINT                          message = m_message;



    outItems.clear();
    outResult = S_OK;

    if (!lister)
    {
        outResult = E_NOTIMPL;
        return true;
    }

    //  Nothing to tell when it arrives: read it now.
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
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"Shell folder reader");
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
//  CassoExplorerShellListings::TakeArrivedKeys
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerShellListings::TakeArrivedKeys()
{
    std::lock_guard<std::mutex>  guard (m_state->lock);
    std::vector<std::wstring>    keys = std::move (m_state->arrived);



    m_state->arrived.clear();

    return keys;
}
