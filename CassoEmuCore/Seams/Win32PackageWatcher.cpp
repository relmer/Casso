#include "Pch.h"

#include "Seams/Win32PackageWatcher.h"
#include "Core/ThreadName.h"


static constexpr const wchar_t *  s_kpszPackagesKey = L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackageWatcher::~Win32PackageWatcher
//
////////////////////////////////////////////////////////////////////////////////

Win32PackageWatcher::~Win32PackageWatcher()
{
    if (m_assocWatch != 0)
    {
        SHChangeNotifyDeregister (m_assocWatch);
    }

    if (m_stop != nullptr)
    {
        SetEvent (m_stop);
    }

    if (m_watcher.joinable())
    {
        m_watcher.join();
    }

    for (HANDLE event : { m_stop, m_reread })
    {
        if (event != nullptr)
        {
            CloseHandle (event);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackageWatcher::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackageWatcher::Start (HWND notify, UINT assocMessage, std::function<void()> onChanged)
{
    SHChangeNotifyEntry  anyItem = {};



    if (m_watcher.joinable())
    {
        return;
    }

    m_onChanged = std::move (onChanged);
    m_stop      = CreateEventW (nullptr, TRUE, FALSE, nullptr);
    m_reread    = CreateEventW (nullptr, FALSE, FALSE, nullptr);

    anyItem.fRecursive = TRUE;
    m_assocWatch       = SHChangeNotifyRegister (notify, SHCNRF_ShellLevel, SHCNE_ASSOCCHANGED, assocMessage, 1, &anyItem);

    m_watcher = std::thread ([this]()
    {
        HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"Package watcher");



        IGNORE_RETURN_VALUE (hrName, S_OK);

        Watch();

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackageWatcher::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackageWatcher::Refresh()
{
    if (m_reread != nullptr)
    {
        SetEvent (m_reread);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackageWatcher::Watch
//
//  Wait for a change to the key or a Refresh, then for the key to be quiet,
//  then call m_onChanged, until the destructor sets m_stop.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackageWatcher::Watch()
{
    HRESULT  hr       = S_OK;
    HKEY     packages = nullptr;
    HANDLE   changed  = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    LSTATUS  status   = ERROR_SUCCESS;
    DWORD    which    = WAIT_FAILED;
    bool     stopping = false;



    CBR (changed != nullptr && m_stop != nullptr && m_reread != nullptr);

    status = RegOpenKeyExW (HKEY_CLASSES_ROOT, s_kpszPackagesKey, 0, KEY_NOTIFY, &packages);
    hr     = HRESULT_FROM_WIN32 (status);
    CHR (hr);

    while (!stopping)
    {
        HANDLE  events[] = { m_stop, changed, m_reread };

        status = RegNotifyChangeKeyValue (packages, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, changed, TRUE);
        hr     = HRESULT_FROM_WIN32 (status);
        CHR (hr);

        which    = WaitForMultipleObjects ((DWORD) std::size (events), events, FALSE, INFINITE);
        stopping = which != WAIT_OBJECT_0 + 1 && which != WAIT_OBJECT_0 + 2;

        //  Wait out the rest of the change.
        while (!stopping)
        {
            status = RegNotifyChangeKeyValue (packages, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, changed, TRUE);
            hr     = HRESULT_FROM_WIN32 (status);
            CHR (hr);

            which    = WaitForMultipleObjects ((DWORD) std::size (events), events, FALSE, s_kQuietMs);
            stopping = which == WAIT_OBJECT_0 || which == WAIT_FAILED;

            if (which == WAIT_TIMEOUT)
            {
                break;
            }
        }

        if (!stopping)
        {
            m_onChanged();
        }
    }

Error:
    if (packages != nullptr)
    {
        RegCloseKey (packages);
    }

    if (changed != nullptr)
    {
        CloseHandle (changed);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}
