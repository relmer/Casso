#include "Pch.h"

#include "Seams/Win32ShellMenuHandlers.h"
#include "Core/TextEncoding.h"
#include "Core/ThreadName.h"


//  The shell's handler for Start's Pin and Unpin on a folder or a program.
static constexpr CLSID  s_kStartPinHandler = { 0x470C0EBD, 0x5D73, 0x4D58, { 0x9C, 0xED, 0xE9, 0x1E, 0x22, 0xE2, 0x32, 0x82 } };





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::~Win32ShellMenuHandlers
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellMenuHandlers::~Win32ShellMenuHandlers()
{
    if (m_newReader.joinable())
    {
        m_newReader.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellMenuHandlers::Start (HWND notify, UINT message)
{
    if (m_newReader.joinable())
    {
        return;
    }

    m_notify        = notify;
    m_notifyMessage = message;

    //  The shell's New menu handler runs in a single-threaded apartment.
    m_newReader = std::thread ([this]()
    {
        HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"New menu");



        IGNORE_RETURN_VALUE (hrName, S_OK);

        ReadNewChoices();

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::ReadNewChoices
//
//  The choices do not depend on the folder, so they are read against the
//  temporary folder, and nothing is created there.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellMenuHandlers::ReadNewChoices()
{
    HRESULT                                 hr                      = S_OK;
    wchar_t                                 temporary[MAX_PATH + 1] = {};
    DWORD                                   length                  = GetTempPathW ((DWORD) std::size (temporary), temporary);
    bool                                    located                 = length != 0 && length < (DWORD) std::size (temporary);
    Microsoft::WRL::ComPtr<IContextMenu3>   menu;
    HMENU                                   popup                   = nullptr;
    HMENU                                   choices                 = nullptr;
    std::vector<NewChoice>                  found;
    int                                     count                   = 0;



    CBR (located);

    hr = OpenNewMenu (temporary, &menu, popup, choices);
    CHR (hr);

    count = GetMenuItemCount (choices);

    for (int i = 0; i < count; i++)
    {
        wchar_t        text[256] = {};
        MENUITEMINFOW  info      = { sizeof (info) };
        NewChoice      choice;

        info.fMask      = MIIM_STRING | MIIM_ID | MIIM_FTYPE;
        info.dwTypeData = text;
        info.cch        = (UINT) std::size (text) - 1;

        if (!GetMenuItemInfoW (choices, (UINT) i, TRUE, &info) || (info.fType & MFT_SEPARATOR) != 0)
        {
            continue;
        }

        choice.title = text;
        choice.verb  = GetMenuVerb (menu.Get(), info.wID);

        if (!choice.title.empty() && !choice.verb.empty())
        {
            found.push_back (std::move (choice));
        }
    }

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_newChoices = std::move (found);
    }

    if (m_notify != nullptr)
    {
        PostMessageW (m_notify, m_notifyMessage, 0, 0);
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::GetStartPinEntry
//
//  The handler is created alone, rather than with the item's whole classic
//  menu, which takes far longer. Its one row is titled Pin or Unpin by
//  whether the item is pinned; choosing it runs the row's verb through the
//  item's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellMenuHandlers::GetStartPinEntry (const std::wstring & path, Win32ExplorerCommandEntries::Entry & outEntry)
{
    HRESULT                                 hr      = S_OK;
    Microsoft::WRL::ComPtr<IShellItem>      item;
    Microsoft::WRL::ComPtr<IDataObject>     data;
    Microsoft::WRL::ComPtr<IShellExtInit>   init;
    Microsoft::WRL::ComPtr<IContextMenu>    menu;
    HMENU                                   popup   = CreatePopupMenu();
    int                                     count   = 0;
    bool                                    found   = false;
    std::wstring                            title;
    std::wstring                            verb;



    CWR (popup != nullptr);

    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    hr = CoCreateInstance (s_kStartPinHandler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&init));
    CHR (hr);

    hr = init->Initialize (nullptr, data.Get(), nullptr);
    CHR (hr);

    hr = init.As (&menu);
    CHR (hr);

    hr = menu->QueryContextMenu (popup, 0, kFirstNewCommandId, kLastNewCommandId, CMF_NORMAL);
    CHR (hr);

    count = GetMenuItemCount (popup);

    for (int i = 0; i < count && !found; i++)
    {
        wchar_t        text[256] = {};
        MENUITEMINFOW  info      = { sizeof (info) };

        info.fMask      = MIIM_STRING | MIIM_ID | MIIM_FTYPE;
        info.dwTypeData = text;
        info.cch        = (UINT) std::size (text) - 1;

        if (GetMenuItemInfoW (popup, (UINT) i, TRUE, &info) && (info.fType & MFT_SEPARATOR) == 0)
        {
            title = text;
            verb  = GetMenuVerb (menu.Get(), info.wID);
            found = !title.empty() && !verb.empty();
        }
    }

    if (found)
    {
        std::wstring  plain = title;

        std::erase (plain, L'&');

        outEntry              = Win32ExplorerCommandEntries::Entry();
        outEntry.title        = title;
        outEntry.verb         = TextEncoding::WideToNarrow (verb);
        outEntry.explorerIcon = (_wcsnicmp (plain.c_str(), L"Unpin", 5) == 0) ? L"windows.unpinfromstartscreen" : L"PinToStartScreen";
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::GetNewChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32ShellMenuHandlers::NewChoice> Win32ShellMenuHandlers::GetNewChoices()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_newChoices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::CreateNew
//
//  The new item's name comes from the handler, "New Text Document.txt" or the
//  like, so the item is found as the one entry the folder did not have
//  before. Shortcut's wizard runs in a process of its own and renames its
//  item when it finishes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenuHandlers::CreateNew (HWND owner, const std::wstring & folder, const std::wstring & verb, std::wstring & outPath)
{
    HRESULT                                 hr       = S_OK;
    Microsoft::WRL::ComPtr<IContextMenu3>   menu;
    HMENU                                   popup    = nullptr;
    HMENU                                   choices  = nullptr;
    UINT                                    chosen   = 0;
    int                                     count    = 0;
    CMINVOKECOMMANDINFOEX                   invoke   = { sizeof (invoke) };
    std::unordered_set<std::wstring>        before;
    std::error_code                         error;



    outPath.clear();

    hr = OpenNewMenu (folder, &menu, popup, choices);
    CHR (hr);

    count = GetMenuItemCount (choices);

    for (int i = 0; i < count && chosen == 0; i++)
    {
        UINT  id = GetMenuItemID (choices, i);

        if (id != (UINT) -1 && _wcsicmp (GetMenuVerb (menu.Get(), id).c_str(), verb.c_str()) == 0)
        {
            chosen = id;
        }
    }

    CBREx (chosen != 0, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    for (const fs::directory_entry & entry : fs::directory_iterator (folder, error))
    {
        before.insert (entry.path().filename().wstring());
    }

    invoke.fMask        = CMIC_MASK_UNICODE;
    invoke.hwnd         = owner;
    invoke.lpVerb       = MAKEINTRESOURCEA (chosen - kFirstNewCommandId);
    invoke.lpVerbW      = MAKEINTRESOURCEW (chosen - kFirstNewCommandId);
    invoke.lpDirectoryW = folder.c_str();
    invoke.nShow        = SW_SHOWNORMAL;

    hr = menu->InvokeCommand ((LPCMINVOKECOMMANDINFO) &invoke);
    CHR (hr);

    for (const fs::directory_entry & entry : fs::directory_iterator (folder, error))
    {
        if (outPath.empty() && !before.contains (entry.path().filename().wstring()))
        {
            outPath = entry.path().wstring();
        }
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::OpenNewMenu
//
//  The handler adds one row, New, and fills its submenu only as the submenu
//  opens, so the opening is sent to it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenuHandlers::OpenNewMenu (const std::wstring & folder, IContextMenu3 ** outMenu, HMENU & outPopup, HMENU & outChoices)
{
    HRESULT                                 hr     = S_OK;
    PIDLIST_ABSOLUTE                        pidl   = nullptr;
    Microsoft::WRL::ComPtr<IShellExtInit>   init;
    Microsoft::WRL::ComPtr<IContextMenu3>   menu;
    MENUITEMINFOW                           info   = { sizeof (info) };
    LRESULT                                 result = 0;
    bool                                    found  = false;



    *outMenu   = nullptr;
    outPopup   = nullptr;
    outChoices = nullptr;

    hr = SHParseDisplayName (folder.c_str(), nullptr, &pidl, 0, nullptr);
    CHR (hr);

    hr = CoCreateInstance (CLSID_NewMenu, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&init));
    CHR (hr);

    hr = init->Initialize (pidl, nullptr, nullptr);
    CHR (hr);

    hr = init.As (&menu);
    CHR (hr);

    outPopup = CreatePopupMenu();
    CWR (outPopup != nullptr);

    hr = menu->QueryContextMenu (outPopup, 0, kFirstNewCommandId, kLastNewCommandId, CMF_NORMAL);
    CHR (hr);

    info.fMask = MIIM_SUBMENU;
    found      = GetMenuItemInfoW (outPopup, 0, TRUE, &info) && info.hSubMenu != nullptr;

    CBREx (found, E_UNEXPECTED);

    hr = menu->HandleMenuMsg2 (WM_INITMENUPOPUP, (WPARAM) info.hSubMenu, 0, &result);
    CHR (hr);

    outChoices = info.hSubMenu;
    *outMenu   = menu.Detach();

Error:
    CoTaskMemFree (pidl);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers::GetMenuVerb
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellMenuHandlers::GetMenuVerb (IContextMenu * menu, UINT id)
{
    wchar_t  verb[256] = {};
    HRESULT  hr        = menu->GetCommandString (id - kFirstNewCommandId, GCS_VERBW, nullptr, (LPSTR) verb, (UINT) std::size (verb) - 1);



    return SUCCEEDED (hr) ? std::wstring (verb) : std::wstring();
}





