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

    //  A file in `folder` deleted: the rows NoteDelete removed are returned,
    //  for NoteRestore when the file is restored. Each writes the manifest
    //  only when it changes it.
    static HRESULT  NoteDelete  (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, std::vector<FullSendRow> & outRows);
    static HRESULT  NoteRestore (IDiskFileIo & fileIo, const std::string & folder, const std::vector<FullSendRow> & rows);

private:
    static HRESULT  Read  (IDiskFileIo & fileIo, const std::string & path, FullSendManifest & outManifest, bool & outExists);
    static HRESULT  Write (IDiskFileIo & fileIo, const std::string & path, const FullSendManifest & manifest);
};