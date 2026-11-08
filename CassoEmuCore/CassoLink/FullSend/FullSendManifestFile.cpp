#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "Devices/Disk/DurableCommit.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::NoteRename
//
//  Each change is made under the folder's lock, as a session sending to the
//  folder reads the manifest again before each disk and writes it after.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::NoteRename (IDiskFileIo & fileIo, const std::string & folder, const std::string & oldName, const std::string & newName)
{
    HRESULT           hr      = S_OK;
    std::string       path    = folder + "\\" + FullSendManifest::kFileName;
    FullSendManifest  manifest;
    bool              exists  = false;
    bool              changed = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);
    BAIL_OUT_IF (!exists, S_OK);

    changed = manifest.RenameImage (oldName, newName);
    BAIL_OUT_IF (!changed, S_OK);

    hr = Write (fileIo, path, manifest);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::NoteDelete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::NoteDelete (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, std::vector<FullSendRow> & outRows)
{
    HRESULT           hr      = S_OK;
    std::string       path    = folder + "\\" + FullSendManifest::kFileName;
    FullSendManifest  manifest;
    bool              exists  = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    outRows.clear();

    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);
    BAIL_OUT_IF (!exists, S_OK);

    outRows = manifest.RemoveImage (name);
    BAIL_OUT_IF (outRows.empty(), S_OK);

    hr = Write (fileIo, path, manifest);
    CHRF (hr, outRows.clear());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::NoteRestore
//
//  A folder whose manifest was deleted as well gets a new one with the rows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::NoteRestore (IDiskFileIo & fileIo, const std::string & folder, const std::vector<FullSendRow> & rows)
{
    HRESULT           hr      = S_OK;
    std::string       path    = folder + "\\" + FullSendManifest::kFileName;
    FullSendManifest  manifest;
    bool              exists  = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    BAIL_OUT_IF (rows.empty(), S_OK);

    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);

    manifest.RestoreRows (rows);

    hr = Write (fileIo, path, manifest);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::Read
//
//  A folder with no manifest reads as an empty one, with outExists false.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::Read (IDiskFileIo & fileIo, const std::string & path, FullSendManifest & outManifest, bool & outExists)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  bytes;



    outExists = fileIo.Exists (path);
    BAIL_OUT_IF (!outExists, S_OK);

    hr = fileIo.ReadAllBytes (path, bytes);
    CHR (hr);

    hr = outManifest.Parse (bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::Write (IDiskFileIo & fileIo, const std::string & path, const FullSendManifest & manifest)
{
    CommitPlan::Progress  progress;



    return DurableCommit::Commit (fileIo, path, manifest.Format(), GetTickCount64(), CommitMode::Replace, progress);
}