#include "Pch.h"
#include "Core/MachineConfig.h"
#include "Core/UnicodeSymbols.h"
#include "Machines/MachineDefinitions.h"
#include "Machines/Apple2/Common/WozCompatibility.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibilityTests
//
//  What a WOZ image declares about the machine it needs, checked against the
//  machine running it, and the text the drive's info icon shows. The cases
//  are the real images on hand: Choplifter declares the ][ and ][+ only, and
//  Carmen Sandiego the ][+ onward with 64K.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozCompatibilityTests)
{
public:

    static constexpr uint16_t  kChoplifterHardware = WozCompatibility::kApple2 | WozCompatibility::kApple2Plus;
    static constexpr uint16_t  kCarmenHardware     = 0x3E;
    static constexpr uint16_t  kCarmenRamK         = 64;
    static constexpr uint16_t  k48K                = 48;
    static constexpr uint16_t  k128K               = 128;


    static WozMetadata MakeInfo (Byte version, uint16_t hardware, uint16_t ramK)
    {
        WozMetadata  meta;

        meta.infoPayload.assign (WozLoader::kInfoChunkSize, 0);
        meta.infoPayload[WozLoader::kInfoOffsetVersion]          = version;
        meta.infoPayload[WozLoader::kInfoOffsetCompatibleHw]     = static_cast<Byte> (hardware & 0xFF);
        meta.infoPayload[WozLoader::kInfoOffsetCompatibleHw + 1] = static_cast<Byte> (hardware >> 8);
        meta.infoPayload[WozLoader::kInfoOffsetRequiredRam]      = static_cast<Byte> (ramK & 0xFF);
        meta.infoPayload[WozLoader::kInfoOffsetRequiredRam + 1]  = static_cast<Byte> (ramK >> 8);

        return meta;
    }


    static WozMachineFacts MakeMachine (const std::string & machineId)
    {
        const MachineDefinition  * definition = MachineDefinitions::Find (machineId);
        MachineConfig              config;

        Assert::IsNotNull (definition);

        config.machineId       = machineId;
        config.ram             = definition->ram;
        config.internalDevices = definition->internalDevices;

        return WozCompatibility::GetMachineFacts (config);
    }


    static WozRequirements Declare (uint16_t hardware, uint16_t ramK)
    {
        WozRequirements  declared;

        declared.compatibleHardware = hardware;
        declared.requiredRamK       = ramK;

        return declared;
    }


    TEST_METHOD (RequirementsAreReadFromInfoVersion2On)
    {
        WozRequirements  declared = WozCompatibility::ReadRequirements (MakeInfo (2, kCarmenHardware, kCarmenRamK));

        Assert::AreEqual (kCarmenHardware, declared.compatibleHardware);
        Assert::AreEqual (kCarmenRamK,     declared.requiredRamK);

        declared = WozCompatibility::ReadRequirements (MakeInfo (3, WozCompatibility::kApple3Plus, 256));

        Assert::AreEqual (WozCompatibility::kApple3Plus, declared.compatibleHardware, L"the high byte of the field counts");
        Assert::AreEqual (uint16_t (256),                declared.requiredRamK,       L"the high byte of the field counts");
    }


    TEST_METHOD (OlderOrMissingInfoDeclaresNothing)
    {
        WozMetadata  none;

        Assert::IsTrue (WozCompatibility::ReadRequirements (none) == WozRequirements(),
                        L"no INFO: a disk Casso built, or not a WOZ at all");
        Assert::IsTrue (WozCompatibility::ReadRequirements (MakeInfo (1, kChoplifterHardware, k48K)) == WozRequirements(),
                        L"INFO version 1 holds reserved bytes there, not these fields");
    }


    TEST_METHOD (EveryShippedMachineHasItsOwnFlag)
    {
        Assert::AreEqual (WozCompatibility::kApple2,          MakeMachine ("Apple2").hardwareFlag);
        Assert::AreEqual (WozCompatibility::kApple2Plus,      MakeMachine ("Apple2Plus").hardwareFlag);
        Assert::AreEqual (WozCompatibility::kApple2e,         MakeMachine ("Apple2e").hardwareFlag);
        Assert::AreEqual (WozCompatibility::kApple2eEnhanced, MakeMachine ("Apple2eEnhanced").hardwareFlag);
        Assert::AreEqual (WozCompatibility::kApple2c,         MakeMachine ("Apple2c").hardwareFlag);
    }


    TEST_METHOD (RamTotalsMatchTheHardware)
    {
        Assert::AreEqual (k48K,  MakeMachine ("Apple2").ramK);
        Assert::AreEqual (k48K,  MakeMachine ("Apple2Plus").ramK);
        Assert::AreEqual (k128K, MakeMachine ("Apple2e").ramK,         L"48K main + 48K aux + 16K language card per bank");
        Assert::AreEqual (k128K, MakeMachine ("Apple2eEnhanced").ramK);
        Assert::AreEqual (k128K, MakeMachine ("Apple2c").ramK);
    }


    TEST_METHOD (UnknownMachineHasNoHardwareFlag)
    {
        MachineConfig    config;
        WozMachineFacts  facts;

        config.machineId = "MyOwnApple";
        facts            = WozCompatibility::GetMachineFacts (config);

        Assert::AreEqual (uint16_t (0), facts.hardwareFlag);
        Assert::IsFalse  (WozCompatibility::HasHardwareConflict (Declare (kChoplifterHardware, 0), facts),
                          L"a machine the format has no bit for cannot be judged against the list");
    }


    TEST_METHOD (ChoplifterConflictsOnlyOffTheIIAndIIPlus)
    {
        WozRequirements  declared = Declare (kChoplifterHardware, k48K);

        Assert::IsFalse (WozCompatibility::HasConflict (declared, MakeMachine ("Apple2")));
        Assert::IsFalse (WozCompatibility::HasConflict (declared, MakeMachine ("Apple2Plus")));
        Assert::IsTrue  (WozCompatibility::HasConflict (declared, MakeMachine ("Apple2e")));
        Assert::IsTrue  (WozCompatibility::HasConflict (declared, MakeMachine ("Apple2eEnhanced")));
        Assert::IsTrue  (WozCompatibility::HasConflict (declared, MakeMachine ("Apple2c")));
    }


    TEST_METHOD (CarmenConflictsOnTheIIAndOnA48KIIPlus)
    {
        WozRequirements  declared = Declare (kCarmenHardware, kCarmenRamK);

        Assert::IsTrue  (WozCompatibility::HasHardwareConflict (declared, MakeMachine ("Apple2")));
        Assert::IsTrue  (WozCompatibility::HasRamConflict      (declared, MakeMachine ("Apple2")));
        Assert::IsFalse (WozCompatibility::HasHardwareConflict (declared, MakeMachine ("Apple2Plus")));
        Assert::IsTrue  (WozCompatibility::HasRamConflict      (declared, MakeMachine ("Apple2Plus")),
                         L"Casso's ][+ has no language card, so 48K against 64K");
        Assert::IsFalse (WozCompatibility::HasConflict         (declared, MakeMachine ("Apple2e")));
        Assert::IsFalse (WozCompatibility::HasConflict         (declared, MakeMachine ("Apple2c")));
    }


    TEST_METHOD (UnsaidFieldsNeverConflict)
    {
        Assert::IsFalse (WozCompatibility::HasConflict (WozRequirements(), MakeMachine ("Apple2")));
        Assert::IsFalse (WozCompatibility::HasConflict (Declare (0, 0), MakeMachine ("Apple2c")));
    }


    TEST_METHOD (HardwareListReadsTheWayTheMachinesWereSold)
    {
        Assert::AreEqual (std::wstring (L"Apple ][") + s_kchThinSpace + L", ][+", WozCompatibility::FormatHardwareList (kChoplifterHardware),
                          L"a comma straight after the ][ tucks under its bracket, so a thin space holds it off");
        Assert::AreEqual (std::wstring (L"Apple ][+, //e, //c, //e Enhanced, IIgs"),
                          WozCompatibility::FormatHardwareList (kCarmenHardware));
        Assert::AreEqual (std::wstring (L"Apple ][") + s_kchThinSpace + L", 0x0200", WozCompatibility::FormatHardwareList (0x0201),
                          L"a bit the format has no model for is shown, not dropped");
    }


    TEST_METHOD (TooltipMarksEachRequirementTheMachineMeetsOrMisses)
    {
        std::wstring  expected = L"This .woz image specifies:\n"
                                 L"  Compatible with: Apple ][" + std::wstring (1, s_kchThinSpace) + L", ][+\n"
                                 L"  Minimum RAM: 48K\n"
                                 L"\n"
                                 L"This machine:\n"
                                 L"  " + std::wstring (s_kpszRedCross) + L" Apple //e Enhanced\n"
                                 L"  " + std::wstring (s_kpszGreenCheck) + L" RAM 128K";

        Assert::AreEqual (expected, WozCompatibility::ComposeTooltip (Declare (kChoplifterHardware, k48K),
                                                                      MakeMachine ("Apple2eEnhanced"),
                                                                      L"Apple //e Enhanced"));
    }


    TEST_METHOD (TooltipMarksBothRequirementsMissed)
    {
        std::wstring  expected = L"This .woz image specifies:\n"
                                 L"  Compatible with: Apple ][+, //e, //c, //e Enhanced, IIgs\n"
                                 L"  Minimum RAM: 64K\n"
                                 L"\n"
                                 L"This machine:\n"
                                 L"  " + std::wstring (s_kpszRedCross) + L" Apple ][\n"
                                 L"  " + std::wstring (s_kpszRedCross) + L" RAM 48K";

        Assert::AreEqual (expected, WozCompatibility::ComposeTooltip (Declare (kCarmenHardware, kCarmenRamK),
                                                                      MakeMachine ("Apple2"),
                                                                      L"Apple ]["));
    }


    TEST_METHOD (TooltipListsOnlyTheRequirementsTheImageGives)
    {
        std::wstring  expected = L"This .woz image specifies:\n"
                                 L"  Minimum RAM: 64K\n"
                                 L"\n"
                                 L"This machine:\n"
                                 L"  " + std::wstring (s_kpszRedCross) + L" RAM 48K";

        Assert::AreEqual (expected, WozCompatibility::ComposeTooltip (Declare (0, kCarmenRamK),
                                                                      MakeMachine ("Apple2Plus"),
                                                                      L"Apple ][ plus"));
    }


    TEST_METHOD (TooltipQuestionsAMachineTheListCannotJudge)
    {
        WozMachineFacts  ownMachine;
        std::wstring     expected = L"This .woz image specifies:\n"
                                    L"  Compatible with: Apple ][+, //e, //c, //e Enhanced, IIgs\n"
                                    L"  Minimum RAM: 64K\n"
                                    L"\n"
                                    L"This machine:\n"
                                    L"  " + std::wstring (s_kpszGrayQuestion) + L" My Own Apple\n"
                                    L"  " + std::wstring (s_kpszRedCross) + L" RAM 48K";

        ownMachine.ramK = k48K;

        Assert::AreEqual (expected, WozCompatibility::ComposeTooltip (Declare (kCarmenHardware, kCarmenRamK),
                                                                      ownMachine,
                                                                      L"My Own Apple"),
                          L"the format has no bit for this machine, so the list can neither pass nor fail it");
    }

    TEST_METHOD (NoConflictMeansNoTooltip)
    {
        Assert::IsTrue (WozCompatibility::ComposeTooltip (Declare (kCarmenHardware, kCarmenRamK),
                                                          MakeMachine ("Apple2e"),
                                                          L"Apple //e").empty());
    }
};
