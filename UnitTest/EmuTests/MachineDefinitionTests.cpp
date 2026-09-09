#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Machines/MachineDefinitions.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitionTests
//
//  What a shipped machine IS comes from code, and no document gets a vote.
//
//  The failure these guard against is not a crash. It is a machine that boots
//  and runs and is simply not the machine it claims to be: a //c with the
//  original keyboard, a ][ with //e soft switches. The preferences store
//  merges user deltas into internalDevices, so before this existed a hand
//  edit could compose hardware that never shipped and nothing would refuse it.
//
//  The tests therefore feed the loader documents that are WELL-FORMED AND
//  WRONG. A malformed file was always caught; a plausible lie was not.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineDefinitionTests)
{
public:

    TEST_METHOD (EveryShippedMachineHasADefinition)
    {
        std::vector<std::string>  ids = MachineDefinitions::GetKnownIds();

        Assert::AreEqual (size_t (5), ids.size(),
                          L"five machines ship, so five definitions exist");

        for (const std::string & id : ids)
        {
            Assert::IsNotNull (MachineDefinitions::Find (id),
                               L"a listed id must resolve to a definition");
        }
    }


    TEST_METHOD (AnUnknownMachineHasNoDefinition)
    {
        //  The documented way to add a machine is to copy a definition under a
        //  new name. Such a machine is its author's to compose, so it must not
        //  silently resolve to something else.
        Assert::IsNull (MachineDefinitions::Find ("Commodore64"));
        Assert::IsNull (MachineDefinitions::Find (""));
    }


    TEST_METHOD (TheTwoIIeModelsShareDevicesAndDifferOnlyInCpu)
    {
        const MachineDefinition *  plain    = MachineDefinitions::Find ("Apple2e");
        const MachineDefinition *  enhanced = MachineDefinitions::Find ("Apple2eEnhanced");

        Assert::IsNotNull (plain);
        Assert::IsNotNull (enhanced);

        Assert::AreEqual (plain->internalDevices.size(),
                          enhanced->internalDevices.size(),
                          L"the enhancement was the CPU, not the device set");

        for (size_t i = 0; i < plain->internalDevices.size(); i++)
        {
            Assert::AreEqual (plain->internalDevices[i].type,
                              enhanced->internalDevices[i].type);
        }

        Assert::AreEqual (std::string ("6502"),  plain->cpu);
        Assert::AreEqual (std::string ("65C02"), enhanced->cpu);
    }


    TEST_METHOD (TheIIcRunsTheIIeFamilyDevices)
    {
        const MachineDefinition *  c = MachineDefinitions::Find ("Apple2c");

        Assert::IsNotNull (c);
        AssertHasDevice (*c, "apple2e-family-keyboard");
        AssertHasDevice (*c, "apple2e-family-softswitches");
        AssertHasDevice (*c, "apple2e-family-mmu");
    }


    TEST_METHOD (ADocumentClaimingTheIIcHasTheOriginalKeyboardIsIgnored)
    {
        MachineConfig  config = TamperedIIcConfig();

        MachineConfigLoader::ApplyMachineDefinition ("Apple2c", config);

        AssertHasDevice   (config, "apple2e-family-keyboard");
        AssertLacksDevice (config, "apple2-family-keyboard");

        Assert::AreEqual (std::string ("apple2e-family-layout"), config.keyboardType,
                          L"the layout is the machine's, not the file's");
        Assert::AreEqual (std::string ("65C02"), config.cpu,
                          L"a //c is a 65C02 whatever the file says");
        Assert::AreEqual (size_t (2), config.ram.size(),
                          L"a //c has an auxiliary bank whatever the file says");
    }


    TEST_METHOD (ADocumentForAnUnknownMachineStillComposesItself)
    {
        MachineConfig  config = TamperedIIcConfig();

        //  The same values, under a name no definition claims. Nothing
        //  overrides them, because a machine Casso does not ship is not
        //  Casso's to compose -- that is the documented way to add one.
        MachineConfigLoader::ApplyMachineDefinition ("MyOwnMachine", config);

        AssertHasDevice (config, "apple2-family-keyboard");
        Assert::AreEqual (std::string ("6502"), config.cpu);
        Assert::AreEqual (size_t (1), config.ram.size());
    }


private:

    static void AssertHasDevice (const MachineConfig & config, const char * type)
    {
        bool  found = false;

        for (const InternalDevice & device : config.internalDevices)
        {
            if (device.type == type)
            {
                found = true;
                break;
            }
        }

        Assert::IsTrue (found, std::wstring (L"expected device: ").append (
                            std::wstring (type, type + strlen (type))).c_str());
    }


    static void AssertHasDevice (const MachineDefinition & definition, const char * type)
    {
        MachineConfig  shim;

        shim.internalDevices = definition.internalDevices;
        AssertHasDevice (shim, type);
    }


    static void AssertLacksDevice (const MachineConfig & config, const char * type)
    {
        for (const InternalDevice & device : config.internalDevices)
        {
            Assert::AreNotEqual (std::string (type), device.type,
                                 L"a device the machine does not have");
        }
    }


    //
    //  A //c as a well-formed and wrong document would describe it: the
    //  original keyboard, the original soft switches, one RAM bank, a 6502.
    //  Every one of those is false and none of them is malformed, which is
    //  exactly the case a parser cannot catch.
    //
    static MachineConfig TamperedIIcConfig()
    {
        MachineConfig  config;

        config.name             = "Apple //c";
        config.cpu              = "6502";
        config.keyboardType     = "apple2-family-layout";
        config.ram              = { { .address = 0x0000, .size = 0xC000 } };
        config.internalDevices  = { { .type = "apple2-family-keyboard" },
                                    { .type = "apple2-family-speaker" },
                                    { .type = "apple2-family-softswitches" } };
        config.videoConfig.modes = { "apple2-text40", "apple2-lores", "apple2-hires" };

        return (config);
    }
};
