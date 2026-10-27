#pragma once

#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellVerbsTypes.h"
#include "Seams/IShellItemVerbs.h"
#include "Seams/Win32ShellPreview.h"

class Win32ShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin
//
//  Deleting and the Recycle Bin: an operation over shell items, recycling
//  and permanent deletion, the bin's items listed, restored, emptied and
//  watched, and their menus.
//
////////////////////////////////////////////////////////////////////////////////

class Win32RecycleBin
{
public:
    explicit Win32RecycleBin (Win32ShellItemVerbs & verbs);

    HRESULT  Recycle             (HWND owner, const std::vector<std::wstring> & paths);
    HRESULT  RestoreRecycled     (HWND owner, const std::vector<std::wstring> & paths);
    HRESULT  ListRecycled        (std::vector<IShellItemVerbs::RecycledItem> & outItems);
    HRESULT  RunRecycledVerb     (HWND owner, const std::vector<std::wstring> & ids, IShellItemVerbs::RecycledVerb verb);
    HRESULT  ShowRecycledMenu    (HWND owner, const std::vector<std::wstring> & ids, POINT screenPx);
    HRESULT  EmptyRecycleBin     (HWND owner);
    void     WatchRecycleBin     (HWND hwnd, UINT message);
    void     UnwatchRecycleBin   ();
    HRESULT  DeletePermanently (HWND owner, const std::vector<std::wstring> & ids);
    static constexpr const wchar_t * s_kCenteredProp   = L"CassoExplorerCentered";
    HRESULT  GetItemArray    (const std::vector<std::wstring> & paths, IShellItemArray ** outItems);
    HRESULT  CreateOperation (HWND owner, IFileOperation ** outOperation);
    static HRESULT  Perform  (IFileOperation * operation);
    HRESULT  GetRecycledMenu (HWND owner, const std::vector<std::wstring> & ids, IContextMenu ** outMenu);

    //  The Recycle Bin's columns: where an item was, and when it went.
    static constexpr PROPERTYKEY  s_kDeletedFrom  = { { 0x9B174B33, 0x40FF, 0x11D2, { 0xA2, 0x7E, 0x00, 0xC0, 0x4F, 0xC3, 0x08, 0x71 } }, 2 };
    static constexpr PROPERTYKEY  s_kDateDeleted  = { { 0x9B174B33, 0x40FF, 0x11D2, { 0xA2, 0x7E, 0x00, 0xC0, 0x4F, 0xC3, 0x08, 0x71 } }, 3 };

    static int64_t  ToUnixSeconds (const FILETIME & when);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;

    ULONG           m_binWatch   = 0;
    ULONG           m_binDeletes = 0;
};
