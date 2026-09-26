#include "Pch.h"

#include "CassoExplorer/Model/FolderOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions::IsShown
//
////////////////////////////////////////////////////////////////////////////////

bool FolderOptions::IsShown (const FileSystemEntry & entry) const
{
    if (!entry.isHidden)
    {
        return true;
    }

    if (!showHidden)
    {
        return false;
    }

    return !entry.isSystem || showProtected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions::ReadFromShell
//
////////////////////////////////////////////////////////////////////////////////

FolderOptions FolderOptions::ReadFromShell()
{
    SHELLSTATEW    state   = {};
    FolderOptions  options;



    SHGetSetSettings (&state, SSF_SHOWALLOBJECTS | SSF_SHOWSUPERHIDDEN | SSF_SHOWCOMPCOLOR, FALSE);

    options.showHidden      = state.fShowAllObjects != 0;
    options.showProtected   = state.fShowSuperHidden != 0;
    options.colorCompressed = state.fShowCompColor != 0;

    return options;
}
