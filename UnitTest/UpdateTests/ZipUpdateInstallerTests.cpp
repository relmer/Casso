#include "Pch.h"

#include "Update/Sha256Digest.h"
#include "Update/ZipUpdateInstaller.h"
#include "FakeSignatureVerifier.h"
#include "MockUpdateFileSystem.h"
#include "TestZipBuilder.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ZipUpdateInstallerTests
//
//  The replace-and-restore sequence over an in-memory install folder. The
//  central test fails each file operation in turn and checks that every
//  failure leaves the installed files exactly as they were.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ZipUpdateInstallerTests)
{
public:

    static constexpr LPCWSTR  kpszInstallDir = L"C:\\Apps\\Casso";



    //
    //  An install of 1.30.0 with a file of the user's own beside it.
    //
    static void MakeOldInstall (MockUpdateFileSystem & fs)
    {
        fs.Put (L"C:\\Apps\\Casso\\Casso.exe",      "old exe");
        fs.Put (L"C:\\Apps\\Casso\\README.md",      "old readme");
        fs.Put (L"C:\\Apps\\Casso\\Demos\\old.dsk", "old disk");
        fs.Put (L"C:\\Apps\\Casso\\Notes.txt",      "the user's own file");
    }



    //
    //  The 1.31.0 release zip as the workflow lays it out: one top folder.
    //
    static std::vector<Byte> MakeReleaseZip (bool includeExe = true)
    {
        TestZipBuilder  builder;

        builder.Add ("Casso-x64/", "");

        if (includeExe)
        {
            builder.Add ("Casso-x64/Casso.exe", "new exe");
        }

        builder.Add ("Casso-x64/README.md",       "new readme");
        builder.Add ("Casso-x64/Demos/",          "");
        builder.Add ("Casso-x64/Demos/new.dsk",   "new disk");

        return builder.Build();
    }



    static ReleaseAsset MakeAsset (const std::vector<Byte> & zip)
    {
        ReleaseAsset  asset;
        HRESULT       hr    = S_OK;

        asset.name      = "Casso-1.31.0-x64.zip";
        asset.sizeBytes = zip.size();

        hr = Sha256Digest::Compute (zip, asset.sha256);
        AssertSucceeded (hr);

        return asset;
    }



    TEST_METHOD (Success_SwapsFilesAndKeepsOldAside)
    {
        MockUpdateFileSystem   fs;
        FakeSignatureVerifier  verifier;
        ZipUpdateInstaller     installer (fs, verifier);
        std::vector<Byte>      zip       = MakeReleaseZip();
        UpdateFailure          failure   = UpdateFailure::BadData;
        HRESULT                hr        = S_OK;



        MakeOldInstall (fs);
        verifier.isOfficial = true;

        hr = installer.Install (kpszInstallDir, zip, MakeAsset (zip), { 1, 31, 0 }, failure);
        AssertSucceeded (hr);

        Assert::IsTrue   (failure == UpdateFailure::None);
        Assert::AreEqual (std::string ("new exe"),             fs.Get (L"C:\\Apps\\Casso\\Casso.exe"));
        Assert::AreEqual (std::string ("new readme"),          fs.Get (L"C:\\Apps\\Casso\\README.md"));
        Assert::AreEqual (std::string ("new disk"),            fs.Get (L"C:\\Apps\\Casso\\Demos\\new.dsk"));
        Assert::AreEqual (std::string ("old disk"),            fs.Get (L"C:\\Apps\\Casso\\Demos\\old.dsk"), L"files the release does not ship stay");
        Assert::AreEqual (std::string ("the user's own file"), fs.Get (L"C:\\Apps\\Casso\\Notes.txt"));
        Assert::AreEqual (std::string ("old exe"),             fs.Get (L"C:\\Apps\\Casso\\.update-old\\Casso.exe"));
        Assert::IsFalse  (fs.HasFilesUnder (L"C:\\Apps\\Casso\\.update-new"), L"staging removed");
        Assert::IsTrue   (verifier.lastPath.ends_with (L"\\.update-new\\Casso.exe"), L"the STAGED exe is what gets verified");

        hr = ZipUpdateInstaller::RemoveOldFiles (fs, kpszInstallDir);
        AssertSucceeded (hr);
        Assert::IsFalse (fs.HasFilesUnder (L"C:\\Apps\\Casso\\.update-old"));
    }



    TEST_METHOD (Digest_MismatchOrMissing_ChangesNothing)
    {
        MockUpdateFileSystem   fs;
        FakeSignatureVerifier  verifier;
        ZipUpdateInstaller     installer (fs, verifier);
        std::vector<Byte>      zip       = MakeReleaseZip();
        ReleaseAsset           asset     = MakeAsset (zip);
        UpdateFailure          failure   = UpdateFailure::None;
        HRESULT                hr        = S_OK;



        MakeOldInstall (fs);
        verifier.isOfficial = true;

        asset.sha256[0] ^= 1;
        hr = installer.Install (kpszInstallDir, zip, asset, { 1, 31, 0 }, failure);
        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (failure == UpdateFailure::DigestMismatch);

        asset.sha256.clear();
        hr = installer.Install (kpszInstallDir, zip, asset, { 1, 31, 0 }, failure);
        Assert::IsTrue   (FAILED (hr), L"no digest, no install");
        Assert::IsTrue   (failure == UpdateFailure::DigestMismatch);

        asset           = MakeAsset (zip);
        asset.sizeBytes = zip.size() + 1;
        hr = installer.Install (kpszInstallDir, zip, asset, { 1, 31, 0 }, failure);
        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (failure == UpdateFailure::DigestMismatch);

        Assert::AreEqual (0, fs.operations, L"nothing touched the folder");
    }



    TEST_METHOD (ZipWithoutExe_IsBadData)
    {
        MockUpdateFileSystem   fs;
        FakeSignatureVerifier  verifier;
        ZipUpdateInstaller     installer (fs, verifier);
        std::vector<Byte>      zip       = MakeReleaseZip (false);
        UpdateFailure          failure   = UpdateFailure::None;
        HRESULT                hr        = S_OK;



        MakeOldInstall (fs);
        verifier.isOfficial = true;

        hr = installer.Install (kpszInstallDir, zip, MakeAsset (zip), { 1, 31, 0 }, failure);
        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (failure == UpdateFailure::BadData);
        Assert::AreEqual (0, fs.operations);
    }



    TEST_METHOD (UnwritableFolder_ChangesNothing)
    {
        MockUpdateFileSystem   fs;
        FakeSignatureVerifier  verifier;
        ZipUpdateInstaller     installer (fs, verifier);
        std::vector<Byte>      zip       = MakeReleaseZip();
        UpdateFailure          failure   = UpdateFailure::None;
        HRESULT                hr        = S_OK;
        auto                   before    = std::map<std::wstring, std::vector<Byte>>();



        MakeOldInstall (fs);
        before              = fs.files;
        verifier.isOfficial = true;
        fs.failAt           = 1;

        hr = installer.Install (kpszInstallDir, zip, MakeAsset (zip), { 1, 31, 0 }, failure);
        Assert::AreEqual (E_ACCESSDENIED, hr);
        Assert::IsTrue   (failure == UpdateFailure::FolderNotWritable);
        Assert::IsTrue   (fs.files == before);
    }



    TEST_METHOD (UnofficialOrWrongVersion_ChangesNothing)
    {
        MockUpdateFileSystem   fs;
        FakeSignatureVerifier  verifier;
        ZipUpdateInstaller     installer (fs, verifier);
        std::vector<Byte>      zip       = MakeReleaseZip();
        UpdateFailure          failure   = UpdateFailure::None;
        HRESULT                hr        = S_OK;
        auto                   before    = std::map<std::wstring, std::vector<Byte>>();



        MakeOldInstall (fs);
        before = fs.files;

        verifier.isOfficial = false;
        hr = installer.Install (kpszInstallDir, zip, MakeAsset (zip), { 1, 31, 0 }, failure);
        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (failure == UpdateFailure::NotOfficial);
        Assert::IsTrue   (fs.files == before, L"staging cleaned up, nothing swapped");

        verifier.isOfficial = true;
        verifier.result     = E_FAIL;
        hr = installer.Install (kpszInstallDir, zip, MakeAsset (zip), { 1, 31, 0 }, failure);
        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (failure == UpdateFailure::NotOfficial, L"an unverifiable exe is not official");

        verifier.result = S_OK;
        hr = installer.Install (kpszInstallDir, zip, MakeAsset (zip), { 1, 32, 0 }, failure);
        Assert::IsTrue   (FAILED (hr));
        Assert::IsTrue   (failure == UpdateFailure::BadData, L"staged exe is 1.31.0, release says 1.32.0");
        Assert::IsTrue   (fs.files == before);
    }



    TEST_METHOD (FailEveryStep_LeavesOldCopyIntact)
    {
        std::vector<Byte>  zip           = MakeReleaseZip();
        ReleaseAsset       asset         = MakeAsset (zip);
        int                totalOps      = 0;
        int                swapFailures  = 0;



        // Count the operations a clean run takes.
        {
            MockUpdateFileSystem   fs;
            FakeSignatureVerifier  verifier;
            ZipUpdateInstaller     installer (fs, verifier);
            UpdateFailure          failure   = UpdateFailure::None;
            HRESULT                hr        = S_OK;

            MakeOldInstall (fs);
            verifier.isOfficial = true;

            hr = installer.Install (kpszInstallDir, zip, asset, { 1, 31, 0 }, failure);
            AssertSucceeded (hr);
            totalOps = fs.operations;
        }

        Assert::IsTrue (totalOps > 10, L"the clean run did real work");

        for (int n = 1; n <= totalOps; n++)
        {
            MockUpdateFileSystem   fs;
            FakeSignatureVerifier  verifier;
            ZipUpdateInstaller     installer (fs, verifier);
            UpdateFailure          failure   = UpdateFailure::None;
            HRESULT                hr        = S_OK;
            std::wstring           label     = std::format (L"failing operation {} of {}", n, totalOps);
            auto                   before    = std::map<std::wstring, std::vector<Byte>>();

            MakeOldInstall (fs);
            before              = fs.GetInstalledFiles (kpszInstallDir);
            verifier.isOfficial = true;
            fs.failAt           = n;

            hr = installer.Install (kpszInstallDir, zip, asset, { 1, 31, 0 }, failure);

            if (SUCCEEDED (hr))
            {
                // Only the closing cleanup of the staging folder may fail
                // without failing the update.
                Assert::AreEqual (totalOps, n, label.c_str());
                Assert::AreEqual (std::string ("new exe"), fs.Get (L"C:\\Apps\\Casso\\Casso.exe"), label.c_str());
                continue;
            }

            Assert::IsTrue (failure == UpdateFailure::FolderNotWritable || failure == UpdateFailure::InstallFailed,
                            label.c_str());
            Assert::IsTrue (fs.GetInstalledFiles (kpszInstallDir) == before, label.c_str());

            if (fs.renames > 0)
            {
                swapFailures++;
            }
        }

        Assert::IsTrue (swapFailures > 0, L"some failures landed mid-swap and were restored");
    }



    TEST_METHOD (RestoreFailure_IsReported)
    {
        std::vector<Byte>  zip          = MakeReleaseZip();
        ReleaseAsset       asset        = MakeAsset (zip);
        int                restoreFails = 0;
        int                successes    = 0;



        for (int n = 1; n <= 60; n++)
        {
            MockUpdateFileSystem   fs;
            FakeSignatureVerifier  verifier;
            ZipUpdateInstaller     installer (fs, verifier);
            UpdateFailure          failure   = UpdateFailure::None;
            HRESULT                hr        = S_OK;

            MakeOldInstall (fs);
            verifier.isOfficial = true;
            fs.failFrom         = n;

            hr = installer.Install (kpszInstallDir, zip, asset, { 1, 31, 0 }, failure);

            if (SUCCEEDED (hr))
            {
                successes++;
                Assert::IsTrue (failure == UpdateFailure::None);
                continue;
            }

            Assert::IsTrue (failure != UpdateFailure::None);

            if (failure == UpdateFailure::RestoreFailed)
            {
                restoreFails++;
            }
        }

        Assert::IsTrue (restoreFails > 0, L"a failure mid-swap with nothing able to move back is RestoreFailed");
        Assert::IsTrue (successes > 0,    L"failures past the end of the run leave it successful");
    }



    TEST_METHOD (Payload_ZipWithoutTopFolder)
    {
        std::vector<ZipEntry>  entries;
        HRESULT                hr      = S_OK;



        entries.push_back ({ "Casso.exe", false, {} });
        entries.push_back ({ "Demos\\a.dsk", false, {} });

        hr = ZipUpdateInstaller::GetPayload (entries);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Demos/a.dsk"), entries[1].path);
    }



    TEST_METHOD (Payload_StrayEntryOutsideTopFolder_IsBadData)
    {
        std::vector<ZipEntry>  entries;
        HRESULT                hr      = S_OK;



        entries.push_back ({ "Casso-x64/Casso.exe", false, {} });
        entries.push_back ({ "Other/x.txt", false, {} });

        hr = ZipUpdateInstaller::GetPayload (entries);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }
};
