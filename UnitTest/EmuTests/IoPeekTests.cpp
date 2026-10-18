#include "Pch.h"
#include "Core/TextEncoding.h"
#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeDeck.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "EmuTests/TapeTestEncoder.h"
#include "Machines/Apple2/Common/CassettePort.h"
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


        //  With a tape playing, a peek at the cassette's output toggle or its
        //  input marks no access, toggles nothing into a recording, and moves
        //  nothing; the input's peek has the tape's level at that cycle.
        TEST_METHOD (APeekLeavesThePlayingTapeAlone)
        {
            TestMachine            machine ("Apple2e", TestMachine::Slots::Empty);
            MemoryBus            & bus     = machine.GetMemoryBus();
            TapeDeck             & deck    = machine.GetTapeDeck();
            NullTapeAudioDecoder   mp3;
            TapeImage              image;
            std::vector<Byte>      wav;
            std::string            error;
            Byte                   peeked  = 0;
            uint64_t               now     = 0;
            HRESULT                hr      = S_OK;



            machine.PowerCycle();

            TapeTestEncoder::MakeWav ({ std::vector<Byte> (64, 0x55) }, TapeEncodeOptions(), wav);
            hr = TapeImageLoader::Load (wav, "test.wav", false, mp3, image, error);
            Assert::AreEqual (S_OK, hr, TextEncoding::NarrowToWide (error).c_str());

            deck.Insert (std::move (image));
            now = *machine.GetCpu()->GetBusCyclePtr();
            deck.Play (now);

            for (Word address : { (Word) 0xC020, (Word) 0xC02F, (Word) 0xC060, (Word) 0xC068 })
            {
                (void) bus.TryPeekIo (address, peeked);
            }

            Assert::IsFalse  (deck.HasBeenAccessed(),               L"no access is marked");
            Assert::IsTrue   (deck.GetCapturedToggles().empty(),    L"no toggle is captured");
            Assert::IsTrue   (deck.GetTransport() == TapeTransport::Playing);

            (void) bus.TryPeekIo (0xC060, peeked);
            Assert::AreEqual (deck.PeekLevel (now), (peeked & CassettePort::kInputBit) != 0, L"the tape's level at that cycle");
        }
    };
}
