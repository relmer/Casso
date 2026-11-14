#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "CassoExplorer/ExplorerBootWorker.h"
#include "CassoLink/FullSend/FullSendManifestFile.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/GivenNames.h"
#include "Seams/Win32DiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorkerTests
//
//  A disk image queued on the worker is booted, and its thumbnail and boot
//  result are in the store when the done function is called.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerBootWorkerTests)
{
public:

    static HRESULT  BuildFromFixtures (const std::string & machineId, BootCheck::Built & outBuilt, std::string & outError)
    {
        std::shared_ptr<TestMachine>  machine = std::make_shared<TestMachine> (machineId);



        outError.clear();

        outBuilt.builder = &machine->GetBuilder();
        outBuilt.host    = machine;

        return S_OK;
    }

    TEST_METHOD (ANumberedName_TakesItsTitleAfterTheNumber)
    {
        Assert::AreEqual (std::string ("0019 The Witness - An Interlogic Mystery.woz"), ExplorerBootWorker::MakeTitledName ("0019.woz", "THE WITNESS: AN INTERLOGIC MYSTERY"));
        Assert::AreEqual (std::string ("0007 Spy vs Spy.woz"), ExplorerBootWorker::MakeTitledName ("0007.woz", "Spy / vs  Spy..."));
        Assert::IsTrue (ExplorerBootWorker::MakeTitledName ("0019 Archon.woz", "ARCHON").empty(), L"A name that isn't a number alone");
        Assert::IsTrue (ExplorerBootWorker::MakeTitledName ("0019.woz", "").empty(), L"No title");
        Assert::IsTrue (ExplorerBootWorker::MakeTitledName ("0019.woz", "???").empty(), L"A title with nothing a file name can have");
    }

    //  The DOS 3.3 master as a file: queued, booted and stored; a file that
    //  isn't a disk image has no report; and a path queued twice while it
    //  waits is booted once.
    TEST_METHOD (AQueuedImage_IsBooted_AndItsThumbnailStored)
    {
        std::filesystem::path             root  = std::filesystem::temp_directory_path() / std::format (L"CassoBootWorker{}", ::GetCurrentProcessId());
        std::filesystem::path             image = root / L"0001.dsk";
        std::filesystem::path             other = root / L"notes.txt";
        std::vector<Byte>                        master    = GuestSession::RequireDos33Master();
        std::vector<Byte>                        png;
        ThumbnailRecord                          record;
        bool                                     isUserSet = false;
        std::mutex                               lock;
        std::vector<ExplorerBootWorker::Result>  results;
        std::atomic<bool>                        isBusy    { true };



        std::filesystem::remove_all (root);
        std::filesystem::create_directories (root);

        {
            std::ofstream  out (image, std::ios::binary);

            out.write ((const char *) master.data(), (std::streamsize) master.size());
        }

        {
            std::ofstream  out (other, std::ios::binary);

            out << "not a disk";
        }

        {
            ThumbnailStore      store (root / L"cache");
            ExplorerBootWorker  worker (store, BuildFromFixtures);

            worker.SetBusy ([&isBusy]() { return isBusy.load(); });
            worker.SetDone ([&lock, &results] (const ExplorerBootWorker::Result & result)
            {
                std::lock_guard  guard (lock);

                results.push_back (result);
            });

            worker.Queue (image.wstring(), "Apple //e enhanced", "Modem");
            worker.Queue (image.wstring(), "Apple //e enhanced");
            worker.Queue (other.wstring(), "");

            Assert::AreEqual ((size_t) 2, worker.GetPending(), L"A path waiting isn't queued twice");

            isBusy = false;
            worker.WaitUntilIdle();

            Assert::AreEqual ((size_t) 2, results.size());
            Assert::IsTrue (results[0].hasReport && results[0].verdict == BootVerdict::Booted, TextEncoding::NarrowToWide (results[0].reason).c_str());
            Assert::AreEqual (std::string ("Apple2eEnhanced"), results[0].machine);
            Assert::AreEqual (std::string ("Modem"), results[0].source);
            Assert::AreEqual (std::string ("0001.dsk booted on the enhanced Apple //e."), ExplorerBootWorker::Describe (results[0]));
            Assert::AreEqual (std::string ("notes.txt wasn't booted."), ExplorerBootWorker::Describe (results[1]));
            Assert::IsFalse (results[1].hasReport, L"A file that isn't a disk image");

            AssertSucceeded (store.Find (image, png, record, isUserSet));
            Assert::IsFalse (png.empty(), L"The thumbnail is stored");
            Assert::AreEqual (std::string ("booted"), record.verdict);
        }

        std::filesystem::remove_all (root);
    }

    TEST_METHOD (ANumberedName_TakesAGivenNameAsWritten)
    {
        Assert::AreEqual (std::string ("0031 Archon (side A).woz"), ExplorerBootWorker::MakeNumberedName ("0031.woz", "Archon (side A)"));
        Assert::AreEqual (std::string ("0031 AC DC.dsk"), ExplorerBootWorker::MakeNumberedName ("0031.dsk", "AC/DC"));
        Assert::IsTrue (ExplorerBootWorker::MakeNumberedName ("0031 Archon.woz", "Archon").empty(), L"A name that isn't a number alone");
    }

    //  A raw capture saved as a number, whose disk's fingerprint has the
    //  user's name for an earlier capture, is renamed to its number and that
    //  name, in the folder and in its manifest row.
    TEST_METHOD (ARawCapture_TakesTheNameGivenToItsFingerprint)
    {
        std::filesystem::path       root        = std::filesystem::temp_directory_path() / std::format (L"CassoBootGiven{}", ::GetCurrentProcessId());
        std::filesystem::path       image       = root / L"0031.dsk";
        std::string                 fingerprint = std::string (64, 'a');
        std::vector<Byte>           master      = GuestSession::RequireDos33Master();
        FullSendManifest            manifest;
        FullSendRow                 row;
        std::vector<Byte>           text;
        Win32DiskFileIo             io;
        ExplorerBootWorker::Result  result;
        std::string                 found;
        bool                        isRaw       = false;



        std::filesystem::remove_all (root);
        std::filesystem::create_directories (root);

        {
            std::ofstream  out (image, std::ios::binary);

            out.write ((const char *) master.data(), (std::streamsize) master.size());
        }

        row.sequence    = 31;
        row.file        = "0031.dsk";
        row.fingerprint = fingerprint;
        row.hash        = std::string (64, 'b');
        row.time        = "2026-11-14T10:00:00";
        row.status      = FullSendManifest::kStatusRawCapture;
        manifest.Add (row);
        text = manifest.Format();

        {
            std::ofstream  out (root / FullSendManifest::kFileName, std::ios::binary);

            out.write ((const char *) text.data(), (std::streamsize) text.size());
        }

        {
            ThumbnailStore      store (root / L"cache");
            GivenNames          given (root / L"cache");
            ExplorerBootWorker  worker (store, BuildFromFixtures);

            AssertSucceeded (given.Set (fingerprint, "Archon"));

            worker.SetManifestIo (&io);
            worker.SetGivenNames (&given);

            result = worker.Check (image.wstring(), "Apple //e enhanced");
        }

        Assert::AreEqual (std::string ("0031.dsk"), result.renamedFrom);
        Assert::IsTrue (std::filesystem::exists (root / L"0031 Archon.dsk"), L"Renamed in the folder");

        AssertSucceeded (FullSendManifestFile::FindFingerprint (io, root.string(), "0031 Archon.dsk", found, isRaw));
        Assert::AreEqual (fingerprint, found, L"Renamed in the manifest");

        std::filesystem::remove_all (root);
    }
};
