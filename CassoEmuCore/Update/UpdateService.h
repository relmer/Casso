#pragma once

#include "Pch.h"

#include "Net/IHttpClient.h"
#include "Update/IInstallEnvironment.h"
#include "Update/IPackageDeployer.h"
#include "Update/ISignatureVerifier.h"
#include "Update/IUpdateFileSystem.h"
#include "Update/IUpdateHost.h"
#include "Update/IUpdateResultPoster.h"
#include "Update/UpdateResult.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateServiceDeps
//
//  Everything UpdateService reaches outside itself. The clock returns Unix
//  seconds.
//
////////////////////////////////////////////////////////////////////////////////

struct UpdateServiceDeps
{
    IHttpClient                    * http        = nullptr;
    ISignatureVerifier             * verifier    = nullptr;
    IInstallEnvironment            * environment = nullptr;
    IUpdateFileSystem              * fileSystem  = nullptr;
    IPackageDeployer               * deployer    = nullptr;
    IUpdateHost                    * host        = nullptr;
    IUpdateResultPoster            * poster      = nullptr;
    std::function<std::int64_t()>    clock;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService
//
//  Runs every slow part of an update off the UI thread: the release check,
//  the release-notes fetch, the download and install, and the removal of
//  the old files after a zip update. Each kind of work has its own worker
//  thread, and a Start call refuses (E_PENDING) while the same kind is
//  still running. Results reach the UI thread through the poster.
//
//  Stop cancels a download in flight and joins every worker; the owner
//  calls it before anything the workers use is destroyed.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateService
{
public:
    static constexpr LPCWSTR  kpszApiHost        = L"api.github.com";
    static constexpr LPCWSTR  kpszLatestPath     = L"/repos/relmer/Casso/releases/latest";
    static constexpr LPCWSTR  kpszRawHost        = L"raw.githubusercontent.com";
    static constexpr LPCWSTR  kpszRepoRawPath    = L"/relmer/Casso/";
    static constexpr LPCWSTR  kpszApiHeaders     = L"Accept: application/vnd.github+json\r\n";
    static constexpr LPCWSTR  kpszReleasesPage   = L"https://github.com/relmer/Casso/releases/latest";
    static constexpr LPCWSTR  kpszUpdatedArg     = L"--updated";
    static constexpr LPCWSTR  kpszCleanupOldArg  = L"--cleanup-old";
    static constexpr DWORD    kStatusOk          = 200;
    static constexpr DWORD    kStatusForbidden   = 403;
    static constexpr DWORD    kStatusTooMany     = 429;
    static constexpr DWORD    kCleanupWaitMs     = 30000;
    static constexpr size_t   kMaxImageBytes     = 8 * 1024 * 1024;

    explicit UpdateService (const UpdateServiceDeps & deps);
    ~UpdateService();

    UpdateService             (const UpdateService &) = delete;
    UpdateService & operator= (const UpdateService &) = delete;

    HRESULT  StartCheck      (UpdateCheckTrigger      trigger,
                              const ReleaseVersion  & running,
                              const std::string     & skippedVersion);
    HRESULT  StartFetchNotes (const ReleaseInfo & release, const ReleaseVersion & running);
    HRESULT  StartApply      (const ReleaseInfo & release, InstallType installType, ReleaseArch arch);
    HRESULT  StartDeploy     (const std::wstring & bundlePath);
    HRESULT  StartCleanup    (DWORD oldProcessId);
    HRESULT  StartFetchImages (const std::string & tag, const std::vector<std::string> & sources);

    void     CancelApply     ();
    void     CancelImages    ();
    void     Wait            ();
    void     Stop            ();

    bool     IsApplying      () const { return m_isApplyBusy; }
    void     GetProgress     (std::uint64_t & outBytesDone, std::uint64_t & outBytesTotal) const;

    static UpdateFailure  MapCheckResponse (HRESULT hrGet, DWORD statusCode);
    static std::wstring   MakeNotesPath    (const std::string & tag, LPCWSTR fileName);
    static bool           TrySplitUrl      (const std::wstring & url, std::wstring & outHost, std::wstring & outPath);
    static std::wstring   MakeRelaunchArgs (DWORD oldProcessId);
    static bool           TryResolveImageUrl (const std::string & src, const std::string & tag, std::wstring & outUrl);
    static HRESULT        DecodeImage      (std::span<const Byte> bytes, NotesImage & outImage);

private:
    struct NotesJob
    {
        ReleaseInfo     release;
        ReleaseVersion  running;
    };

    struct ApplyJob
    {
        ReleaseInfo     release;
        InstallType     installType = InstallType::Unknown;
        ReleaseArch     arch        = ReleaseArch::X64;
    };

    void     RunCheck        (UpdateCheckTrigger trigger, ReleaseVersion running, std::string skippedVersion);
    void     RunFetchNotes   (NotesJob job);
    void     RunApply        (ApplyJob job);
    void     RunDeploy       (std::wstring bundlePath);
    void     RunCleanup      (DWORD oldProcessId);
    void     RunFetchImages  (std::string tag, std::vector<std::string> sources);
    HRESULT  FetchImage      (const std::wstring & url, std::shared_ptr<const NotesImage> & outImage, UpdateResult & result);

    HRESULT  FetchText       (LPCWSTR host, const std::wstring & path, std::string & outText);
    HRESULT  FetchNotes      (const NotesJob & job, ReleaseNotes & outNotes);
    HRESULT  Download        (const ReleaseAsset & asset, std::vector<Byte> & outBytes, UpdateResult & result);
    HRESULT  InstallZip      (const ApplyJob & job, const ReleaseAsset & asset, std::span<const Byte> bytes, UpdateResult & result);
    HRESULT  StageBundle     (const ReleaseAsset & asset, std::span<const Byte> bytes, UpdateResult & result);
    HRESULT  GetInstallDir   (std::wstring & outDir, std::wstring & outExePath);

    void     Post            (std::unique_ptr<UpdateResult> result);

    static const ReleaseAsset *  FindAsset (const ApplyJob & job);
    static void                  ReapIfIdle (std::thread & worker, const std::atomic<bool> & isBusy);

    UpdateServiceDeps                        m_deps;
    std::atomic<bool>                        m_isCheckBusy   = false;
    std::atomic<bool>                        m_isNotesBusy   = false;
    std::atomic<bool>                        m_isApplyBusy   = false;
    std::atomic<bool>                        m_isCleanupBusy = false;
    std::atomic<bool>                        m_isImagesBusy  = false;
    std::atomic<bool>                        m_imagesCancel  = false;
    std::atomic<bool>                        m_cancel        = false;
    std::atomic<bool>                        m_stopping      = false;
    std::atomic<std::uint64_t>               m_bytesDone     = 0;
    std::atomic<std::uint64_t>               m_bytesTotal    = 0;
    std::mutex                               m_notesMutex;
    std::map<std::string, ReleaseNotes>      m_notesCache;
    std::map<std::wstring, std::shared_ptr<const NotesImage>>  m_imageCache;
    std::thread                              m_checkThread;
    std::thread                              m_notesThread;
    std::thread                              m_applyThread;
    std::thread                              m_cleanupThread;
    std::thread                              m_imagesThread;
};
