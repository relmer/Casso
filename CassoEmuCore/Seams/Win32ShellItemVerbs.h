#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs
//
//  IShellItemVerbs through the shell's COM interfaces, on the UI thread's
//  apartment.
//
//  THE SHELL MENU RUNS IN ITS OWN WINDOW. Send to, Open with and the
//  handlers other programs add fill their submenus only when opened, and
//  draw their own rows, so the window the menu belongs to has to pass those
//  messages back to the menu while it is up. A hidden window of this class's
//  own does that, so the caller's window procedure knows nothing about it.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellItemVerbs : public IShellItemVerbs
{
public:
    Win32ShellItemVerbs  () = default;
    ~Win32ShellItemVerbs () override;

    HRESULT  Open                (HWND owner, const std::wstring & path) override;
    HRESULT  GetOpenWithHandlers (const std::wstring & path, std::vector<Handler> & outHandlers) override;
    HRESULT  OpenWith            (HWND owner, const std::wstring & path, size_t handlerIndex) override;
    HRESULT  ChooseOtherApp      (HWND owner, const std::wstring & path) override;
    HRESULT  ShowShellMenu       (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx) override;
    HRESULT  Recycle             (HWND owner, const std::vector<std::wstring> & paths) override;
    HRESULT  RenameItem          (HWND owner, const std::wstring & path, const std::wstring & newName) override;
    HRESULT  PlaceOnClipboard    (HWND owner, const std::vector<std::wstring> & paths, bool cut) override;
    bool     ClipboardHasFiles   () const override;
    HRESULT  PasteInto           (HWND owner, const std::wstring & folder) override;
    HRESULT  CreateFolder        (HWND owner, const std::wstring & parent, const std::wstring & name) override;
    HRESULT  Share               (HWND owner, const std::vector<std::wstring> & paths) override;

private:
    static constexpr UINT            s_kFirstCommandId = 1;
    static constexpr UINT            s_kLastCommandId  = 0x7FFF;
    static constexpr const wchar_t * s_kClassName      = L"CassoExplorerShellMenuHost";

    //  The canonical verb of Explorer's Share command, which opens the same
    //  share sheet an app would through the data transfer manager.
    static constexpr const char    * s_kShareVerb      = "Windows.ModernShare";
    static constexpr const wchar_t * s_kShareVerbW     = L"Windows.ModernShare";

    static LRESULT CALLBACK  MenuHostProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT  EnumHandlers    (const std::wstring & path, std::vector<IAssocHandler *> & outHandlers);
    HRESULT  CreateMenuHost  (HWND owner);
    HRESULT  GetItemsMenu    (HWND owner, const std::vector<std::wstring> & paths, IContextMenu ** outMenu);
    HRESULT  GetItemArray    (const std::vector<std::wstring> & paths, IShellItemArray ** outItems);
    HRESULT  CreateOperation (HWND owner, IFileOperation ** outOperation);

    HWND            m_menuHost   = nullptr;
    IContextMenu2 * m_activeMenu = nullptr;
};
