#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IShellItemVerbs
//
//  What Windows itself does with a real file or folder: open it in the
//  program it belongs to, offer the other programs that can open it, and
//  show the shell's own context menu.
//
//  A SEAM BECAUSE A TEST MAY NOT CALL THE SHELL. Which of these a menu offers
//  is decided by the caller; this only carries them out.
//
////////////////////////////////////////////////////////////////////////////////

class IShellItemVerbs
{
public:
    //  A program registered to open a file's type.
    struct Handler
    {
        std::wstring  name;
    };

    virtual ~IShellItemVerbs () = default;

    //  The file's default action, as a double-click in Explorer does.
    virtual HRESULT  Open (HWND owner, const std::wstring & path) = 0;

    //  The programs Windows recommends for the file's type, in its order,
    //  and opening the file in one of them by its index in that list.
    virtual HRESULT  GetOpenWithHandlers (const std::wstring & path, std::vector<Handler> & outHandlers) = 0;
    virtual HRESULT  OpenWith            (HWND owner, const std::wstring & path, size_t handlerIndex) = 0;

    //  Windows' own Open with dialog, for a program not on the list.
    virtual HRESULT  ChooseOtherApp (HWND owner, const std::wstring & path) = 0;

    //  The shell's full context menu for the items, which all share a
    //  folder, at a screen point. Whatever is picked is carried out there,
    //  including by other programs' menu handlers.
    virtual HRESULT  ShowShellMenu (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx) = 0;
};
