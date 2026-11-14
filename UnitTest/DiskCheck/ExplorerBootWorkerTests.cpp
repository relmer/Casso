#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "CassoExplorer/ExplorerBootWorker.h"
#include "Core/TextEncoding.h"

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

            worker.Queue (image.wstring(), "Apple //e enhanced");
            worker.Queue (image.wstring(), "Apple //e enhanced");
            worker.Queue (other.wstring(), "");

            Assert::AreEqual ((size_t) 2, worker.GetPending(), L"A path waiting isn't queued twice");

            isBusy = false;
            worker.WaitUntilIdle();

            Assert::AreEqual ((size_t) 2, results.size());
            Assert::IsTrue (results[0].hasReport && results[0].verdict == BootVerdict::Booted, TextEncoding::NarrowToWide (results[0].reason).c_str());
            Assert::AreEqual (std::string ("Apple2eEnhanced"), results[0].machine);
            Assert::IsFalse (results[1].hasReport, L"A file that isn't a disk image");

            AssertSucceeded (store.Find (image, png, record, isUserSet));
            Assert::IsFalse (png.empty(), L"The thumbnail is stored");
            Assert::AreEqual (std::string ("booted"), record.verdict);
        }

        std::filesystem::remove_all (root);
    }
};
