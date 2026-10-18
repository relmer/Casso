#include "Pch.h"
#include "Core/StateReader.h"
#include "EmuTests/ReverseSessionRig.h"
#include "EmuTests/ThreadAllocationCounter.h"
#include "Devices/IInputEventSink.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ObserverSlotTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CountingSink
    //
    //  Counts the Disk II and input events reported to it, by kind.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CountingSink : public IDisk2EventSink, public IInputEventSink
    {
    public:
        int  disk         = 0;
        int  keyReads     = 0;
        int  strobes      = 0;
        int  buttonReads  = 0;
        int  paddleReads  = 0;
        int  triggers     = 0;
        int  hostKeys     = 0;
        int  autoRepeats  = 0;

        int  GetGuestReads() const { return keyReads + strobes + buttonReads + paddleReads + triggers; }

        // IDisk2EventSink
        void OnMotorCommandOn() override                    { disk++; }
        void OnMotorEngaged() override                    { disk++; }
        void OnMotorCommandOff() override                    { disk++; }
        void OnMotorDisengaged() override                    { disk++; }
        void OnHeadStep        (int, int) override            { disk++; }
        void OnHeadBump        (int) override                 { disk++; }
        void OnAddressMark     (int, int, int) override       { disk++; }
        void OnDataMarkRead    (int, int, int, std::span<const Byte>) override  { disk++; }
        void OnDataMarkWrite   (int, int, int, std::span<const Byte>) override  { disk++; }
        void OnDriveSelect     (int) override                 { disk++; }
        void OnDiskInserted    (int) override                 { disk++; }
        void OnDiskEjected     (int) override                 { disk++; }

        // IInputEventSink
        void OnKbdDataRead    (Word, Byte, bool) override  { keyReads++; }
        void OnKbdStrobe      (Word, Byte, bool) override  { strobes++; }
        void OnButtonRead     (Word, Byte) override        { buttonReads++; }
        void OnPaddleTrigger  (Word) override              { triggers++; }
        void OnPaddleRead     (Word, Byte) override        { paddleReads++; }
        void OnHostAutoRepeat (Byte) override              { autoRepeats++; }
        void OnHostKeyDown    (Byte) override              { hostKeys++; }
        void OnHostKeyUp      (Byte) override              { hostKeys++; }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ObserverSlotTests
    //
    //  The Disk II and input event feeds' two listeners: both receive what the
    //  machine does, host input goes to the Panel alone, the read edges are
    //  saved with the machine's state, and a polling loop costs one call per
    //  change and no allocation.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ObserverSlotTests)
    {
    public:

        static constexpr size_t  kSlices      = 60;
        static constexpr size_t  kSliceCycles = 17030;


        //  The rig's loop: it polls the keyboard and a paddle, reads the
        //  disk's data latch with the motor on, and writes to the disk.
        static void  RunRig (TestMachine & machine)
        {
            size_t  slice = 0;
            size_t  spent = 0;



            for (slice = 0; slice < kSlices; slice++)
            {
                ReverseSessionRig::Feed (machine, slice);

                for (spent = 0; spent < kSliceCycles; )
                {
                    spent += machine.StepOne();
                }
            }
        }


        TEST_METHOD (BothSlotsHearWhatTheMachineDoes)
        {
            TestMachine   machine { "Apple2e" };
            CountingSink  panel;
            CountingSink  history;



            ReverseSessionRig::Prepare (machine);
            machine.AttachObservers ({ &panel,   &panel },   ObserverSlot::Panel);
            machine.AttachObservers ({ &history, &history }, ObserverSlot::History);

            RunRig (machine);

            Assert::IsTrue   (panel.disk > 0,              L"the motor came on and the head read");
            Assert::IsTrue   (panel.GetGuestReads() > 0,   L"the loop polled the keyboard and paddle");
            Assert::AreEqual (panel.disk,              history.disk);
            Assert::AreEqual (panel.GetGuestReads(),   history.GetGuestReads());
        }


        TEST_METHOD (HostInputGoesToThePanelAlone)
        {
            TestMachine   machine { "Apple2e" };
            CountingSink  panel;
            CountingSink  history;



            ReverseSessionRig::Prepare (machine);
            machine.AttachObservers ({ &panel,   &panel },   ObserverSlot::Panel);
            machine.AttachObservers ({ &history, &history }, ObserverSlot::History);

            machine.GetRefs().keyboard->BeginKeyRepeat ('A');
            machine.GetRefs().keyboard->EndKeyRepeat();

            Assert::AreEqual (2, panel.hostKeys);
            Assert::AreEqual (0, history.hostKeys, L"a replay takes host keys from the input journal");
        }


        //  A state saved after a read loads with that read's value as the
        //  last reported, so the same value read again is not reported; one
        //  saved before any read loads with none reported.
        TEST_METHOD (TheReadEdgesAreSavedWithTheState)
        {
            TestMachine        machine { "Apple2e" };
            CountingSink       history;
            std::vector<Byte>  unread;
            std::vector<Byte>  read;



            unread = ReverseSessionRig::Save (machine);

            machine.AttachObservers ({ nullptr, &history }, ObserverSlot::History);

            (void) machine.GetMemoryBus().ReadByte (0xC000);
            (void) machine.GetMemoryBus().ReadByte (0xC000);
            Assert::AreEqual (1, history.keyReads, L"a second read of the same value is not reported");

            read = ReverseSessionRig::Save (machine);
            Load (machine, read);

            (void) machine.GetMemoryBus().ReadByte (0xC000);
            Assert::AreEqual (1, history.keyReads, L"nor is it after a load of the state saved after the read");

            Load (machine, unread);

            (void) machine.GetMemoryBus().ReadByte (0xC000);
            Assert::AreEqual (2, history.keyReads, L"the first read after a load of one saved before any read is");
        }


        static void  Load (TestMachine & machine, const std::vector<Byte> & state)
        {
            StateReader  reader (state);
            HRESULT      hr     = machine.LoadState (reader);



            Assert::AreEqual (S_OK, hr, L"LoadState");
        }


        //  A loop polling the keyboard, a button and a paddle with both slots
        //  listening: each source is reported when first read and when its
        //  value changes, which here is once more for the paddle, whose timer
        //  bit drops when the one-shot started at power-on runs out. The loop
        //  allocates nothing. The emits take no lock and make no call for a
        //  read of the value last reported, so a poll costs a compare.
        TEST_METHOD (APollingLoopCostsOneCallPerChangeAndNoAllocation)
        {
            static constexpr Byte  kPoll[] =
            {
                0xAD, 0x00, 0xC0,       // 0300  LDA $C000
                0xAD, 0x61, 0xC0,       // 0303  LDA $C061
                0xAD, 0x64, 0xC0,       // 0306  LDA $C064
                0x4C, 0x00, 0x03,       // 0309  JMP $0300
            };
            static constexpr size_t  kInstructions = 100'000;
            TestMachine              machine { "Apple2e" };
            CountingSink             panel;
            CountingSink             history;
            ThreadAllocationCounter  counter;
            uint64_t                 allocated = 0;
            size_t                   i         = 0;



            for (i = 0; i < std::size (kPoll); i++)
            {
                machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), kPoll[i]);
            }

            machine.GetCpu()->SetPC (0x0300);
            machine.AttachObservers ({ &panel,   &panel },   ObserverSlot::Panel);
            machine.AttachObservers ({ &history, &history }, ObserverSlot::History);

            counter.Start();

            for (i = 0; i < kInstructions; i++)
            {
                (void) machine.StepOne();
            }

            allocated = counter.Stop();

            Assert::AreEqual (1, history.keyReads,    L"the keyboard, once");
            Assert::AreEqual (1, history.buttonReads, L"the button, once");
            Assert::AreEqual (2, history.paddleReads, L"the paddle, then its timer running out");
            Assert::AreEqual (4, panel.GetGuestReads());

            if (ThreadAllocationCounter::IsAvailable())
            {
                Assert::AreEqual<uint64_t> (0, allocated, L"no allocation in the loop");
            }
        }
    };
}
