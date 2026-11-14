#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "Core/TextEncoding.h"
#include "DiskCheck/BootCheck.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootCheckTests
//
//  The machines an image is booted on, in order, and the report from the
//  best of them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootCheckTests)
{
public:

    //  A machine from the unit tests' fixtures, as BootCheck's builder.
    static HRESULT  BuildFromFixtures (const std::string & machineId, BootCheck::Built & outBuilt, std::string & outError)
    {
        std::shared_ptr<TestMachine>  machine = std::make_shared<TestMachine> (machineId);



        outError.clear();

        outBuilt.builder = &machine->GetBuilder();
        outBuilt.host    = machine;

        return S_OK;
    }

    //  The model the disk was captured from is first when Casso has it, then
    //  the enhanced //e and the ][+, with no machine twice.
    TEST_METHOD (TheMachines_BeginWithTheModelCapturedFrom)
    {
        std::vector<std::string>  fromC    = { "Apple2c", "Apple2eEnhanced", "Apple2Plus" };
        std::vector<std::string>  fromNone = { "Apple2eEnhanced", "Apple2Plus" };
        std::vector<std::string>  fromPlus = { "Apple2Plus", "Apple2eEnhanced" };



        Assert::IsTrue (BootCheck::GetMachines ("Apple //c (ROM 3)") == fromC);
        Assert::IsTrue (BootCheck::GetMachines ("") == fromNone);
        Assert::IsTrue (BootCheck::GetMachines ("Apple //e enhanced") == fromNone, L"The enhanced //e once");
        Assert::IsTrue (BootCheck::GetMachines ("Apple ][+") == fromPlus);
        Assert::IsTrue (BootCheck::GetMachines ("Apple IIgs") == fromNone, L"A model Casso lacks");

        Assert::IsTrue (BootCheck::IsBetter (BootVerdict::Booted, BootVerdict::Unclear));
        Assert::IsTrue (BootCheck::IsBetter (BootVerdict::Unclear, BootVerdict::DidNotBoot));
        Assert::IsFalse (BootCheck::IsBetter (BootVerdict::DidNotBoot, BootVerdict::DidNotBoot));
    }

    //  A machine that can't be built is passed over and listed, and the
    //  image is booted on the next; with no machine built there is no
    //  report.
    TEST_METHOD (AMachineThatCantBeBuilt_IsPassedOver_AndListed)
    {
        std::vector<Byte>         master = GuestSession::RequireDos33Master();
        HRESULT                   hrNone = S_OK;
        BootReport                report;
        std::string               machine;
        std::vector<std::string>  missing;
        std::vector<std::string>  built;
        auto                      noC    = [&built] (const std::string & id, BootCheck::Built & outBuilt, std::string & outError)
        {
            built.push_back (id);

            if (id == "Apple2c")
            {
                outError = "Apple2c's ROM isn't installed.";
                return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
            }

            return BuildFromFixtures (id, outBuilt, outError);
        };
        auto                      none   = [] (const std::string &, BootCheck::Built &, std::string & outError)
        {
            outError = "No ROM is installed.";
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        };



        AssertSucceeded (BootCheck::Run (master, DiskFormat::Dsk, "Apple //c", BootProbe::Limits(), noC, report, machine, missing));

        Assert::IsTrue (report.verdict == BootVerdict::Booted, TextEncoding::NarrowToWide (report.reason).c_str());
        Assert::AreEqual (std::string ("Apple2eEnhanced"), machine);
        Assert::AreEqual ((size_t) 1, missing.size());
        Assert::AreEqual ((size_t) 2, built.size(), L"No machine after the one that booted");

        hrNone = BootCheck::Run (master, DiskFormat::Dsk, "", BootProbe::Limits(), none, report, machine, missing);

        Assert::IsTrue (FAILED (hrNone), L"No report with no machine");
        Assert::AreEqual ((size_t) 2, missing.size());
    }
};
