#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"

class Win32ShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus
//
//  The shell's menus and Open with: an item's handlers, the item and
//  background context menus opened from a hidden host window, running one
//  of their verbs, and opening an item.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellMenus
{
public:
    explicit Win32ShellMenus (Win32ShellItemVerbs & verbs);

    HRESULT  Open                (HWND owner, const std::wstring & path);
    HRESULT  GetOpenWithHandlers (const std::wstring & path, std::vector<IShellItemVerbs::Handler> & outHandlers);
    HRESULT  OpenWith            (HWND owner, const std::wstring & path, size_t handlerIndex);
    HRESULT  ChooseOtherApp      (HWND owner, const std::wstring & path);
    HRESULT  ShowShellMenu       (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx, const std::function<void()> & rename);
    HRESULT  ShowBackgroundMenu  (HWND owner, const std::wstring & folder, POINT screenPx);
    HRESULT  RunItemVerb         (HWND owner, const std::vector<std::wstring> & paths, const char * verb);
    static constexpr const wchar_t * s_kClassName      = L"CassoExplorerShellMenuHost";

    static LRESULT CALLBACK  MenuHostProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT  EnumHandlers    (const std::wstring & path, std::vector<IAssocHandler *> & outHandlers);
    HRESULT  CreateMenuHost  (HWND owner);
    HRESULT  GetItemsMenu    (HWND owner, const std::vector<std::wstring> & paths, IContextMenu ** outMenu);
    static void  RemoveExtraSeparators (HMENU menu);
    HRESULT  TrackMenu       (HWND owner, IContextMenu * menu, POINT screenPx, const std::function<void()> & rename);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;

    IContextMenu2 * m_activeMenu = nullptr;
};
