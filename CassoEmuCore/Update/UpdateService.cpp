#include "Pch.h"

#include "Update/UpdateService.h"
#include "Update/ZipUpdateInstaller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::UpdateService
//
////////////////////////////////////////////////////////////////////////////////

UpdateService::UpdateService (const UpdateServiceDeps & deps) :
    m_deps (deps)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::~UpdateService
//
//  A worker still running would touch the seams after their owner freed
//  them, so the service never outlives its threads.
//
////////////////////////////////////////////////////////////////////////////////

UpdateService::~UpdateService()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::ReapIfIdle
//
//  Joins a worker that has finished, so its slot can take the next job. A
//  worker clears its busy flag as its last act, so the join is immediate.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::ReapIfIdle (std::thread & worker, const std::atomic<bool> & isBusy)
{
    if (!isBusy && worker.joinable())
    {
        worker.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StartCheck
//
//  Starts a check for the latest release. The automatic check runs in one
//  instance only; the worker takes the check lock and ends quietly when
//  another instance holds it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartCheck (
    UpdateCheckTrigger      trigger,
    const ReleaseVersion  & running,
    const std::string     & skippedVersion)
{
    HRESULT  hr = S_OK;



    ReapIfIdle (m_checkThread, m_isCheckBusy);

    BAIL_OUT_IF (m_stopping,    E_ABORT);
    BAIL_OUT_IF (m_isCheckBusy, E_PENDING);

    m_isCheckBusy = true;
    m_checkThread = std::thread ([this, trigger, running, skippedVersion]
    {
        RunCheck (trigger, running, skippedVersion);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StartFetchNotes
//
//  Starts fetching a release's notes from its own tag. A release fetched
//  once this session is answered from the cache.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartFetchNotes (const ReleaseInfo & release, const ReleaseVersion & running)
{
    HRESULT   hr  = S_OK;
    NotesJob  job;



    ReapIfIdle (m_notesThread, m_isNotesBusy);

    BAIL_OUT_IF (m_stopping,    E_ABORT);
    BAIL_OUT_IF (m_isNotesBusy, E_PENDING);

    job.release = release;
    job.running = running;

    m_isNotesBusy = true;
    m_notesThread = std::thread ([this, job]
    {
        RunFetchNotes (job);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StartApply
//
//  Starts downloading and installing a release by the method for this
//  copy's install type.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartApply (const ReleaseInfo & release, InstallType installType, ReleaseArch arch)
{
    HRESULT   hr  = S_OK;
    ApplyJob  job;



    ReapIfIdle (m_applyThread, m_isApplyBusy);

    BAIL_OUT_IF (m_stopping,    E_ABORT);
    BAIL_OUT_IF (m_isApplyBusy, E_PENDING);

    job.release     = release;
    job.installType = installType;
    job.arch        = arch;

    m_cancel      = false;
    m_bytesDone   = 0;
    m_bytesTotal  = 0;
    m_isApplyBusy = true;
    m_applyThread = std::thread ([this, job]
    {
        RunApply (job);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StartDeploy
//
//  Starts installing a downloaded MSIX bundle. The UI thread calls this
//  after it has flushed disks and settings, since a successful deploy ends
//  this process without running its shutdown.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartDeploy (const std::wstring & bundlePath)
{
    HRESULT  hr = S_OK;



    ReapIfIdle (m_applyThread, m_isApplyBusy);

    BAIL_OUT_IF (m_stopping,    E_ABORT);
    BAIL_OUT_IF (m_isApplyBusy, E_PENDING);

    m_isApplyBusy = true;
    m_applyThread = std::thread ([this, bundlePath]
    {
        RunDeploy (bundlePath);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StartCleanup
//
//  Starts removing the files a zip update set aside, once the process that
//  was running them has exited.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartCleanup (DWORD oldProcessId)
{
    HRESULT  hr = S_OK;



    ReapIfIdle (m_cleanupThread, m_isCleanupBusy);

    BAIL_OUT_IF (m_stopping,      E_ABORT);
    BAIL_OUT_IF (m_isCleanupBusy, E_PENDING);

    m_isCleanupBusy = true;
    m_cleanupThread = std::thread ([this, oldProcessId]
    {
        RunCleanup (oldProcessId);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::CancelApply
//
//  Stops a download in flight. Once the install step has begun the request
//  has no effect, since stopping a file swap halfway is what the restore
//  path exists to avoid.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::CancelApply()
{
    m_cancel = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::Wait
//
//  Joins every worker. Tests use it to let the work finish; Stop uses it
//  after canceling.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::Wait()
{
    for (std::thread * worker : { &m_checkThread, &m_notesThread, &m_applyThread, &m_cleanupThread })
    {
        if (worker->joinable())
        {
            worker->join();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::Stop
//
//  Shutdown: cancel what can be canceled, post nothing more, and join.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::Stop()
{
    m_stopping = true;
    m_cancel   = true;

    Wait();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::GetProgress
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::GetProgress (std::uint64_t & outBytesDone, std::uint64_t & outBytesTotal) const
{
    outBytesDone  = m_bytesDone;
    outBytesTotal = m_bytesTotal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::MapCheckResponse
//
//  No response at all is a network failure. GitHub answers a request over
//  its rate limit with 403 or 429. Any other status that is not 200 means
//  the answer is not the release record.
//
////////////////////////////////////////////////////////////////////////////////

UpdateFailure UpdateService::MapCheckResponse (HRESULT hrGet, DWORD statusCode)
{
    UpdateFailure  failure = UpdateFailure::None;



    if (FAILED (hrGet))
    {
        failure = UpdateFailure::Network;
    }
    else if (statusCode == kStatusForbidden || statusCode == kStatusTooMany)
    {
        failure = UpdateFailure::RateLimited;
    }
    else if (statusCode != kStatusOk)
    {
        failure = UpdateFailure::BadData;
    }

    return failure;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::MakeNotesPath
//
//  The raw path of one file as a release tag has it, such as
//  "/relmer/Casso/v1.31.0/CHANGELOG.md".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateService::MakeNotesPath (const std::string & tag, LPCWSTR fileName)
{
    std::wstring  path = kpszRepoRawPath;



    path.append (tag.begin(), tag.end());
    path += L"/";
    path += fileName;

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::TrySplitUrl
//
//  Splits an https URL into the host and the path WinHTTP takes. Anything
//  that is not https, or has no path, is refused.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateService::TrySplitUrl (const std::wstring & url, std::wstring & outHost, std::wstring & outPath)
{
    constexpr std::wstring_view  kScheme = L"https://";



    std::wstring_view  rest;
    size_t             slash   = 0;
    bool               isSplit = false;



    if (url.starts_with (kScheme))
    {
        rest  = std::wstring_view (url).substr (kScheme.size());
        slash = rest.find (L'/');

        if (slash != std::wstring_view::npos && slash > 0 && slash + 1 < rest.size())
        {
            outHost = std::wstring (rest.substr (0, slash));
            outPath = std::wstring (rest.substr (slash));
            isSplit = true;
        }
    }

    return isSplit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::MakeRelaunchArgs
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateService::MakeRelaunchArgs (DWORD oldProcessId)
{
    return std::format (L"{} {} {}", kpszUpdatedArg, kpszCleanupOldArg, oldProcessId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::Post
//
//  Nothing is posted once shutdown has begun: the window that would
//  receive it is about to go.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::Post (std::unique_ptr<UpdateResult> result)
{
    if (!m_stopping && m_deps.poster != nullptr)
    {
        m_deps.poster->Post (std::move (result));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunCheck
//
//  The check worker. Fetches the latest release record, classifies this
//  copy's install type (the signature check is too slow for the UI
//  thread), and reports whether the release is news. An automatic check in
//  an instance without the check lock ends without a result.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunCheck (UpdateCheckTrigger trigger, ReleaseVersion running, std::string skippedVersion)
{
    HRESULT                        hr          = S_OK;
    HRESULT                        hrDetect    = S_OK;
    std::unique_ptr<UpdateResult>  result      = std::make_unique<UpdateResult>();
    HttpRequest                    request;
    HttpResponse                   response;
    std::string                    json;
    bool                           hasLock     = true;
    bool                           isAnswered  = false;



    result->kind    = UpdateResultKind::Check;
    result->trigger = trigger;

    if (trigger == UpdateCheckTrigger::Automatic)
    {
        hasLock = m_deps.host->TryAcquireCheckLock();
    }

    BAIL_OUT_IF (!hasLock, S_OK);

    request.host            = kpszApiHost;
    request.path            = kpszLatestPath;
    request.extraHeaders    = kpszApiHeaders;
    request.displayName     = "the latest release";
    request.cancelRequested = &m_stopping;

    hr = m_deps.http->Get (request, response, result->detail);

    result->failure = MapCheckResponse (hr, response.statusCode);
    isAnswered      = result->failure == UpdateFailure::None;
    CBR (isAnswered);

    json.assign (response.body.begin(), response.body.end());

    hr = ReleaseInfo::Parse (json, result->release);
    CHRF (hr, result->failure = UpdateFailure::BadData);

    hrDetect = InstallTypeDetector::Detect (*m_deps.environment, *m_deps.verifier, result->installType);

    if (FAILED (hrDetect))
    {
        result->installType = InstallType::Developer;
    }

    result->isNewer      = result->release.version > running;
    result->isOffered    = UpdateSchedule::ShouldOfferRelease (trigger, running, result->release.version, skippedVersion);
    result->checkedAtUtc = m_deps.clock ? m_deps.clock() : 0;

Error:
    m_isCheckBusy = false;

    if (hasLock)
    {
        Post (std::move (result));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::FetchText
//
//  One raw file as text. A status other than 200 means the file is not
//  there at that tag.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::FetchText (LPCWSTR host, const std::wstring & path, std::string & outText)
{
    HRESULT       hr       = S_OK;
    HttpRequest   request;
    HttpResponse  response;
    std::string   error;
    bool          isFound  = false;



    request.host            = host;
    request.path            = path;
    request.displayName     = "the release notes";
    request.cancelRequested = &m_stopping;

    hr = m_deps.http->Get (request, response, error);
    CHR (hr);

    isFound = response.statusCode == kStatusOk;
    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    outText.assign (response.body.begin(), response.body.end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::FetchNotes
//
//  The CHANGELOG sections are required: a tag whose CHANGELOG has no section
//  for its own version is not the document expected. The README highlights
//  are optional, since a patch release has none and a missing README costs
//  the dialog nothing it needs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::FetchNotes (const NotesJob & job, ReleaseNotes & outNotes)
{
    HRESULT      hr         = S_OK;
    HRESULT      hrReadme   = S_OK;
    std::string  changelog;
    std::string  readme;
    bool         isDated    = false;



    hr = FetchText (kpszRawHost, MakeNotesPath (job.release.tag, L"CHANGELOG.md"), changelog);
    CHR (hr);

    hr = ReleaseNotesExtractor::ExtractChanges (changelog, job.running, job.release.version, outNotes.changes);
    CHR (hr);

    // The new tag's CHANGELOG holds the running version's section too, which
    // dates the running build for the dialog's remark on its age.
    isDated = ReleaseNotesExtractor::TryGetReleaseDate (changelog, job.running, outNotes.runningReleaseDate);
    IGNORE_RETURN_VALUE (isDated, false);

    hrReadme = FetchText (kpszRawHost, MakeNotesPath (job.release.tag, L"README.md"), readme);

    if (SUCCEEDED (hrReadme))
    {
        hrReadme = ReleaseNotesExtractor::ExtractHighlights (readme, job.running, job.release.version, outNotes.highlights);
    }

    if (FAILED (hrReadme))
    {
        outNotes.highlights.clear();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunFetchNotes
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunFetchNotes (NotesJob job)
{
    HRESULT                        hr       = S_OK;
    std::unique_ptr<UpdateResult>  result   = std::make_unique<UpdateResult>();
    bool                           isCached = false;



    result->kind    = UpdateResultKind::Notes;
    result->release = job.release;

    {
        std::lock_guard<std::mutex>  lock (m_notesMutex);
        auto                         it   = m_notesCache.find (job.release.tag);

        isCached = it != m_notesCache.end();

        if (isCached)
        {
            result->notes = it->second;
        }
    }

    BAIL_OUT_IF (isCached, S_OK);

    hr = FetchNotes (job, result->notes);
    CHRF (hr, result->failure = UpdateFailure::BadData);

    {
        std::lock_guard<std::mutex>  lock (m_notesMutex);

        m_notesCache[job.release.tag] = result->notes;
    }

Error:
    m_isNotesBusy = false;
    Post (std::move (result));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::FindAsset
//
//  The download for this install type: the zip for the running processor
//  for a zip copy, the bundle for a packaged one. A developer build has no
//  asset to install.
//
////////////////////////////////////////////////////////////////////////////////

const ReleaseAsset * UpdateService::FindAsset (const ApplyJob & job)
{
    const ReleaseAsset  * asset = nullptr;



    switch (job.installType)
    {
        case InstallType::Zip:  asset = job.release.FindZipAsset (job.arch); break;
        case InstallType::Msix: asset = job.release.FindBundleAsset();       break;
        default:                                                             break;
    }

    return asset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::Download
//
//  The whole asset into memory, with progress and cancel, then checked
//  against the release's size and digest before anything uses it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::Download (const ReleaseAsset & asset, std::vector<Byte> & outBytes, UpdateResult & result)
{
    HRESULT       hr         = S_OK;
    HttpRequest   request;
    HttpResponse  response;
    bool          isSplit    = false;
    bool          isCanceled = false;
    bool          isFound    = false;



    m_bytesTotal = asset.sizeBytes;

    isSplit = TrySplitUrl (asset.downloadUrl, request.host, request.path);
    CBRF (isSplit, result.failure = UpdateFailure::BadData);

    request.displayName     = asset.name;
    request.progressBytes   = &m_bytesDone;
    request.cancelRequested = &m_cancel;

    hr         = m_deps.http->Get (request, response, result.detail);
    isCanceled = m_cancel;

    CBRF (!isCanceled, result.wasCanceled = true);
    CHRF (hr,          result.failure = UpdateFailure::Network);

    isFound = response.statusCode == kStatusOk;
    CBRF (isFound, result.failure = UpdateFailure::Network);

    hr = ZipUpdateInstaller::VerifyDownload (response.body, asset);
    CHRF (hr, result.failure = UpdateFailure::DigestMismatch);

    outBytes = std::move (response.body);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::GetInstallDir
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::GetInstallDir (std::wstring & outDir, std::wstring & outExePath)
{
    HRESULT  hr        = S_OK;
    size_t   separator = 0;



    hr = m_deps.environment->GetExecutablePath (outExePath);
    CHR (hr);

    separator = outExePath.find_last_of (L"\\/");
    CBREx (separator != std::wstring::npos, HRESULT_FROM_WIN32 (ERROR_BAD_PATHNAME));

    outDir = outExePath.substr (0, separator);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::InstallZip
//
//  Swaps the release's files in, then starts the new Casso with the old
//  process's id so it can remove the set-aside files once this one exits.
//  The owner closes this window when the result arrives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::InstallZip (
    const ApplyJob          & job,
    const ReleaseAsset      & asset,
    std::span<const Byte>     bytes,
    UpdateResult            & result)
{
    HRESULT             hr        = S_OK;
    UpdateFailure       failure   = UpdateFailure::None;
    std::wstring        installDir;
    std::wstring        exePath;
    ZipUpdateInstaller  installer (*m_deps.fileSystem, *m_deps.verifier);



    hr = GetInstallDir (installDir, exePath);
    CHRF (hr, result.failure = UpdateFailure::InstallFailed);

    hr = installer.Install (installDir, bytes, asset, job.release.version, failure);
    CHRF (hr, result.failure = (failure == UpdateFailure::None) ? UpdateFailure::InstallFailed : failure);

    hr = m_deps.host->LaunchProcess (exePath, MakeRelaunchArgs (m_deps.host->GetCurrentPid()));
    CHRF (hr, result.failure = UpdateFailure::InstallFailed);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StageBundle
//
//  Writes the verified bundle where the package deployer can read it. The
//  deploy waits for the UI thread to flush disks and settings first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StageBundle (const ReleaseAsset & asset, std::span<const Byte> bytes, UpdateResult & result)
{
    HRESULT       hr     = S_OK;
    std::wstring  folder;
    std::wstring  path;



    hr = m_deps.host->GetDownloadFolder (folder);
    CHRF (hr, result.failure = UpdateFailure::InstallFailed);

    hr = m_deps.fileSystem->CreateDirectoryTree (folder);
    CHRF (hr, result.failure = UpdateFailure::InstallFailed);

    path = folder + L"\\";
    path.append (asset.name.begin(), asset.name.end());

    hr = m_deps.fileSystem->WriteAllBytes (path, bytes);
    CHRF (hr, result.failure = UpdateFailure::InstallFailed);

    result.kind       = UpdateResultKind::ReadyToDeploy;
    result.bundlePath = path;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunApply
//
//  The apply worker: refuse while another Casso is open, find the asset,
//  download and verify it, then install a zip copy or stage a bundle. A
//  cancel during the download ends the work with nothing changed.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunApply (ApplyJob job)
{
    HRESULT                        hr          = S_OK;
    std::unique_ptr<UpdateResult>  result      = std::make_unique<UpdateResult>();
    const ReleaseAsset           * asset       = nullptr;
    std::vector<Byte>              bytes;
    bool                           isOtherOpen = false;
    bool                           isCanceled  = false;



    result->kind        = UpdateResultKind::Applied;
    result->release     = job.release;
    result->installType = job.installType;

    isOtherOpen = m_deps.host->IsOtherInstanceRunning();
    CBRF (!isOtherOpen, result->failure = UpdateFailure::OtherInstanceRunning);

    CBRF (job.installType == InstallType::Zip || job.installType == InstallType::Msix,
          result->failure = UpdateFailure::NotOfficial);

    asset = FindAsset (job);
    CBRF (asset != nullptr, result->failure = UpdateFailure::NoAsset);

    hr = Download (*asset, bytes, *result);
    CHR (hr);

    isCanceled = m_cancel;
    CBRF (!isCanceled, result->wasCanceled = true);

    if (job.installType == InstallType::Zip)
    {
        hr = InstallZip (job, *asset, bytes, *result);
        CHR (hr);
    }
    else
    {
        hr = StageBundle (*asset, bytes, *result);
        CHR (hr);
    }

Error:
    m_isApplyBusy = false;
    Post (std::move (result));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunDeploy
//
//  On success Windows ends this process and restarts the new version, so
//  the result posted after a successful deploy may never be read.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunDeploy (std::wstring bundlePath)
{
    HRESULT                        hr     = S_OK;
    std::unique_ptr<UpdateResult>  result = std::make_unique<UpdateResult>();



    result->kind        = UpdateResultKind::Applied;
    result->installType = InstallType::Msix;
    result->bundlePath  = bundlePath;

    hr = m_deps.deployer->DeployBundle (bundlePath);
    CHRF (hr, result->failure = UpdateFailure::InstallFailed);

Error:
    m_isApplyBusy = false;
    Post (std::move (result));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunCleanup
//
//  The old executable is one of the set-aside files and cannot be deleted
//  while it runs, so the wait comes first. A wait that times out still
//  tries; whatever is still locked stays for the next update to clear.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunCleanup (DWORD oldProcessId)
{
    HRESULT       hr         = S_OK;
    HRESULT       hrWait     = S_OK;
    std::wstring  installDir;
    std::wstring  exePath;



    hrWait = m_deps.host->WaitForProcessExit (oldProcessId, kCleanupWaitMs);
    IGNORE_RETURN_VALUE (hrWait, S_OK);

    hr = GetInstallDir (installDir, exePath);
    CHR (hr);

    hr = ZipUpdateInstaller::RemoveOldFiles (*m_deps.fileSystem, installDir);
    CHR (hr);

Error:
    m_isCleanupBusy = false;
}
