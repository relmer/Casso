#include "Pch.h"

#include "Update/ZipUpdateInstaller.h"
#include "Update/Sha256Digest.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::ZipUpdateInstaller
//
////////////////////////////////////////////////////////////////////////////////

ZipUpdateInstaller::ZipUpdateInstaller (IUpdateFileSystem & fileSystem, ISignatureVerifier & verifier) :
    m_fileSystem (fileSystem),
    m_verifier   (verifier)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::Install
//
//  The whole sequence described on the class. `outFailure` gives the step
//  that failed; on RestoreFailed the old copy could not be fully put back.
//  The staging folder is removed whatever the outcome, once it exists.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::Install (
    const std::wstring     & installDir,
    std::span<const Byte>    zipBytes,
    const ReleaseAsset     & asset,
    const ReleaseVersion   & version,
    UpdateFailure          & outFailure)
{
    HRESULT                   hr          = S_OK;
    HRESULT                   hrRestore   = S_OK;
    HRESULT                   hrCleanup   = S_OK;
    std::wstring              stagingDir  = installDir + L"\\" + kpszStagingFolder;
    std::wstring              oldDir      = installDir + L"\\" + kpszOldFolder;
    std::vector<ZipEntry>     entries;
    std::vector<SwappedFile>  swapped;
    bool                      isStaging   = false;



    outFailure = UpdateFailure::None;

    hr = VerifyDownload (zipBytes, asset);
    CHRF (hr, outFailure = UpdateFailure::DigestMismatch);

    hr = ZipArchive::Extract (zipBytes, entries);
    CHRF (hr, outFailure = UpdateFailure::BadData);

    hr = GetPayload (entries);
    CHRF (hr, outFailure = UpdateFailure::BadData);

    hr = ProbeWritable (installDir);
    CHRF (hr, outFailure = UpdateFailure::FolderNotWritable);

    isStaging = true;

    hr = m_fileSystem.RemoveDirectoryTree (stagingDir);
    CHRF (hr, outFailure = UpdateFailure::InstallFailed);

    hr = Stage (stagingDir, entries);
    CHRF (hr, outFailure = UpdateFailure::InstallFailed);

    hr = VerifyStaged (stagingDir, version, outFailure);
    CHR (hr);

    hr = m_fileSystem.RemoveDirectoryTree (oldDir);
    CHRF (hr, outFailure = UpdateFailure::InstallFailed);

    hr = Swap (installDir, entries, swapped);

    if (FAILED (hr))
    {
        hrRestore  = Restore (swapped);
        outFailure = FAILED (hrRestore) ? UpdateFailure::RestoreFailed : UpdateFailure::InstallFailed;
    }

    CHR (hr);

Error:
    if (isStaging)
    {
        hrCleanup = m_fileSystem.RemoveDirectoryTree (stagingDir);
        IGNORE_RETURN_VALUE (hrCleanup, S_OK);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::RemoveOldFiles
//
//  Deletes the files a successful update moved aside. Run by the new
//  process once the old one has exited and released Casso.exe.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::RemoveOldFiles (IUpdateFileSystem & fileSystem, const std::wstring & installDir)
{
    HRESULT  hr = S_OK;



    hr = fileSystem.RemoveDirectoryTree (installDir + L"\\" + kpszOldFolder);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::VerifyDownload
//
//  The bytes must be the published asset: its size when GitHub gave one,
//  and its SHA-256 digest, which is required. A release without a digest
//  cannot be verified and is not installed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::VerifyDownload (std::span<const Byte> bytes, const ReleaseAsset & asset)
{
    HRESULT            hr           = S_OK;
    std::vector<Byte>  digest;
    bool               isRightSize  = asset.sizeBytes == 0 || bytes.size() == asset.sizeBytes;
    bool               hasDigest    = asset.sha256.size() == Sha256Digest::kDigestBytes;
    bool               isMatch      = false;



    CBREx (isRightSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (hasDigest,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = Sha256Digest::Compute (bytes, digest);
    CHR (hr);

    isMatch = digest == asset.sha256;
    CBREx (isMatch, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::GetPayload
//
//  Rewrites `entries` relative to the folder that holds Casso.exe. The
//  release zip wraps everything in one top folder (Casso-x64\...), and a zip
//  without one is accepted too. Separators become '/'. An archive without
//  Casso.exe, or with anything outside its folder, is ERROR_INVALID_DATA.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::GetPayload (std::vector<ZipEntry> & entries)
{
    HRESULT                hr       = S_OK;
    std::string            prefix;
    std::string            name;
    bool                   hasExe   = false;
    bool                   isInside = false;
    size_t                 slash    = 0;
    std::vector<ZipEntry>  payload;



    for (ZipEntry & entry : entries)
    {
        std::replace (entry.path.begin(), entry.path.end(), '\\', '/');

        slash = entry.path.rfind ('/');
        name  = (slash == std::string::npos) ? entry.path : entry.path.substr (slash + 1);

        if (!hasExe && _stricmp (name.c_str(), kpszExeName) == 0 && entry.path.find ('/') == slash)
        {
            hasExe = true;
            prefix = (slash == std::string::npos) ? std::string() : entry.path.substr (0, slash + 1);
        }
    }

    CBREx (hasExe, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (ZipEntry & entry : entries)
    {
        if (entry.path == prefix)
        {
            continue;
        }

        isInside = entry.path.starts_with (prefix);
        CBREx (isInside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        entry.path = entry.path.substr (prefix.size());
        payload.push_back (std::move (entry));
    }

    entries = std::move (payload);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::ProbeWritable
//
//  Creates and deletes a probe file, so a folder the user cannot write
//  (Program Files without elevation) is found before anything is staged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::ProbeWritable (const std::wstring & installDir)
{
    HRESULT       hr    = S_OK;
    std::wstring  probe = installDir + L"\\" + kpszProbeFile;



    hr = m_fileSystem.WriteAllBytes (probe, std::span<const Byte>());
    CHR (hr);

    hr = m_fileSystem.RemoveFile (probe);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::Stage
//
//  Writes the payload under the staging folder, creating folders as needed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::Stage (const std::wstring & stagingDir, const std::vector<ZipEntry> & entries)
{
    HRESULT       hr = S_OK;
    std::wstring  path;



    hr = m_fileSystem.CreateDirectoryTree (stagingDir);
    CHR (hr);

    for (const ZipEntry & entry : entries)
    {
        path = JoinPath (stagingDir, entry.path);

        if (entry.isDirectory)
        {
            hr = m_fileSystem.CreateDirectoryTree (path);
            CHR (hr);
            continue;
        }

        hr = m_fileSystem.CreateDirectoryTree (GetParent (path));
        CHR (hr);

        hr = m_fileSystem.WriteAllBytes (path, entry.data);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::VerifyStaged
//
//  The staged Casso.exe must be signed by Casso's publisher and must be the
//  version the release says it is, so a zip that is not what it claims is
//  never swapped in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::VerifyStaged (
    const std::wstring     & stagingDir,
    const ReleaseVersion   & version,
    UpdateFailure          & outFailure)
{
    HRESULT         hr             = S_OK;
    std::wstring    exePath        = JoinPath (stagingDir, kpszExeName);
    bool            isOfficial     = false;
    bool            isRightVersion = false;
    ReleaseVersion  stagedVersion;



    hr = m_verifier.IsOfficialFile (exePath, isOfficial);
    CHRF (hr, outFailure = UpdateFailure::NotOfficial);

    CBRFEx (isOfficial, TRUST_E_NOSIGNATURE, outFailure = UpdateFailure::NotOfficial);

    hr = m_fileSystem.GetFileVersion (exePath, stagedVersion);
    CHRF (hr, outFailure = UpdateFailure::BadData);

    isRightVersion = stagedVersion == version;
    CBRFEx (isRightVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outFailure = UpdateFailure::BadData);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::Swap
//
//  Swaps every file entry in. `swapped` records each file as far as it got,
//  including the one that failed, which is what Restore walks back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::Swap (
    const std::wstring           & installDir,
    const std::vector<ZipEntry>  & entries,
    std::vector<SwappedFile>     & swapped)
{
    HRESULT  hr = S_OK;



    swapped.clear();

    for (const ZipEntry & entry : entries)
    {
        if (entry.isDirectory)
        {
            continue;
        }

        swapped.emplace_back();

        hr = SwapOne (installDir, entry.path, swapped.back());
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::SwapOne
//
//  Renames the installed file (if any) into .update-old, then moves the
//  staged file into its place. Rename rather than overwrite: Windows lets a
//  running executable be renamed but not replaced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::SwapOne (
    const std::wstring & installDir,
    const std::string  & path,
    SwappedFile        & swapped)
{
    HRESULT       hr       = S_OK;
    std::wstring  staged   = JoinPath (installDir + L"\\" + kpszStagingFolder, path);
    bool          isThere  = false;



    swapped.target = JoinPath (installDir, path);
    swapped.backup = JoinPath (installDir + L"\\" + kpszOldFolder, path);

    isThere = m_fileSystem.Exists (swapped.target);

    if (isThere)
    {
        hr = m_fileSystem.CreateDirectoryTree (GetParent (swapped.backup));
        CHR (hr);

        hr = m_fileSystem.RenameFile (swapped.target, swapped.backup);
        CHR (hr);

        swapped.isBackedUp = true;
    }

    hr = m_fileSystem.CreateDirectoryTree (GetParent (swapped.target));
    CHR (hr);

    hr = m_fileSystem.RenameFile (staged, swapped.target);
    CHR (hr);

    swapped.isInstalled = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::Restore
//
//  Undoes a partial swap, newest first: removes each file moved in and
//  renames its original back. Keeps going past a failure so as much as
//  possible is put back, and returns the first failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ZipUpdateInstaller::Restore (const std::vector<SwappedFile> & swapped)
{
    HRESULT  hr     = S_OK;
    HRESULT  hrStep = S_OK;



    for (auto it = swapped.rbegin(); it != swapped.rend(); it++)
    {
        if (it->isInstalled)
        {
            hrStep = m_fileSystem.RemoveFile (it->target);

            if (FAILED (hrStep))
            {
                hr = SUCCEEDED (hr) ? hrStep : hr;
                continue;
            }
        }

        if (it->isBackedUp)
        {
            hrStep = m_fileSystem.RenameFile (it->backup, it->target);
            hr     = (SUCCEEDED (hr) && FAILED (hrStep)) ? hrStep : hr;
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::JoinPath
//
//  `dir` plus a '/'-separated relative path, as a Windows path.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ZipUpdateInstaller::JoinPath (const std::wstring & dir, std::string_view relativePath)
{
    std::wstring  relative = TextEncoding::Utf8ToWide (std::string (relativePath));



    std::replace (relative.begin(), relative.end(), L'/', L'\\');

    while (!relative.empty() && relative.back() == L'\\')
    {
        relative.pop_back();
    }

    return dir + L"\\" + relative;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller::GetParent
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ZipUpdateInstaller::GetParent (const std::wstring & path)
{
    size_t  slash = path.rfind (L'\\');



    return (slash == std::wstring::npos) ? std::wstring() : path.substr (0, slash);
}
