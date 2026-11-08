#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "Devices/Disk/DurableCommit.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::NoteRename
//
//  Under the folder's lock, as a session sending to the folder reads the
//  manifest again before each disk and writes it after.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::NoteRename (IDiskFileIo & fileIo, const std::string & folder, const std::string & oldName, const std::string & newName)
{
    HRESULT               hr       = S_OK;
    std::string           path     = folder + "\\" + FullSendManifest::kFileName;
    std::vector<Byte>     bytes;
    FullSendManifest      manifest;
    CommitPlan::Progress  progress;
    bool                  changed  = false;
    std::lock_guard       lock (FullSendSession::GetFolderLock (folder));



    BAIL_OUT_IF (!fileIo.Exists (path), S_OK);

    hr = fileIo.ReadAllBytes (path, bytes);
    CHR (hr);

    hr = manifest.Parse (bytes);
    CHR (hr);

    changed = manifest.RenameImage (oldName, newName);
    BAIL_OUT_IF (!changed, S_OK);

    hr = DurableCommit::Commit (fileIo, path, manifest.Format(), GetTickCount64(), CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}