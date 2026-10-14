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

        //  Every address of the I/O page and the slot pages has a peek, and
        //  each peek is the byte the read made just after it returns, on
        //  every kind of machine.
        TEST_METHOD (EachPeekReturnsTheByteAReadReturns)
        {
            for (const char * id : { "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" })
            {
                TestMachine  machine (id, TestMachine::Slots::AsShipped);
                MemoryBus  & bus     = machine.GetMemoryBus();
                Byte         peeked  = 0;
                Byte         read    = 0;



                machine.PowerCycle();
                SetInputs (machine);

                Assert::IsTrue   (bus.TryPeekIo (0xC000, peeked), L"the keyboard data has a peek");
                Assert::AreEqual ((int) (Byte) ('A' | 0x80), (int) peeked, L"with the key and its strobe");

                for (uint32_t address = 0xC000; address <= 0xCFFF; address++)
                {
                    std::wstring  what = std::format (L"{} ${:04X}", TextEncoding::NarrowToWide (id), address);

                    Assert::IsTrue (bus.TryPeekIo ((Word) address, peeked), what.c_str());

                    read = bus.ReadByte ((Word) address);

                    Assert::AreEqual ((int) read, (int) peeked, what.c_str());
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
