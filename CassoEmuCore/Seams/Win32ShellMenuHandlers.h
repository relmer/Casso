#pragma once

#include "Pch.h"

#include "Seams/Win32ExplorerCommandEntries.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenuHandlers
//
//  The shell's context menu handlers that Casso Explorer creates alone: the
//  New menu, whose choices are read once on a separate thread, and Start's
//  Pin and Unpin.
//
//  The shell's handler for the New menu takes a few hundred milliseconds to
//  list its choices. Choosing one creates a fresh copy of the handler for the
//  folder and runs the choice there, as File Explorer runs it.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellMenuHandlers
{
public:
    //  One of the shell's New choices. The verb is the shell's for it: a
    //  file type's extension, such as ".txt", or NewFolder or NewLink.
    struct NewChoice
    {
        std::wstring  title;
        std::wstring  verb;
    };

    static constexpr const wchar_t *  kNewFolderVerb     = L"NewFolder";
    static constexpr const wchar_t *  kNewLinkVerb       = L"NewLink";
    static constexpr UINT             kFirstNewCommandId = 1;

    ~Win32ShellMenuHandlers();

    //  Starts reading New's choices, and posts `message` to `notify` once
    //  they are read.
    void  Start (HWND notify, UINT message);

    //  Returns Start's Pin or Unpin command for one item, with the title from
    //  the shell's handler; false when the handler has neither.
    bool  GetStartPinEntry (const std::wstring & path, Win32ExplorerCommandEntries::Entry & outEntry);

    //  Returns New's choices, in the shell's order; empty until read.
    std::vector<NewChoice>  GetNewChoices ();

    //  Runs a New choice in the folder and returns the path of the item it
    //  created, or an empty path when none can be found.
    HRESULT  CreateNew (HWND owner, const std::wstring & folder, const std::wstring & verb, std::wstring & outPath);

    //  Returns the verb of a menu row whose id was given from kFirstNewCommandId.
    static std::wstring  GetMenuVerb (IContextMenu * menu, UINT id);

private:
    static constexpr UINT  kLastNewCommandId = 0x7FFF;

    void            ReadNewChoices ();
    static HRESULT  OpenNewMenu    (const std::wstring & folder, IContextMenu3 ** outMenu, HMENU & outPopup, HMENU & outChoices);

    std::thread             m_newReader;
    std::mutex              m_lock;
    std::vector<NewChoice>  m_newChoices;
    HWND                    m_notify        = nullptr;
    UINT                    m_notifyMessage = 0;
};
