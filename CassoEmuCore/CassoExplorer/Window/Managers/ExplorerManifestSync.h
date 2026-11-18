#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerManifestSync
//
//  A Full Send folder's manifest updated for what Casso Explorer does to its
//  images: a rename renames the image's rows, a deletion removes them, and a
//  restore, by Undo or from the Recycle Bin while this window is open, puts
//  them back.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerManifestSync
{
public:
    explicit ExplorerManifestSync (ExplorerContext & context);

    void  NoteRename   (const std::wstring & oldPath, const std::wstring & newName);
    void  NoteDeletes  (const std::vector<std::wstring> & paths);
    void  NoteRestores (const std::vector<std::wstring> & paths);

private:
    //  What a deleted image's deletion removed and changed in the manifest,
    //  and the folder as it was deleted from.
    struct RemovedRows
    {
        std::string      folder;
        FullSendRemoval  removal;
    };

    ExplorerContext                      & m_context;
    std::map<std::wstring, RemovedRows>    m_removedRows;   // by the image's path in lowercase
};