#include "Pch.h"

#include "Core/ThreadName.h"
#include "Seams/Win32InfoTips.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips::~Win32InfoTips
//
////////////////////////////////////////////////////////////////////////////////

Win32InfoTips::~Win32InfoTips()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_stopping = true;
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32InfoTips::Start (HWND hwnd, UINT message)
{
    m_hwnd    = hwnd;
    m_message = message;

    if (!m_thread.joinable())
    {
        m_thread = std::thread ([this] { HRESULT hrName = ThreadName::SetForCurrentThread (L"Casso Explorer info tips"); IGNORE_RETURN_VALUE (hrName, S_OK); Run(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool Win32InfoTips::TryGet (const std::wstring & path, std::wstring & outTip)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    auto                         found = m_tips.find (path);



    if (found != m_tips.end())
    {
        outTip = found->second;
        return true;
    }

    if (m_wanted != path)
    {
        m_wanted = path;
        m_wake.notify_one();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips::Clear
//
////////////////////////////////////////////////////////////////////////////////

void Win32InfoTips::Clear()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    m_tips.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips::Run
//
////////////////////////////////////////////////////////////////////////////////

void Win32InfoTips::Run()
{
    HRESULT  hrCom = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);



    for (;;)
    {
        std::wstring  path;
        std::wstring  tip;

        {
            std::unique_lock<std::mutex>  guard (m_lock);

            m_wake.wait (guard, [this] { return m_stopping || (!m_wanted.empty() && m_tips.find (m_wanted) == m_tips.end()); });

            if (m_stopping)
            {
                break;
            }

            path = m_wanted;
        }

        tip = Read (path);

        {
            std::lock_guard<std::mutex>  guard (m_lock);

            m_tips[path] = tip;
        }

        if (m_hwnd != nullptr)
        {
            PostMessageW (m_hwnd, m_message, 0, 0);
        }
    }

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InfoTips::Read
//
//  The tip the item's folder gives it, as Explorer asks for it. Empty when
//  the shell has none.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32InfoTips::Read (const std::wstring & path)
{
    HRESULT               hr     = S_OK;
    PIDLIST_ABSOLUTE      pidl   = nullptr;
    PCUITEMID_CHILD       child  = nullptr;
    ComPtr<IShellFolder>  parent;
    ComPtr<IQueryInfo>    info;
    PWSTR                 text   = nullptr;
    std::wstring          tip;



    hr = SHParseDisplayName (path.c_str(), nullptr, &pidl, 0, nullptr);
    CHR (hr);

    hr = SHBindToParent (pidl, IID_PPV_ARGS (&parent), &child);
    CHR (hr);

    hr = parent->GetUIObjectOf (nullptr, 1, &child, IID_IQueryInfo, nullptr, (void **) info.GetAddressOf());
    CHR (hr);

    hr = info->GetInfoTip (QITIPF_DEFAULT, &text);
    CHR (hr);

    tip = (text != nullptr) ? text : L"";

Error:
    CoTaskMemFree (text);
    CoTaskMemFree (pidl);

    return tip;
}
