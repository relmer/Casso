#include "Pch.h"
#include "Core/TextEncoding.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace IoPeekTests
{
    //  Every peekable switch of the I/O page, on each kind of machine.
    static constexpr Word  s_kPeekable[] =
    {
        0xC000, 0xC005, 0xC00F,
        0xC011, 0xC012, 0xC013, 0xC014, 0xC015, 0xC016, 0xC017,
        0xC018, 0xC019, 0xC01A, 0xC01B, 0xC01C, 0xC01D, 0xC01E, 0xC01F,
        0xC061, 0xC062, 0xC063,
        0xC064, 0xC065, 0xC066, 0xC067,
    };

    //  Switches that a read operates: the strobe clear, the speaker, the
    //  video switches, the annunciators, PTRIG and the Disk II.
    static constexpr Word  s_kOperated[] =
    {
        0xC010, 0xC030, 0xC050, 0xC054, 0xC058, 0xC05E, 0xC070, 0xC07F, 0xC0E9,
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetInputs
    //
    //  A key waiting, the first button down and the paddles at their ends, so
    //  the peeks have bytes other than zero to return.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void  SetInputs (TestMachine & machine)
    {
        const MachineRefs  & refs = machine.GetRefs();



        Assert::IsNotNull (refs.keyboard);
        refs.keyboard->PressKey ('A');

        if (refs.iieKeyboard != nullptr)
        {
            refs.iieKeyboard->SetOpenApple (true);
        }

        if (refs.iieSoftSwitches != nullptr)
        {
            refs.iieSoftSwitches->SetPaddle (0, 255);
            refs.iieSoftSwitches->SetPaddle (1, 0);
        }

        if (refs.gamePort != nullptr)
        {
            refs.gamePort->SetButton (0, true);
            refs.gamePort->SetPaddle (0, 255);
            refs.gamePort->SetPaddle (1, 0);
        }
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IoPeekTests
    //
    //  A peek returns the byte a read returns, and a switch a read operates
    //  has no peek.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IoPeekTests)
    {
    public:

        TEST_METHOD (EachPeekReturnsTheByteAReadReturns)
        {
            for (const char * id : { "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" })
            {
                TestMachine  machine (id, TestMachine::Slots::AsShipped);
                MemoryBus  & bus     = machine.GetMemoryBus();
                Byte         peeked  = 0;
                Byte         read    = 0;
                int          peeks   = 0;



                machine.PowerCycle();
                SetInputs (machine);

                for (Word address : s_kPeekable)
                {
                    if (!bus.TryPeekIo (address, peeked))
                    {
                        continue;
                    }

                    read = bus.ReadByte (address);
                    peeks++;

                    Assert::AreEqual ((int) read, (int) peeked, std::format (L"{} ${:04X}", TextEncoding::NarrowToWide (id), address).c_str());
                }

                Assert::IsTrue (peeks > 0, TextEncoding::NarrowToWide (id).c_str());
                Assert::IsTrue (bus.TryPeekIo (0xC000, peeked), L"the keyboard data has a peek");
                Assert::AreEqual ((int) (Byte) ('A' | 0x80), (int) peeked, L"with the key and its strobe");
            }
        }


        TEST_METHOD (ASwitchAReadOperatesHasNoPeek)
        {
            for (const char * id : { "Apple2Plus", "Apple2e", "Apple2c" })
            {
                TestMachine  machine (id, TestMachine::Slots::AsShipped);
                MemoryBus  & bus     = machine.GetMemoryBus();
                Byte         peeked  = 0;



                machine.PowerCycle();

                for (Word address : s_kOperated)
                {
                    Assert::IsFalse (bus.TryPeekIo (address, peeked), std::format (L"{} ${:04X}", TextEncoding::NarrowToWide (id), address).c_str());
                }
            }
        }


        TEST_METHOD (APeekChangesNothing)
        {
            TestMachine  machine ("Apple2e", TestMachine::Slots::AsShipped);
            MemoryBus  & bus     = machine.GetMemoryBus();
            Byte         peeked  = 0;
            uint64_t     trigger = 0;



            machine.PowerCycle();
            SetInputs (machine);

            trigger = machine.GetRefs().iieSoftSwitches->GetPaddleTriggerCycle();

            for (Word address = 0xC000; address <= 0xC0FF; address++)
            {
                (void) bus.TryPeekIo (address, peeked);
            }

            Assert::IsFalse  (machine.GetRefs().keyboard->IsStrobeClear(), L"the strobe is set");
            Assert::AreEqual (trigger, machine.GetRefs().iieSoftSwitches->GetPaddleTriggerCycle(), L"no paddle timer is armed");
        }
    };
}
