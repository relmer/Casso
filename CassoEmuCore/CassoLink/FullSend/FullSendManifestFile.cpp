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
//  FullSendManifestFile::NoteBoot
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::NoteBoot (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, const std::string & boot)
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

    changed = manifest.SetBoot (name, boot);
    BAIL_OUT_IF (!changed, S_OK);

    hr = Write (fileIo, path, manifest);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::RenameCapture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::RenameCapture (IDiskFileIo & fileIo, const std::string & folder, const std::string & oldName, const std::string & newName, bool & outRenamed)
{
    HRESULT           hr       = S_OK;
    std::string       path     = folder + "\\" + FullSendManifest::kFileName;
    std::string       oldPath  = folder + "\\" + oldName;
    std::string       newPath  = folder + "\\" + newName;
    std::string       reads    = ".reads";
    FullSendManifest  manifest;
    bool              exists   = false;
    bool              isListed = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    outRenamed = false;

    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);
    BAIL_OUT_IF (!exists, S_OK);

    isListed = std::any_of (manifest.GetRows().begin(), manifest.GetRows().end(), [&oldName] (const FullSendRow & row)
    {
        return row.status == FullSendManifest::kStatusRawCapture && _stricmp (row.file.c_str(), oldName.c_str()) == 0;
    });

    BAIL_OUT_IF (!isListed || !fileIo.Exists (oldPath) || fileIo.Exists (newPath), S_OK);

    hr = fileIo.RenameWithoutReplacing (oldPath, newPath);
    CHR (hr);

    if (fileIo.Exists (oldPath + reads) && !fileIo.Exists (newPath + reads))
    {
        hr = fileIo.RenameWithoutReplacing (oldPath + reads, newPath + reads);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    manifest.RenameImage (oldName, newName);

    hr = Write (fileIo, path, manifest);
    CHR (hr);

    outRenamed = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::NoteDelete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::NoteDelete (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, FullSendRemoval & outRemoval)
{
    HRESULT           hr      = S_OK;
    std::string       path    = folder + "\\" + FullSendManifest::kFileName;
    FullSendManifest  manifest;
    bool              exists  = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    outRemoval = FullSendRemoval();

    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);
    BAIL_OUT_IF (!exists, S_OK);

    outRemoval = manifest.RemoveImage (name);
    BAIL_OUT_IF (outRemoval.IsEmpty(), S_OK);

    hr = Write (fileIo, path, manifest);
    CHRF (hr, outRemoval = FullSendRemoval());

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

HRESULT FullSendManifestFile::NoteRestore (IDiskFileIo & fileIo, const std::string & folder, const FullSendRemoval & removal)
{
    HRESULT           hr      = S_OK;
    std::string       path    = folder + "\\" + FullSendManifest::kFileName;
    FullSendManifest  manifest;
    bool              exists  = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    BAIL_OUT_IF (removal.IsEmpty(), S_OK);

    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);

    manifest.RestoreRows (removal);

    hr = Write (fileIo, path, manifest);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifestFile::FindFingerprint
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifestFile::FindFingerprint (IDiskFileIo & fileIo, const std::string & folder, const std::string & name, std::string & outFingerprint, bool & outIsRawCapture)
{
    HRESULT           hr      = S_OK;
    std::string       path    = folder + "\\" + FullSendManifest::kFileName;
    FullSendManifest  manifest;
    bool              exists  = false;
    std::lock_guard   lock (FullSendSession::GetFolderLock (folder));



    outFingerprint.clear();
    outIsRawCapture = false;

    hr = Read (fileIo, path, manifest, exists);
    CHR (hr);
    BAIL_OUT_IF (!exists, S_OK);

    for (const FullSendRow & row : manifest.GetRows())
    {
        if (_stricmp (row.file.c_str(), name.c_str()) == 0)
        {
            outFingerprint  = row.fingerprint;
            outIsRawCapture = row.status == FullSendManifest::kStatusRawCapture;
        }
    }

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