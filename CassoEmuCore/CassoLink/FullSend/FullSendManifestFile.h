#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile
//
//  A Full Send folder's manifest.fullsend, updated for renames made in Casso
//  Explorer, so the manifest lists each image by its current name.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendManifestFile
{
public:
    //  A file in `folder` renamed from oldName to newName. A folder with no
    //  manifest, or a manifest with no row for the old name, is left as it
    //  is. A manifest that can't be parsed fails with ERROR_INVALID_DATA and
    //  is not written.
    static HRESULT  NoteRename  (IDiskFileIo & fileIo, const std::string & folder, const std::string & oldName, const std::string & newName);

    //  The boot result of a file in `folder`, written to its row. A folder
    //  with no manifest, or no row for the file, is left as it is.
    static HRESULT  NoteBoot    (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, const std::string & boot);

    //  A file in `folder` deleted: what NoteDelete removed and changed is
    //  returned, for NoteRestore when the file is restored. Each writes the
    //  manifest only when it changes it.
    static HRESULT  NoteDelete  (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, FullSendRemoval & outRemoval);
    static HRESULT  NoteRestore (IDiskFileIo & fileIo, const std::string & folder, const FullSendRemoval & removal);

private:
    static HRESULT  Read  (IDiskFileIo & fileIo, const std::string & path, FullSendManifest & outManifest, bool & outExists);
    static HRESULT  Write (IDiskFileIo & fileIo, const std::string & path, const FullSendManifest & manifest);
};