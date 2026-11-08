#pragma once

#include "Pch.h"

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
    static HRESULT  NoteRename (IDiskFileIo & fileIo, const std::string & folder, const std::string & oldName, const std::string & newName);
};