#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ExplorerCommandEntries
//
//  The entries of File Explorer's command menus, and the helpers that build
//  them from IExplorerCommand handlers, for both the app commands and the
//  commands Windows adds.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ExplorerCommandEntries
{
public:
    //  The kind of item a registration applies to.
    enum class ItemKind { File, Folder, Background, Drive };

    struct Entry
    {
        std::wstring                                title;
        std::wstring                                icon;       // a "path,index" resource, or empty
        bool                                        enabled = true;
        bool                                        cloud   = false;     // a sync client's group, which has a menu group of its own
        Microsoft::WRL::ComPtr<IExplorerCommand>    command;
        std::string                                 verb;           // a static verb, invoked by name, when there is no command
        std::wstring                                explorerIcon;   // File Explorer's own icon for one of Windows' commands
        std::wstring                                package;        // the app's package family, or empty
        std::vector<Entry>                          children;
    };

    static HRESULT       MakeItems          (const std::vector<std::wstring> & paths, IShellItemArray ** outItems);
    static void          AddEntry           (IExplorerCommand * command, IShellItemArray * items, int depth, const std::wstring & fallbackTitle,
                                             const std::wstring & fallbackIcon, std::vector<Entry> & outEntries);
    static std::wstring  LoadIndirectString (const std::wstring & text);
};
