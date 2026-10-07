#include "Pch.h"

#include "Update/Sha256Digest.h"
#include "Update/UpdateService.h"
#include "FakeInstallEnvironment.h"
#include "FakePackageDeployer.h"
#include "FakeSignatureVerifier.h"
#include "FakeUpdateHost.h"
#include "MockHttpClient.h"
#include "MockUpdateFileSystem.h"
#include "RecordingResultPoster.h"
#include "TestZipBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateServiceTests
//
//  The check, notes, apply, deploy and cleanup workers over mocks: no
//  network, no files, no processes. Each test starts work, joins the
//  workers with Wait, then reads what was posted.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UpdateServiceTests)
{
public:

    static constexpr std::int64_t  kNow          = 1790000000;
    static constexpr LPCWSTR       kpszGitHub    = L"github.com";
    static constexpr LPCWSTR       kpszZipPath   = L"/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0-x64.zip";
    static constexpr LPCWSTR       kpszBndlPath  = L"/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0.msixbundle";



    //
    //  Everything a service needs, with defaults for an official zip copy of
    //  1.30.0 installed in C:\Apps\Casso.
    //
    struct Rig
    {
        MockHttpClient                  http;
        FakeSignatureVerifier           verifier;
        FakeInstallEnvironment          environment;
        MockUpdateFileSystem            fileSystem;
        FakePackageDeployer             deployer;
        FakeUpdateHost                  host;
        RecordingResultPoster           poster;
        std::unique_ptr<UpdateService>  service;

        Rig()
        {
            UpdateServiceDeps  deps;

            verifier.isOfficial = true;
            environment.exePath = L"C:\\Apps\\Casso\\Casso.exe";

            deps.http        = &http;
            deps.verifier    = &verifier;
            deps.environment = &environment;
            deps.fileSystem  = &fileSystem;
            deps.deployer    = &deployer;
            deps.host        = &host;
            deps.poster      = &poster;
            deps.clock       = [] () { return kNow; };

            service = std::make_unique<UpdateService> (deps);
        }

        UpdateResult & Only()
        {
            Assert::AreEqual ((size_t) 1, poster.results.size(), L"exactly one result posted");
            return *poster.results.front();
        }
    };



    static std::string ToHex (const std::vector<Byte> & bytes)
    {
        std::string  hex;

        for (Byte value : bytes)
        {
            hex += std::format ("{:02x}", value);
        }

        return hex;
    }



    //
    //  A release record for 1.31.0 whose zip and bundle assets carry the
    //  digests of the given bytes.
    //
    static std::string MakeReleaseJson (const std::vector<Byte> & zip, const std::vector<Byte> & bundle)
    {
        std::vector<Byte>  zipDigest;
        std::vector<Byte>  bundleDigest;
        HRESULT            hr = S_OK;

        hr = Sha256Digest::Compute (zip, zipDigest);
        AssertSucceeded (hr);

        hr = Sha256Digest::Compute (bundle, bundleDigest);
        AssertSucceeded (hr);

        return std::format (R"({{
  "tag_name": "v1.31.0",
  "html_url": "https://github.com/relmer/Casso/releases/tag/v1.31.0",
  "prerelease": false,
  "published_at": "2026-10-20T18:04:11Z",
  "assets": [
    {{ "name": "Casso-1.31.0-x64.zip", "size": {},
      "browser_download_url": "https://github.com/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0-x64.zip",
      "digest": "sha256:{}" }},
    {{ "name": "Casso-1.31.0.msixbundle", "size": {},
      "browser_download_url": "https://github.com/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0.msixbundle",
      "digest": "sha256:{}" }}
  ]
}})", zip.size(), ToHex (zipDigest), bundle.size(), ToHex (bundleDigest));
    }



    static std::vector<Byte> MakeReleaseZip()
    {
        TestZipBuilder  builder;

        builder.Add ("Casso-x64/Casso.exe", "new exe");
        builder.Add ("Casso-x64/README.md", "new readme");

        return builder.Build();
    }



    static ReleaseInfo ParseRelease (const std::string & json)
    {
        ReleaseInfo  info;
        HRESULT      hr = ReleaseInfo::Parse (json, info);

        AssertSucceeded (hr);
        return info;
    }



    static void SetLatest (Rig & rig, const std::string & json)
    {
        rig.http.SetText (UpdateService::kpszApiHost, UpdateService::kpszLatestPath, MockHttpClient::kStatusOk, json);
    }



    //  the check

    TEST_METHOD (Check_NewerRelease_IsOfferedAndRecorded)
    {
        Rig  rig;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));

        AssertSucceeded (rig.service->StartCheck (UpdateCheckTrigger::Automatic, { 1, 30, 0 }, ""));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().kind == UpdateResultKind::Check);
        Assert::IsTrue   (rig.Only().failure == UpdateFailure::None);
        Assert::IsTrue   (rig.Only().release.version == ReleaseVersion { 1, 31, 0 });
        Assert::IsTrue   (rig.Only().isNewer);
        Assert::IsTrue   (rig.Only().isOffered);
        Assert::IsTrue   (rig.Only().installType == InstallType::Zip, L"the worker classifies the copy");
        Assert::AreEqual (kNow, rig.Only().checkedAtUtc);
        Assert::IsTrue   (rig.http.requests.front().extraHeaders.find (L"application/vnd.github+json") != std::wstring::npos);
    }



    TEST_METHOD (Check_CurrentRelease_IsNotOffered)
    {
        Rig  rig;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));

        AssertSucceeded (rig.service->StartCheck (UpdateCheckTrigger::Manual, { 1, 31, 0 }, ""));
        rig.service->Wait();

        Assert::IsFalse (rig.Only().isNewer);
        Assert::IsFalse (rig.Only().isOffered);
    }



    TEST_METHOD (Check_SkippedRelease_OfferedOnlyToManual)
    {
        Rig  automatic;
        Rig  manual;



        SetLatest (automatic, MakeReleaseJson ({ 1 }, { 2 }));
        SetLatest (manual,    MakeReleaseJson ({ 1 }, { 2 }));

        AssertSucceeded (automatic.service->StartCheck (UpdateCheckTrigger::Automatic, { 1, 30, 0 }, "1.31.0"));
        AssertSucceeded (manual.service->StartCheck    (UpdateCheckTrigger::Manual,    { 1, 30, 0 }, "1.31.0"));
        automatic.service->Wait();
        manual.service->Wait();

        Assert::IsFalse (automatic.Only().isOffered, L"the automatic check respects the skip");
        Assert::IsTrue  (manual.Only().isOffered,    L"a manual check offers it anyway");
    }



    TEST_METHOD (Check_AutomaticWithoutLock_PostsNothingAndFetchesNothing)
    {
        Rig  rig;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));
        rig.host.hasCheckLock = false;

        AssertSucceeded (rig.service->StartCheck (UpdateCheckTrigger::Automatic, { 1, 30, 0 }, ""));
        rig.service->Wait();

        Assert::AreEqual ((size_t) 0, rig.poster.results.size());
        Assert::AreEqual ((size_t) 0, rig.http.GetRequestCount(), L"another instance runs the check");
    }



    TEST_METHOD (Check_ManualIgnoresTheLock)
    {
        Rig  rig;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));
        rig.host.hasCheckLock = false;

        AssertSucceeded (rig.service->StartCheck (UpdateCheckTrigger::Manual, { 1, 30, 0 }, ""));
        rig.service->Wait();

        Assert::AreEqual (0, rig.host.lockRequests);
        Assert::IsTrue   (rig.Only().failure == UpdateFailure::None);
    }



    TEST_METHOD (Check_Failures_MapToTheirCause)
    {
        Rig  offline;
        Rig  limited;
        Rig  broken;
        Rig  garbled;
        Rig  prerelease;



        offline.http.SetFailure (UpdateService::kpszApiHost, UpdateService::kpszLatestPath, HRESULT_FROM_WIN32 (ERROR_NETWORK_UNREACHABLE));
        limited.http.SetText (UpdateService::kpszApiHost, UpdateService::kpszLatestPath, UpdateService::kStatusForbidden, "{}");
        broken.http.SetText  (UpdateService::kpszApiHost, UpdateService::kpszLatestPath, 500, "{}");
        SetLatest (garbled, "not json");
        SetLatest (prerelease, R"({ "tag_name": "v1.32.0", "prerelease": true })");

        for (Rig * rig : { &offline, &limited, &broken, &garbled, &prerelease })
        {
            AssertSucceeded (rig->service->StartCheck (UpdateCheckTrigger::Manual, { 1, 30, 0 }, ""));
            rig->service->Wait();
        }

        Assert::IsTrue (offline.Only().failure    == UpdateFailure::Network);
        Assert::IsTrue (limited.Only().failure    == UpdateFailure::RateLimited);
        Assert::IsTrue (broken.Only().failure     == UpdateFailure::BadData);
        Assert::IsTrue (garbled.Only().failure    == UpdateFailure::BadData);
        Assert::IsTrue (prerelease.Only().failure == UpdateFailure::BadData, L"a prerelease is never offered");
        Assert::IsFalse (offline.Only().isOffered);
    }



    TEST_METHOD (Check_UnsignedCopy_IsADeveloperBuild)
    {
        Rig  rig;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));
        rig.verifier.isOfficial = false;

        AssertSucceeded (rig.service->StartCheck (UpdateCheckTrigger::Automatic, { 1, 30, 0 }, ""));
        rig.service->Wait();

        Assert::IsTrue (rig.Only().installType == InstallType::Developer);
    }



    TEST_METHOD (Check_SecondStartWhileRunning_IsRefused)
    {
        Rig                rig;
        HRESULT            second  = S_OK;
        std::atomic<bool>  release = false;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));
        rig.http.onGet = [&release] (const HttpRequest &)
        {
            while (!release)
            {
                std::this_thread::yield();
            }
        };

        AssertSucceeded (rig.service->StartCheck (UpdateCheckTrigger::Automatic, { 1, 30, 0 }, ""));
        second  = rig.service->StartCheck (UpdateCheckTrigger::Manual, { 1, 30, 0 }, "");
        release = true;
        rig.service->Wait();

        Assert::AreEqual (E_PENDING, second);
    }



    //  the notes

    TEST_METHOD (Notes_ReadFromTheReleaseTagAndCached)
    {
        Rig          rig;
        ReleaseInfo  release = ParseRelease (MakeReleaseJson ({ 1 }, { 2 }));



        rig.http.SetText (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.0/CHANGELOG.md", 200,
                          "## [1.31.0] - 2026-10-20: Updates\n\n- Update notification\n\n## [1.30.0] - 2026-10-03: Old\n\n- Old\n");
        rig.http.SetText (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.0/README.md", 200,
                          "### [2026-10-20 \xC2\xB7 1.31] Updates\n\nCasso can update itself.\n");

        AssertSucceeded (rig.service->StartFetchNotes (release, { 1, 30, 0 }));
        rig.service->Wait();
        AssertSucceeded (rig.service->StartFetchNotes (release, { 1, 30, 0 }));
        rig.service->Wait();

        Assert::AreEqual ((size_t) 2, rig.poster.results.size());
        Assert::AreEqual ((size_t) 2, rig.http.GetRequestCount(), L"the second open is answered from the cache");

        for (const std::unique_ptr<UpdateResult> & result : rig.poster.results)
        {
            Assert::IsTrue   (result->kind == UpdateResultKind::Notes);
            Assert::IsTrue   (result->failure == UpdateFailure::None);
            Assert::AreEqual ((size_t) 1, result->notes.changes.size(), L"only the sections after the running build");
            Assert::AreEqual ((size_t) 1, result->notes.highlights.size());
        }
    }



    TEST_METHOD (Notes_ChangelogWithoutTheRelease_IsAFailure)
    {
        Rig          rig;
        ReleaseInfo  release = ParseRelease (MakeReleaseJson ({ 1 }, { 2 }));



        rig.http.SetText (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.0/CHANGELOG.md", 200, "## [1.30.0] - x\n\n- Old\n");

        AssertSucceeded (rig.service->StartFetchNotes (release, { 1, 30, 0 }));
        rig.service->Wait();

        Assert::IsTrue (rig.Only().failure == UpdateFailure::BadData);
    }



    TEST_METHOD (Notes_MissingReadme_StillShowsTheChanges)
    {
        Rig          rig;
        ReleaseInfo  release = ParseRelease (MakeReleaseJson ({ 1 }, { 2 }));



        rig.http.SetText (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.0/CHANGELOG.md", 200, "## [1.31.0] - x\n\n- New\n");

        AssertSucceeded (rig.service->StartFetchNotes (release, { 1, 30, 9 }));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().failure == UpdateFailure::None);
        Assert::AreEqual ((size_t) 1, rig.Only().notes.changes.size());
        Assert::AreEqual ((size_t) 0, rig.Only().notes.highlights.size());
    }



    //  the zip update

    TEST_METHOD (Apply_Zip_SwapsFilesAndRelaunches)
    {
        Rig                rig;
        std::vector<Byte>  zip     = MakeReleaseZip();
        ReleaseInfo        release = ParseRelease (MakeReleaseJson (zip, { 2 }));



        rig.fileSystem.Put (L"C:\\Apps\\Casso\\Casso.exe", "old exe");
        rig.http.SetBytes (kpszGitHub, kpszZipPath, zip);

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().kind == UpdateResultKind::Applied);
        Assert::IsTrue   (rig.Only().failure == UpdateFailure::None);
        Assert::AreEqual (std::string ("new exe"), rig.fileSystem.Get (L"C:\\Apps\\Casso\\Casso.exe"));
        Assert::AreEqual (1, rig.host.launches);
        Assert::AreEqual (std::wstring (L"C:\\Apps\\Casso\\Casso.exe"), rig.host.launchedExe);
        Assert::AreEqual (std::wstring (L"--updated --cleanup-old 4242"), rig.host.launchedArgs);
    }



    TEST_METHOD (Apply_OtherInstanceOpen_DownloadsNothing)
    {
        Rig                rig;
        std::vector<Byte>  zip     = MakeReleaseZip();
        ReleaseInfo        release = ParseRelease (MakeReleaseJson (zip, { 2 }));



        rig.fileSystem.Put (L"C:\\Apps\\Casso\\Casso.exe", "old exe");
        rig.http.SetBytes (kpszGitHub, kpszZipPath, zip);
        rig.host.isOtherInstanceOpen = true;

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().failure == UpdateFailure::OtherInstanceRunning);
        Assert::AreEqual ((size_t) 0, rig.http.GetRequestCount());
        Assert::AreEqual (std::string ("old exe"), rig.fileSystem.Get (L"C:\\Apps\\Casso\\Casso.exe"));
    }



    TEST_METHOD (Apply_CorruptDownload_ChangesNothing)
    {
        Rig                rig;
        std::vector<Byte>  zip     = MakeReleaseZip();
        ReleaseInfo        release = ParseRelease (MakeReleaseJson (zip, { 2 }));
        std::vector<Byte>  served  = zip;



        served.back() ^= 0xFF;
        rig.fileSystem.Put (L"C:\\Apps\\Casso\\Casso.exe", "old exe");
        rig.http.SetBytes (kpszGitHub, kpszZipPath, served);

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().failure == UpdateFailure::DigestMismatch);
        Assert::AreEqual (0, rig.fileSystem.operations, L"no file operation ran");
        Assert::AreEqual (0, rig.host.launches);
    }



    TEST_METHOD (Apply_DownloadFails_IsANetworkFailure)
    {
        Rig          rig;
        ReleaseInfo  release = ParseRelease (MakeReleaseJson ({ 1 }, { 2 }));



        rig.http.SetFailure (kpszGitHub, kpszZipPath, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().failure == UpdateFailure::Network);
        Assert::AreEqual (0, rig.fileSystem.operations);
    }



    TEST_METHOD (Apply_CanceledDownload_ChangesNothing)
    {
        Rig                rig;
        std::vector<Byte>  zip     = MakeReleaseZip();
        ReleaseInfo        release = ParseRelease (MakeReleaseJson (zip, { 2 }));
        UpdateService    * service = rig.service.get();



        rig.http.SetBytes (kpszGitHub, kpszZipPath, zip);
        rig.http.onGet = [service] (const HttpRequest &) { service->CancelApply(); };

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().wasCanceled);
        Assert::IsTrue   (rig.Only().failure == UpdateFailure::None, L"a cancel is not a failure to report");
        Assert::AreEqual (0, rig.fileSystem.operations);
    }



    TEST_METHOD (Apply_DeveloperBuild_IsNotOfficial)
    {
        Rig          rig;
        ReleaseInfo  release = ParseRelease (MakeReleaseJson ({ 1 }, { 2 }));



        AssertSucceeded (rig.service->StartApply (release, InstallType::Developer, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().failure == UpdateFailure::NotOfficial);
        Assert::AreEqual ((size_t) 0, rig.http.GetRequestCount());
    }



    TEST_METHOD (Apply_NoAssetForThisCopy_IsNoAsset)
    {
        Rig          rig;
        ReleaseInfo  release = ParseRelease (MakeReleaseJson ({ 1 }, { 2 }));



        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::Arm64));
        rig.service->Wait();

        Assert::IsTrue (rig.Only().failure == UpdateFailure::NoAsset);
    }



    TEST_METHOD (Apply_ZipWhenClosed_StagesWithoutSwappingOrRelaunching)
    {
        Rig                rig;
        std::vector<Byte>  zip     = MakeReleaseZip();
        ReleaseInfo        release = ParseRelease (MakeReleaseJson (zip, { 2 }));



        rig.fileSystem.Put (L"C:\\Apps\\Casso\\Casso.exe", "old exe");
        rig.http.SetBytes (kpszGitHub, kpszZipPath, zip);

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64, DeployTiming::WhenClosed));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().failure == UpdateFailure::None);
        Assert::IsTrue   (rig.Only().isPending, L"waiting for Casso to close");
        Assert::AreEqual (std::wstring (L"C:\\Apps\\Casso"), rig.Only().installDir);
        Assert::IsFalse  (rig.Only().stagedPaths.empty());
        Assert::AreEqual (std::string ("old exe"), rig.fileSystem.Get (L"C:\\Apps\\Casso\\Casso.exe"), L"nothing swapped yet");
        Assert::AreEqual (std::string ("new exe"), rig.fileSystem.Get (L"C:\\Apps\\Casso\\.update-new\\Casso.exe"), L"checked and staged");
        Assert::AreEqual (0, rig.host.launches);
    }



    TEST_METHOD (CommitPending_SwapsAndRelaunchesWithTheRepeatedOptions)
    {
        Rig                rig;
        std::vector<Byte>  zip     = MakeReleaseZip();
        ReleaseInfo        release = ParseRelease (MakeReleaseJson (zip, { 2 }));
        UpdateResult       pending;



        rig.fileSystem.Put (L"C:\\Apps\\Casso\\Casso.exe", "old exe");
        rig.http.SetBytes (kpszGitHub, kpszZipPath, zip);
        rig.service->SetRelaunchArguments ({ L"--title", L"my box" });

        AssertSucceeded (rig.service->StartApply (release, InstallType::Zip, ReleaseArch::X64, DeployTiming::WhenClosed));
        rig.service->Wait();

        pending = *rig.poster.results.back();

        AssertSucceeded (rig.service->StartCommitPending (pending.installDir, pending.stagedPaths));
        rig.service->Wait();

        Assert::IsTrue   (rig.poster.results.back()->failure == UpdateFailure::None);
        Assert::IsFalse  (rig.poster.results.back()->isPending);
        Assert::AreEqual (std::string ("new exe"), rig.fileSystem.Get (L"C:\\Apps\\Casso\\Casso.exe"));
        Assert::AreEqual (std::wstring (L"--updated --cleanup-old 4242 --title \"my box\""), rig.host.launchedArgs);
    }



    //  the MSIX update

    TEST_METHOD (Apply_Msix_StagesTheBundleThenDeploysOnRequest)
    {
        Rig                rig;
        std::vector<Byte>  bundle  = { 'b', 'u', 'n', 'd', 'l', 'e' };
        ReleaseInfo        release = ParseRelease (MakeReleaseJson ({ 1 }, bundle));
        std::wstring       staged  = L"C:\\Users\\me\\AppData\\Local\\Casso\\Updates\\Casso-1.31.0.msixbundle";



        rig.http.SetBytes (kpszGitHub, kpszBndlPath, bundle);

        AssertSucceeded (rig.service->StartApply (release, InstallType::Msix, ReleaseArch::X64));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().kind == UpdateResultKind::ReadyToDeploy, L"the deploy waits for the UI thread's flush");
        Assert::AreEqual (staged, rig.Only().bundlePath);
        Assert::AreEqual (std::string ("bundle"), rig.fileSystem.Get (staged));
        Assert::AreEqual (0, rig.deployer.deploys);

        AssertSucceeded (rig.service->StartDeploy (staged));
        rig.service->Wait();

        Assert::AreEqual (1, rig.deployer.deploys);
        Assert::AreEqual (staged, rig.deployer.deployedPath);
        Assert::IsTrue   (rig.poster.results.back()->kind == UpdateResultKind::Applied);
        Assert::IsTrue   (rig.poster.results.back()->failure == UpdateFailure::None);
    }



    TEST_METHOD (Apply_MsixWhenClosed_DeploysDeferredWithoutARestart)
    {
        Rig                rig;
        std::vector<Byte>  bundle  = { 'b', 'u', 'n', 'd', 'l', 'e' };
        ReleaseInfo        release = ParseRelease (MakeReleaseJson ({ 1 }, bundle));



        rig.http.SetBytes (kpszGitHub, kpszBndlPath, bundle);

        AssertSucceeded (rig.service->StartApply (release, InstallType::Msix, ReleaseArch::X64, DeployTiming::WhenClosed));
        rig.service->Wait();

        Assert::IsTrue   (rig.Only().kind == UpdateResultKind::Applied);
        Assert::IsTrue   (rig.Only().isPending);
        Assert::AreEqual (1, rig.deployer.deploys);
        Assert::IsTrue   (rig.deployer.timing == DeployTiming::WhenClosed, L"Windows applies it once Casso exits");
        Assert::AreEqual (std::wstring(), rig.deployer.restartArgs, L"and nothing restarts Casso");
    }



    TEST_METHOD (Deploy_Now_RestartsWithTheRepeatedOptions)
    {
        Rig  rig;



        rig.service->SetRelaunchArguments ({ L"--trace", L"50M", L"--no-image-watch" });

        AssertSucceeded (rig.service->StartDeploy (L"C:\\x.msixbundle"));
        rig.service->Wait();

        Assert::IsTrue   (rig.deployer.timing == DeployTiming::Now);
        Assert::AreEqual (std::wstring (L"--updated --trace 50M --no-image-watch"), rig.deployer.restartArgs);
    }



    TEST_METHOD (Deploy_Failure_IsReported)
    {
        Rig  rig;



        rig.deployer.result = HRESULT_FROM_WIN32 (ERROR_INSTALL_FAILED);

        AssertSucceeded (rig.service->StartDeploy (L"C:\\x.msixbundle"));
        rig.service->Wait();

        Assert::IsTrue (rig.Only().failure == UpdateFailure::InstallFailed);
    }



    //  after a zip update

    TEST_METHOD (Cleanup_WaitsForTheOldProcessThenRemovesOldFiles)
    {
        Rig  rig;



        rig.fileSystem.Put (L"C:\\Apps\\Casso\\.update-old\\Casso.exe", "old exe");

        AssertSucceeded (rig.service->StartCleanup (777));
        rig.service->Wait();

        Assert::AreEqual ((DWORD) 777, rig.host.waitedPid);
        Assert::IsFalse  (rig.fileSystem.Exists (L"C:\\Apps\\Casso\\.update-old\\Casso.exe"));
        Assert::AreEqual ((size_t) 0, rig.poster.results.size());
    }



    //  shutdown

    TEST_METHOD (Stop_PostsNothingAndRefusesNewWork)
    {
        Rig  rig;



        SetLatest (rig, MakeReleaseJson ({ 1 }, { 2 }));
        rig.service->Stop();

        Assert::AreEqual (E_ABORT, rig.service->StartCheck (UpdateCheckTrigger::Manual, { 1, 30, 0 }, ""));
        Assert::AreEqual ((size_t) 0, rig.poster.results.size());
    }



    //  pure helpers

    TEST_METHOD (MapCheckResponse_EveryStatus)
    {
        Assert::IsTrue (UpdateService::MapCheckResponse (E_FAIL, 0)  == UpdateFailure::Network);
        Assert::IsTrue (UpdateService::MapCheckResponse (S_OK, 200)  == UpdateFailure::None);
        Assert::IsTrue (UpdateService::MapCheckResponse (S_OK, 403)  == UpdateFailure::RateLimited);
        Assert::IsTrue (UpdateService::MapCheckResponse (S_OK, 429)  == UpdateFailure::RateLimited);
        Assert::IsTrue (UpdateService::MapCheckResponse (S_OK, 404)  == UpdateFailure::BadData);
    }



    TEST_METHOD (TrySplitUrl_HttpsOnly)
    {
        std::wstring  host;
        std::wstring  path;



        Assert::IsTrue   (UpdateService::TrySplitUrl (L"https://github.com/a/b.zip", host, path));
        Assert::AreEqual (std::wstring (L"github.com"), host);
        Assert::AreEqual (std::wstring (L"/a/b.zip"),   path);

        Assert::IsFalse (UpdateService::TrySplitUrl (L"http://github.com/a", host, path));
        Assert::IsFalse (UpdateService::TrySplitUrl (L"https://github.com",  host, path));
        Assert::IsFalse (UpdateService::TrySplitUrl (L"https:///a",          host, path));
    }



    TEST_METHOD (MakeNotesPath_UsesTheTag)
    {
        Assert::AreEqual (std::wstring (L"/relmer/Casso/v1.31.0/README.md"),
                          UpdateService::MakeNotesPath ("v1.31.0", L"README.md"));
    }
};
