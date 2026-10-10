#include "Pch.h"

#include "Update/UpdateService.h"
#include "Update/ZipUpdateInstaller.h"
#include "Devices/Printer/PngCodec.h"
#include "Core/TextEncoding.h"
#include "Devices/Printer/RgbaImage.h"





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

HRESULT UpdateService::StartApply (const ReleaseInfo & release, InstallType installType, ReleaseArch arch, DeployTiming timing)
{
    HRESULT   hr  = S_OK;
    ApplyJob  job;



    ReapIfIdle (m_applyThread, m_isApplyBusy);

    BAIL_OUT_IF (m_stopping,    E_ABORT);
    BAIL_OUT_IF (m_isApplyBusy, E_PENDING);

    job.release     = release;
    job.installType = installType;
    job.arch        = arch;
    job.timing      = timing;

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
//  UpdateService::StartCommitPending
//
//  Update now, for an update already staged to apply when Casso closes:
//  swaps the staged files in and relaunches, as a zip update does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartCommitPending (const std::wstring & installDir, const std::vector<std::string> & paths)
{
    HRESULT  hr = S_OK;



    ReapIfIdle (m_applyThread, m_isApplyBusy);

    BAIL_OUT_IF (m_stopping,    E_ABORT);
    BAIL_OUT_IF (m_isApplyBusy, E_PENDING);

    m_isApplyBusy = true;
    m_applyThread = std::thread ([this, installDir, paths]
    {
        RunCommitPending (installDir, paths);
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
    for (std::thread * worker : { &m_checkThread, &m_notesThread, &m_applyThread, &m_cleanupThread, &m_imagesThread })
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
    m_stopping     = true;
    m_cancel       = true;
    m_imagesCancel = true;

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



    path += TextEncoding::Utf8ToWide (tag);
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
//  What a finished zip update starts the new Casso with: the update's own
//  flags, then the options this process was started with that a relaunch
//  repeats.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateService::MakeRelaunchArgs (DWORD oldProcessId, const std::vector<std::wstring> & repeated)
{
    std::wstring  args = std::format (L"{} {} {}", kpszUpdatedArg, kpszCleanupOldArg, oldProcessId);



    for (const std::wstring & argument : repeated)
    {
        args += L" " + QuoteArgument (argument);
    }

    return args;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::MakeRestartArgs
//
//  What Windows restarts a packaged Casso with after an MSIX update: the
//  same as a zip relaunch, without an old process to clean up after.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateService::MakeRestartArgs (const std::vector<std::wstring> & repeated)
{
    std::wstring  args = kpszUpdatedArg;



    for (const std::wstring & argument : repeated)
    {
        args += L" " + QuoteArgument (argument);
    }

    return args;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::QuoteArgument
//
//  One argument as CommandLineToArgvW reads it back: bare when it has no
//  space, tab or quote, otherwise quoted, with each quote escaped and the
//  backslashes before a quote (or before the closing quote) doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring UpdateService::QuoteArgument (const std::wstring & argument)
{
    std::wstring  quoted  = L"\"";
    size_t        slashes = 0;
    bool          isBare  = !argument.empty() && argument.find_first_of (L" \t\"") == std::wstring::npos;



    for (wchar_t ch : argument)
    {
        if (ch == L'\\')
        {
            slashes++;
            continue;
        }

        quoted.append (ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        quoted += ch;
        slashes = 0;
    }

    quoted.append (slashes * 2, L'\\');
    quoted += L"\"";

    return isBare ? argument : quoted;
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
//  an instance without the check lock fetches nothing and posts a
//  CheckSkipped result, so the shell can wait for the holder's record.
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

    if (!hasLock)
    {
        OutputDebugStringW (L"Casso: automatic update check skipped: another Casso holds the check lock\n");

        result->kind = UpdateResultKind::CheckSkipped;
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

    Post (std::move (result));
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
//  The owner closes this window when the result arrives. Applied when Casso
//  closes, only the checks and the staging run now; the result carries the
//  staged files for the swap at exit.
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

    if (job.timing == DeployTiming::WhenClosed)
    {
        hr = installer.Prepare (installDir, bytes, asset, job.release.version, result.stagedPaths, failure);
        CHRF (hr, result.failure = (failure == UpdateFailure::None) ? UpdateFailure::InstallFailed : failure);

        result.isPending  = true;
        result.installDir = installDir;
    }
    else
    {
        hr = installer.Install (installDir, bytes, asset, job.release.version, failure);
        CHRF (hr, result.failure = (failure == UpdateFailure::None) ? UpdateFailure::InstallFailed : failure);

        hr = m_deps.host->LaunchProcess (exePath, MakeRelaunchArgs (m_deps.host->GetCurrentPid(), m_relaunchArgs));
        CHRF (hr, result.failure = UpdateFailure::InstallFailed);
    }

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
    path += TextEncoding::Utf8ToWide (asset.name);

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

        // When closed, Windows takes the bundle now and registers it once
        // Casso has exited; nothing here needs the UI thread's flush first.
        if (job.timing == DeployTiming::WhenClosed)
        {
            hr = m_deps.deployer->DeployBundle (result->bundlePath, DeployTiming::WhenClosed, L"");
            CHRF (hr, result->failure = UpdateFailure::InstallFailed);

            result->kind      = UpdateResultKind::Applied;
            result->isPending = true;
        }
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

    hr = m_deps.deployer->DeployBundle (bundlePath, DeployTiming::Now, MakeRestartArgs (m_relaunchArgs));
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





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunCommitPending
//
//  The swap a pending update was waiting for, then the relaunch.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunCommitPending (std::wstring installDir, std::vector<std::string> paths)
{
    HRESULT                        hr          = S_OK;
    std::unique_ptr<UpdateResult>  result      = std::make_unique<UpdateResult>();
    UpdateFailure                  failure     = UpdateFailure::None;
    std::wstring                   dir;
    std::wstring                   exePath;
    ZipUpdateInstaller             installer (*m_deps.fileSystem, *m_deps.verifier);
    bool                           isOtherOpen = false;



    result->kind        = UpdateResultKind::Applied;
    result->installType = InstallType::Zip;

    isOtherOpen = m_deps.host->IsOtherInstanceRunning();
    CBRF (!isOtherOpen, result->failure = UpdateFailure::OtherInstanceRunning);

    hr = GetInstallDir (dir, exePath);
    CHRF (hr, result->failure = UpdateFailure::InstallFailed);

    hr = installer.Commit (installDir, paths, failure);
    CHRF (hr, result->failure = (failure == UpdateFailure::None) ? UpdateFailure::InstallFailed : failure);

    hr = m_deps.host->LaunchProcess (exePath, MakeRelaunchArgs (m_deps.host->GetCurrentPid(), m_relaunchArgs));
    CHRF (hr, result->failure = UpdateFailure::InstallFailed);

Error:
    m_isApplyBusy = false;
    Post (std::move (result));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::CancelImages
//
//  Stops fetching release-notes images; the dialog that wanted them closed.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::CancelImages()
{
    m_imagesCancel = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::StartFetchImages
//
//  Starts fetching the images the notes show, one result posted per image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::StartFetchImages (const std::string & tag, const std::vector<std::string> & sources)
{
    HRESULT  hr = S_OK;



    ReapIfIdle (m_imagesThread, m_isImagesBusy);

    BAIL_OUT_IF (m_stopping,     E_ABORT);
    BAIL_OUT_IF (m_isImagesBusy, E_PENDING);

    m_imagesCancel = false;
    m_isImagesBusy = true;
    m_imagesThread = std::thread ([this, tag, sources]
    {
        RunFetchImages (tag, sources);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::TryResolveImageUrl
//
//  An https URL is fetched as written. A relative path resolves against the
//  release tag's raw files, the way GitHub renders the README. Anything else
//  -- plain http, data:, another scheme, a protocol-relative "//host" -- is
//  not fetched, and the dialog shows the alt text in its place.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateService::TryResolveImageUrl (const std::string & src, const std::string & tag, std::wstring & outUrl)
{
    std::string_view  path       = src;
    bool              isResolved = false;



    outUrl.clear();

    if (src.starts_with ("https://"))
    {
        outUrl     = TextEncoding::Utf8ToWide (src);
        isResolved = true;
    }
    else if (!src.empty() && src.find (':') == std::string::npos && !src.starts_with ("//") && !tag.empty())
    {
        while (path.starts_with ("./"))
        {
            path.remove_prefix (2);
        }

        while (path.starts_with ('/'))
        {
            path.remove_prefix (1);
        }

        outUrl  = L"https://";
        outUrl += kpszRawHost;
        outUrl += kpszRepoRawPath;
        outUrl += TextEncoding::Utf8ToWide (tag);
        outUrl += L"/";
        outUrl += TextEncoding::Utf8ToWide (std::string (path));
        isResolved = !path.empty();
    }

    return isResolved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::DecodeImage
//
//  Any format WIC reads (PNG, JPEG, GIF's first frame), as premultiplied
//  BGRA. The calling thread must have initialized COM.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::DecodeImage (std::span<const Byte> bytes, NotesImage & outImage)
{
    HRESULT            hr     = S_OK;
    RgbaImage          rgba;
    std::vector<Byte>  copy (bytes.begin(), bytes.end());
    size_t             i      = 0;
    size_t             count  = 0;
    const Byte       * px     = nullptr;



    hr = PngCodec::DecodeRgba (copy, rgba);
    CHR (hr);

    count = (size_t) rgba.width * (size_t) rgba.height;
    outImage.width  = rgba.width;
    outImage.height = rgba.height;
    outImage.bgraPremul.resize (count);

    for (i = 0; i < count; i++)
    {
        px = &rgba.rgba[i * 4];
        outImage.bgraPremul[i] = ((uint32_t) px[3] << 24)                          |
                                 ((uint32_t) (px[0] * px[3] / 255) << 16)          |
                                 ((uint32_t) (px[1] * px[3] / 255) << 8)           |
                                  (uint32_t) (px[2] * px[3] / 255);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::FetchImage
//
//  One image by URL, answered from the session cache when it was fetched
//  before. A file over kMaxImageBytes is refused undecoded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateService::FetchImage (const std::wstring & url, std::shared_ptr<const NotesImage> & outImage, UpdateResult & result)
{
    HRESULT                      hr       = S_OK;
    HttpRequest                  request;
    HttpResponse                 response;
    std::shared_ptr<NotesImage>  decoded  = std::make_shared<NotesImage>();
    bool                         isSplit  = false;
    bool                         isCached = false;
    bool                         isFound  = false;
    bool                         isSmall  = false;



    {
        std::lock_guard<std::mutex>  lock (m_notesMutex);
        auto                         it   = m_imageCache.find (url);

        isCached = it != m_imageCache.end();
        outImage = isCached ? it->second : nullptr;
    }

    BAIL_OUT_IF (isCached, S_OK);

    isSplit = TrySplitUrl (url, request.host, request.path);
    CBRF (isSplit, result.failure = UpdateFailure::BadData);

    request.displayName     = "a release-notes image";
    request.cancelRequested = &m_imagesCancel;

    hr = m_deps.http->Get (request, response, result.detail);
    CHRF (hr, result.failure = UpdateFailure::Network);

    isFound = response.statusCode == kStatusOk;
    CBRF (isFound, result.failure = UpdateFailure::Network);

    isSmall = response.body.size() <= kMaxImageBytes;
    CBRF (isSmall, result.failure = UpdateFailure::BadData);

    hr = DecodeImage (response.body, *decoded);
    CHRF (hr, result.failure = UpdateFailure::BadData);

    outImage = decoded;

    {
        std::lock_guard<std::mutex>  lock (m_notesMutex);

        m_imageCache[url] = outImage;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateService::RunFetchImages
//
//  The images worker. Each source gets a result, decoded or failed, so the
//  dialog can replace every placeholder with the image or its alt text.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateService::RunFetchImages (std::string tag, std::vector<std::string> sources)
{
    HRESULT       hrCom      = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    HRESULT       hrFetch    = S_OK;
    std::wstring  url;
    bool          isFetching = false;



    for (const std::string & src : sources)
    {
        std::unique_ptr<UpdateResult>  result = std::make_unique<UpdateResult>();

        if (m_imagesCancel || m_stopping)
        {
            break;
        }

        result->kind     = UpdateResultKind::Image;
        result->imageSrc = src;
        isFetching       = TryResolveImageUrl (src, tag, url);

        if (!isFetching)
        {
            result->failure = UpdateFailure::BadData;
        }
        else
        {
            hrFetch = FetchImage (url, result->image, *result);

            if (FAILED (hrFetch) && result->failure == UpdateFailure::None)
            {
                result->failure = UpdateFailure::Network;
            }
        }

        if (!m_imagesCancel)
        {
            Post (std::move (result));
        }
    }

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }

    m_isImagesBusy = false;
}
