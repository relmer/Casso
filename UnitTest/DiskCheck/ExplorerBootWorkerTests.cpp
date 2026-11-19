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

    TEST_METHOD (ANumberedName_TakesAGivenNameAsWritten)
    {
        Assert::AreEqual (std::string ("0031 Archon (side A).woz"), ExplorerBootWorker::MakeNumberedName ("0031.woz", "Archon (side A)"));
        Assert::AreEqual (std::string ("0031 AC DC.dsk"), ExplorerBootWorker::MakeNumberedName ("0031.dsk", "AC/DC"));
        Assert::IsTrue (ExplorerBootWorker::MakeNumberedName ("0031 Archon.woz", "Archon").empty(), L"A name that isn't a number alone");
    }
};
