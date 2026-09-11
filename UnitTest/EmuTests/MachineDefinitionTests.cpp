#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Machines/MachineDefinitions.h"
#include "Machines/Apple2/Apple2/Apple2.h"
#include "Machines/Apple2/Apple2Plus/Apple2Plus.h"
#include "Machines/Apple2/Apple2e/Apple2e.h"
#include "Machines/Apple2/Apple2eEnhanced/Apple2eEnhanced.h"
#include "Machines/Apple2/Apple2c/Apple2c.h"

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


    TEST_METHOD (TheChainIsRootedAtTheAppleII)
    {
        //  Each machine is the one before it with things added or replaced, so
        //  a later model IS an earlier one and the compiler can say so. If this
        //  stops compiling the hierarchy has been re-rooted.
        Assert::IsTrue ((std::is_base_of_v<Apple2,      Apple2Plus>));
        Assert::IsTrue ((std::is_base_of_v<Apple2Plus,  Apple2e>));
        Assert::IsTrue ((std::is_base_of_v<Apple2e,     Apple2eEnhanced>));
        Assert::IsTrue ((std::is_base_of_v<Apple2e,     Apple2c>));

        //  Siblings, not ancestors: the //c shipped a year before the Enhanced
        //  //e and brought the 65C02 first.
        Assert::IsFalse ((std::is_base_of_v<Apple2eEnhanced, Apple2c>));
        Assert::IsFalse ((std::is_base_of_v<Apple2c, Apple2eEnhanced>));
    }


    TEST_METHOD (ThePlusInheritsEverythingButItsName)
    {
        Apple2      plain;
        Apple2Plus  plus;

        //  A ][+ differs from a ][ by a ROM file and a card in a slot, both of
        //  which are the owner's. Nothing about the machine itself changed.
        Assert::AreEqual (plain.GetCpu(),                    plus.GetCpu());
        Assert::AreEqual (plain.GetKeyboardLayout(),         plus.GetKeyboardLayout());
        Assert::AreEqual (plain.GetInternalDevices().size(), plus.GetInternalDevices().size());
        Assert::AreEqual (plain.GetVideoModes().size(),      plus.GetVideoModes().size());
        Assert::AreEqual (plain.GetSlotCount(),              plus.GetSlotCount());
    }


    TEST_METHOD (TheIIcHasNoSlotsAndTheIIeHasSeven)
    {
        Apple2e  e;
        Apple2c  c;

        //  Zero is a count, not a facet taken away: every loop that walks
        //  slots still runs, and finds none.
        Assert::AreEqual (7, e.GetSlotCount());
        Assert::AreEqual (0, c.GetSlotCount());
    }


    TEST_METHOD (TheOriginalMachinesHaveAGamePortDeviceAndTheLaterOnesDoNot)
    {
        Apple2      plain;
        Apple2Plus  plus;
        Apple2e     e;
        Apple2c     c;

        //  The one real removal in the family, expressed as declining to
        //  create: Apple2eSoftSwitchBank absorbed the paddle timer and PREAD,
        //  so a separate game-port device would be a second owner of $C070.
        Assert::IsTrue  (plain.HasGamePortDevice());
        Assert::IsTrue  (plus.HasGamePortDevice());
        Assert::IsFalse (e.HasGamePortDevice());
        Assert::IsFalse (c.HasGamePortDevice());
    }


    TEST_METHOD (OnlyTheAuxBankMachinesOfferEightyColumnsAndDoubleHiRes)
    {
        Apple2   plain;
        Apple2e  e;

        //  Physical, not policy: without a second bank there is nowhere for the
        //  interleaved half of the picture to live.
        Assert::AreEqual ((size_t) 1, plain.GetRam().size());
        Assert::AreEqual ((size_t) 2, e.GetRam().size());

        AssertHasMode (plain, "apple2-hires");
        AssertLacksMode (plain, "apple2-text80");
        AssertLacksMode (plain, "apple2-doublehires");

        AssertHasMode (e, "apple2-text80");
        AssertHasMode (e, "apple2-doublehires");
    }


private:

    static void AssertHasMode (const IMachine & machine, const char * mode)
    {
        std::vector<std::string>  modes = machine.GetVideoModes();

        Assert::IsTrue (std::find (modes.begin(), modes.end(), mode) != modes.end(),
                        L"expected this machine to offer the mode");
    }


    static void AssertLacksMode (const IMachine & machine, const char * mode)
    {
        std::vector<std::string>  modes = machine.GetVideoModes();

        Assert::IsTrue (std::find (modes.begin(), modes.end(), mode) == modes.end(),
                        L"this machine has no hardware for that mode");
    }


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

        return (config);
    }
};
