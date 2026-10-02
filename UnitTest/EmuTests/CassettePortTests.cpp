#include "Pch.h"

#include "Devices/Tape/ITapeDeckPort.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/CassettePort.h"
#include "Machines/MachineDefinitions.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeTapeDeckPort
//
//  A recorder whose input level the test sets, counting output toggles and
//  remembering the cycle of the last access.
//
////////////////////////////////////////////////////////////////////////////////

class FakeTapeDeckPort : public ITapeDeckPort
{
public:
    bool      level       = false;
    int       toggles     = 0;
    int       reads       = 0;
    uint64_t  lastCycle   = 0;

    bool  ReadInputLevel (uint64_t busCycle) override { reads++; lastCycle = busCycle; return level; }
    void  OnOutputToggle (uint64_t busCycle) override { toggles++; lastCycle = busCycle; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassettePortTests
//
//  The guest-visible contract of the cassette jacks on each model, driven
//  through the real machine builders and the bus.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassettePortTests)
{
public:

    static constexpr Byte  kBit7     = 0x80;
    static constexpr Byte  kLowBits  = 0x7F;

    static constexpr const char * s_kpszCassetteModels[] = { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" };


    static void AttachFake (TestMachine & machine, FakeTapeDeckPort & fake)
    {
        CassettePort  * port = machine.GetRefs().cassettePort;



        Assert::IsNotNull (port);
        port->SetDeck (&fake);
    }


    TEST_METHOD (InputLevelAppearsInBit7OfC060AndC068OnEveryCassetteModel)
    {
        constexpr Word  kAddresses[] = { CassettePort::kInputAddress, CassettePort::kInputMirrorAddress };
        size_t          checked      = 0;



        for (const char * pszModel : s_kpszCassetteModels)
        {
            TestMachine       machine (pszModel, TestMachine::Slots::Empty);
            FakeTapeDeckPort  fake;



            AttachFake (machine, fake);

            for (Word address : kAddresses)
            {
                fake.level = true;
                Assert::AreEqual (kBit7, (Byte) (machine.GetMemoryBus().ReadByte (address) & kBit7), ToWide (pszModel).c_str());

                fake.level = false;
                Assert::AreEqual (Byte (0), (Byte) (machine.GetMemoryBus().ReadByte (address) & kBit7), ToWide (pszModel).c_str());
                checked++;
            }
        }

        Assert::AreEqual (size_t (8), checked);
    }


    TEST_METHOD (LowBitsFloatOnTheIIPlus)
    {
        TestMachine       machine ("Apple2Plus", TestMachine::Slots::Empty);
        FakeTapeDeckPort  fake;
        Byte              floating = 0;
        Byte              value    = 0;



        AttachFake (machine, fake);
        machine.GetMemoryBus().WriteByte (0x0300, 0x5A);
        machine.GetMemoryBus().ReadByte  (0xC000);

        floating = machine.GetMemoryBus().GetFloatingBusValue();
        value    = machine.GetMemoryBus().ReadByte (CassettePort::kInputAddress);

        Assert::AreEqual ((Byte) (floating & kLowBits), (Byte) (value & kLowBits));
    }


    TEST_METHOD (LowBitsReadZeroOnTheIIe)
    {
        TestMachine       machine ("Apple2e", TestMachine::Slots::Empty);
        FakeTapeDeckPort  fake;



        AttachFake (machine, fake);
        fake.level = true;

        Assert::AreEqual (kBit7, machine.GetMemoryBus().ReadByte (CassettePort::kInputAddress));
        Assert::AreEqual (kBit7, machine.GetMemoryBus().ReadByte (CassettePort::kInputMirrorAddress));
    }


    TEST_METHOD (EveryC02xAccessTogglesTheOutput)
    {
        size_t  checked = 0;



        for (const char * pszModel : s_kpszCassetteModels)
        {
            TestMachine       machine (pszModel, TestMachine::Slots::Empty);
            FakeTapeDeckPort  fake;
            CassettePort    * port    = machine.GetRefs().cassettePort;
            bool              before  = false;



            AttachFake (machine, fake);
            before = port->GetOutputLevel();

            for (Word address = CassettePort::kFirstOutputAddress; address <= CassettePort::kLastOutputAddress; address++)
            {
                machine.GetMemoryBus().ReadByte  (address);
                machine.GetMemoryBus().WriteByte (address, 0);
            }

            Assert::AreEqual (32, fake.toggles, ToWide (pszModel).c_str());
            Assert::AreEqual (before, port->GetOutputLevel(), ToWide (pszModel).c_str());

            machine.GetMemoryBus().ReadByte (CassettePort::kFirstOutputAddress);
            Assert::AreNotEqual (before, port->GetOutputLevel(), ToWide (pszModel).c_str());
            checked++;
        }

        Assert::AreEqual (size_t (4), checked);
    }


    TEST_METHOD (InputReadsLowWithNothingPlugged)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);



        Assert::AreEqual (Byte (0), (Byte) (machine.GetMemoryBus().ReadByte (CassettePort::kInputAddress) & kBit7));
    }


    TEST_METHOD (ButtonsStillWorkBesideTheCassetteInput)
    {
        TestMachine       iiPlus ("Apple2Plus", TestMachine::Slots::Empty);
        TestMachine       iie    ("Apple2e",    TestMachine::Slots::Empty);
        FakeTapeDeckPort  fakePlus;
        FakeTapeDeckPort  fakeIie;



        AttachFake (iiPlus, fakePlus);
        AttachFake (iie,    fakeIie);

        iiPlus.GetRefs().gamePort->SetButton (0, true);
        Assert::AreEqual (kBit7, (Byte) (iiPlus.GetMemoryBus().ReadByte (0xC061) & kBit7));
        Assert::AreEqual (0, fakePlus.reads);

        iie.GetRefs().iieKeyboard->SetOpenApple (true);
        Assert::AreEqual (kBit7, (Byte) (iie.GetMemoryBus().ReadByte (0xC061) & kBit7));
        Assert::AreEqual (0, fakeIie.reads);
    }


    TEST_METHOD (TheIIcHasNoCassettePort)
    {
        TestMachine  machine ("Apple2c", TestMachine::Slots::Empty);



        Assert::IsNull  (machine.GetRefs().cassettePort);
        Assert::IsFalse (MachineDefinitions::Find ("Apple2c")->hasCassettePort);
    }


    TEST_METHOD (EveryOtherModelHasACassettePort)
    {
        for (const char * pszModel : s_kpszCassetteModels)
        {
            Assert::IsTrue (MachineDefinitions::Find (pszModel)->hasCassettePort, ToWide (pszModel).c_str());
        }
    }


    static std::wstring ToWide (const char * pszText)
    {
        std::string  text (pszText);



        return std::wstring (text.begin(), text.end());
    }
};
