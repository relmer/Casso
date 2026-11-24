#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/GuestSession.h"
#include "../EmuTests/TestMachine.h"
#include "Core/JsonParser.h"
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
        std::vector<std::string>  fromC     = { "Apple2c", "Apple2eEnhanced", "Apple2Plus" };
        std::vector<std::string>  fromNone  = { "Apple2eEnhanced", "Apple2Plus" };
        std::vector<std::string>  fromPlus  = { "Apple2Plus", "Apple2eEnhanced" };
        std::vector<std::string>  fromCPlus = { "Apple2cPlus", "Apple2eEnhanced", "Apple2Plus" };



        Assert::IsTrue (BootCheck::GetMachines ("Apple //c (ROM 3)") == fromC);
        Assert::IsTrue (BootCheck::GetMachines ("") == fromNone);
        Assert::IsTrue (BootCheck::GetMachines ("Apple //e enhanced") == fromNone, L"The enhanced //e once");
        Assert::IsTrue (BootCheck::GetMachines ("Apple ][+") == fromPlus);
        Assert::IsTrue (BootCheck::GetMachines ("Apple IIgs") == fromNone, L"A model Casso lacks");
        Assert::IsTrue (BootCheck::GetMachines ("Apple //c Plus") == fromCPlus);
        Assert::AreEqual (std::string ("Apple //c Plus"), BootCheck::GetMachineName ("Apple2cPlus"));

        Assert::IsTrue (BootCheck::IsBetter (BootVerdict::Booted, BootVerdict::Unclear));
        Assert::IsTrue (BootCheck::IsBetter (BootVerdict::Unclear, BootVerdict::DidNotBoot));
        Assert::IsFalse (BootCheck::IsBetter (BootVerdict::DidNotBoot, BootVerdict::DidNotBoot));
    }

    //  A boot's machine has only its Disk II cards: a card this build can't
    //  make, as a newer build's, doesn't fail the machine.
    TEST_METHOD (KeepDiskSlots_DropsEveryCardButTheDiskII)
    {
        std::string  json = R"({ "name": "Apple //e", "slots": [
                                   { "slot": 4, "device": "mockingboard-c" },
                                   { "slot": 5, "device": "liron-card", "rom": "Liron.rom" },
                                   { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" } ] })";
        JsonValue          root;
        JsonParseError     error;
        const JsonValue  * slots = nullptr;
        std::string        device;



        AssertSucceeded (BootMachine::KeepDiskSlots (json));
        AssertSucceeded (JsonParser::Parse (json, root, error));

        Assert::IsTrue (root.HasArray ("slots", slots));
        Assert::AreEqual ((size_t) 1, slots->GetArraySize());
        Assert::IsTrue (slots->GetArrayElement (0).HasString ("device", device));
        Assert::AreEqual (std::string ("disk-ii"), device);
        Assert::IsTrue (root.HasString ("name", device), L"The other fields are as they were");
    }
};
