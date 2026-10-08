#pragma once

#include "Pch.h"

#include "Update/ISignatureVerifier.h"
#include "Update/IUpdateFileSystem.h"
#include "Update/ReleaseInfo.h"
#include "Update/UpdateFailure.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstaller
//
//  Replaces a release-zip install with a newer release's files, in a way
//  that either finishes or puts the old copy back:
//
//    1. the download must match the release's size and SHA-256 digest
//    2. the zip must extract cleanly and hold Casso.exe
//    3. the install folder must be writable
//    4. the payload is written to <install>\.update-new
//    5. the staged Casso.exe must carry the official signature and the
//       release's version
//    6. each file is swapped: the existing one renamed into
//       <install>\.update-old, the staged one moved into place
//
//  Any failure in step 6 moves every renamed file back and removes what was
//  moved in. Nothing before step 6 touches an installed file. The old files
//  stay in .update-old after success, because the running Casso.exe is one
//  of them; the new process removes them once this one has exited.
//
//  Prepare runs steps 1 to 5 and Commit step 6, so an update applied when
//  Casso closes can be checked now and swapped in at exit.
//
////////////////////////////////////////////////////////////////////////////////

class ZipUpdateInstaller
{
public:
    static constexpr LPCWSTR  kpszStagingFolder = L".update-new";
    static constexpr LPCWSTR  kpszOldFolder     = L".update-old";
    static constexpr LPCWSTR  kpszProbeFile     = L".update-probe";
    static constexpr LPCSTR   kpszExeName       = "Casso.exe";

    ZipUpdateInstaller (IUpdateFileSystem & fileSystem, ISignatureVerifier & verifier);

    HRESULT         Install         (const std::wstring     & installDir,
                                     std::span<const Byte>    zipBytes,
                                     const ReleaseAsset     & asset,
                                     const ReleaseVersion   & version,
                                     UpdateFailure          & outFailure);

    HRESULT         Prepare         (const std::wstring          & installDir,
                                     std::span<const Byte>         zipBytes,
                                     const ReleaseAsset          & asset,
                                     const ReleaseVersion        & version,
                                     std::vector<std::string>    & outPaths,
                                     UpdateFailure               & outFailure);
    HRESULT         Commit          (const std::wstring              & installDir,
                                     const std::vector<std::string>  & paths,
                                     UpdateFailure                   & outFailure);

    static HRESULT  RemoveOldFiles  (IUpdateFileSystem & fileSystem, const std::wstring & installDir);
    static HRESULT  DiscardStaged   (IUpdateFileSystem & fileSystem, const std::wstring & installDir);

    static HRESULT  VerifyDownload  (std::span<const Byte> bytes, const ReleaseAsset & asset);
    static HRESULT  GetPayload      (std::vector<ZipEntry> & entries);

private:
    struct SwappedFile
    {
        std::wstring  target;
        std::wstring  backup;
        bool          isBackedUp  = false;
        bool          isInstalled = false;
    };

    HRESULT         ProbeWritable   (const std::wstring & installDir);
    HRESULT         Stage           (const std::wstring & stagingDir, const std::vector<ZipEntry> & entries);
    HRESULT         VerifyStaged    (const std::wstring     & stagingDir,
                                     const ReleaseVersion   & version,
                                     UpdateFailure          & outFailure);
    HRESULT         Swap            (const std::wstring               & installDir,
                                     const std::vector<std::string>   & paths,
                                     std::vector<SwappedFile>         & swapped);
    HRESULT         SwapOne         (const std::wstring & installDir,
                                     const std::string  & path,
                                     SwappedFile        & swapped);
    HRESULT         Restore         (const std::vector<SwappedFile> & swapped);

    static std::wstring  JoinPath   (const std::wstring & dir, std::string_view relativePath);
    static std::wstring  GetParent  (const std::wstring & path);

    IUpdateFileSystem   & m_fileSystem;
    ISignatureVerifier  & m_verifier;
};
