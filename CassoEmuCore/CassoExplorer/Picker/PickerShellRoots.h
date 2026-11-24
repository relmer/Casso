#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots
//
//  The shell's navigation roots and Quick access folders, read on a
//  background thread before a picker opens. Reading them takes most of a
//  second, which would otherwise delay the picker's window. Each read
//  replaces the folders of the one before.
//
////////////////////////////////////////////////////////////////////////////////

class PickerShellRoots
{
public:
    using Folders = std::vector<IShellItemVerbs::ShellFolderItem>;
    using Reader  = std::function<HRESULT (Folders & outNavRoots, Folders & outPinned)>;

    explicit PickerShellRoots (Reader reader);
    ~PickerShellRoots();

    //  Starts a read on a background thread, unless one is under way.
    void  Refresh ();

    //  The folders of the last read that finished; false before one has.
    bool  TryGet  (Folders & outNavRoots, Folders & outPinned) const;

    //  Reads the folders from the shell, as Casso Explorer's tree does.
    static HRESULT  ReadFromShell (Folders & outNavRoots, Folders & outPinned);

private:
    void  Read ();

    Reader              m_reader;
    mutable std::mutex  m_lock;
    std::thread         m_thread;
    bool                m_isReading = false;
    bool                m_hasRead   = false;
    Folders             m_navRoots;
    Folders             m_pinned;
};
