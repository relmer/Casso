#include "Pch.h"

#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/TapeTurboGovernor.h"
#include "KeystrokeInjector.h"
#include "Shell/CpuManager.h"
#include "TapeTestEncoder.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTurboGovernorTests
//
//  When a tape load may run at Maximum speed. A 1 MHz clock makes the access
//  window exactly 100,000 cycles.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeTurboGovernorTests)
{
public:

    static constexpr double    kClock  = 1000000.0;
    static constexpr uint64_t  kWindow = 100000;


    static void Load (TapeDeck & deck, bool isWritable = true)
    {
        TapeImage  image;



        image.isWritable           = isWritable;
        image.signal.sampleRate    = 1000000;
        image.signal.lengthSamples = 10000000;
        image.signal.transitions   = { 10.5, 20.5 };
        deck.SetCpuClock (kClock);
        deck.Insert (std::move (image));
    }


    TEST_METHOD (OnWhilePlayingAndTheGuestReadsTheTape)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.ReadInputLevel (5000);

        Assert::IsTrue (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, 5000,           kClock));
        Assert::IsTrue (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, 5000 + kWindow, kClock));
    }


    TEST_METHOD (OffAfterATenthOfASecondWithoutAccess)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.ReadInputLevel (5000);

        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, 5000 + kWindow + 1, kClock));
    }


    TEST_METHOD (OffWhenThePreferenceIsOff)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.ReadInputLevel (5000);

        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (false, deck, 5000, kClock));
    }


    TEST_METHOD (OffOnStopEjectAndEndOfTape)
    {
        TapeDeck  stopped;
        TapeDeck  ejected;
        TapeDeck  ended;



        Load (stopped);
        stopped.Play (0);
        stopped.ReadInputLevel (5000);
        stopped.Stop (5001);
        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, stopped, 5002, kClock));

        Load (ejected);
        ejected.Play (0);
        ejected.ReadInputLevel (5000);
        ejected.Eject (5001);
        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, ejected, 5002, kClock));

        Load (ended);
        ended.Play (0);
        ended.ReadInputLevel (9999990);
        ended.Update (10000001);
        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, ended, 10000002, kClock));
    }


    //  The Monitor's wait over a leader: read once, then untouched for
    //  seconds while the leader plays. The speed holds over the leader for
    //  up to four seconds after that read, and never once the data is
    //  under the head.
    TEST_METHOD (HoldsOverALeaderAfterARead)
    {
        TapeDeck             deck;
        TapeImage            image;
        TapeEncodeOptions    options;
        std::vector<double>  halves;
        std::vector<Byte>    data (16, 0xA5);
        double               at       = 0.0;
        uint64_t             dataUs   = 0;
        uint64_t             readAt   = 100000;



        options.leaderSeconds = 6.0;
        TapeTestEncoder::AppendRecord (halves, data, options);

        image.signal.sampleRate = 1000000;

        for (double us : halves)
        {
            at += us;
            image.signal.transitions.push_back (at);
        }

        image.signal.lengthSamples = (uint64_t) at + 1000000;
        dataUs                     = (uint64_t) (options.leaderSeconds * 1000000.0);

        deck.SetCpuClock (kClock);
        deck.Insert (std::move (image));
        deck.Play (0);
        deck.ReadInputLevel (readAt);

        Assert::IsTrue  (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, readAt + 2000000, kClock), L"waiting over the leader");
        Assert::IsTrue  (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, readAt + 3900000, kClock));
        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, readAt + 4100000, kClock), L"four seconds unread");

        deck.ReadInputLevel (dataUs + 50000);
        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, dataUs + 50000 + kWindow + 1000, kClock),
                         L"the data is not a leader");
    }


    TEST_METHOD (OnWhileRecordingAndTheGuestWrites)
    {
        TapeDeck  deck;



        Load (deck);
        deck.SetRecordArmed (true);
        deck.Play (0);
        deck.OnOutputToggle (3000);

        Assert::IsTrue (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, 3500, kClock));
    }


    TEST_METHOD (PollingAStoppedDeckNeverTurnsItOn)
    {
        TapeDeck  deck;



        Load (deck);
        deck.ReadInputLevel (5000);

        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, 5000, kClock));
    }


    TEST_METHOD (PlayingWithNoAccessYetStaysOff)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);

        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, deck, 10, kClock));
    }


    //  A //e game polling its buttons ($C061-$C063) and the shared $C060 line
    //  with no tape playing runs at normal speed.
    TEST_METHOD (IIeButtonPollingWithNoTapeStaysOff)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);
        uint64_t     now     = 0;



        for (int i = 0; i < 1000; i++)
        {
            machine.GetMemoryBus().ReadByte (0xC060);
            machine.GetMemoryBus().ReadByte (0xC061);
            machine.GetMemoryBus().ReadByte (0xC062);
            machine.GetMemoryBus().ReadByte (0xC063);
        }

        now = *machine.GetCpu()->GetBusCyclePtr();

        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, machine.GetTapeDeck(), now, kClock));
        Assert::IsFalse (machine.GetTapeDeck().HasBeenAccessed());
    }


    //  The governor sampled through a real ROM load: on once the Monitor's
    //  READ starts reading, off again once the tape has run out.
    TEST_METHOD (RealLoadEngagesAndReleases)
    {
        constexpr uint64_t    kSlice     = 1023;
        TestMachine           machine ("Apple2Plus", TestMachine::Slots::Empty);
        std::vector<Byte>     data (64, 0x5A);
        std::vector<Byte>     wav;
        TapeEncodeOptions     options;
        NullTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        double                clock      = 0.0;
        bool                  wasOn      = false;
        uint64_t              cycles     = 0;
        uint64_t              total      = 0;



        TapeTestEncoder::MakeWav ({ data }, options, wav);
        Assert::AreEqual (S_OK, TapeImageLoader::Load (wav, "t.wav", false, mp3, image, error));

        machine.PowerCycle();
        machine.RunCycles (5'000'000);
        KeystrokeInjector::InjectLine (machine, "CALL -151", 2'000'000);
        machine.GetTapeDeck().Insert (std::move (image));
        KeystrokeInjector::InjectLine (machine, "800.83FR", 1'000'000);

        clock = (double) machine.GetConfig().clockSpeed;
        total = (uint64_t) (clock * 8.0);
        machine.GetTapeDeck().Play (*machine.GetCpu()->GetBusCyclePtr());

        for (cycles = 0; cycles < total; cycles += kSlice)
        {
            machine.RunCycles (kSlice);
            machine.GetTapeDeck().Update (*machine.GetCpu()->GetBusCyclePtr());
            wasOn = wasOn || TapeTurboGovernor::ShouldRunAtMaximum (true, machine.GetTapeDeck(), *machine.GetCpu()->GetBusCyclePtr(), clock);
        }

        Assert::IsTrue  (wasOn, L"the override engaged during the load");
        Assert::IsFalse (TapeTurboGovernor::ShouldRunAtMaximum (true, machine.GetTapeDeck(), *machine.GetCpu()->GetBusCyclePtr(), clock),
                         L"and released after the tape ran out");
    }


    TEST_METHOD (CpuOverrideShowsMaximumAndLeavesTheUserSpeed)
    {
        CpuManager  cpu;



        cpu.SetSpeedMode (SpeedMode::Double);
        cpu.SetMaximumOverride (true);

        Assert::IsTrue (cpu.GetEffectiveSpeedMode() == SpeedMode::Maximum);
        Assert::IsTrue (cpu.GetSpeedMode() == SpeedMode::Double, L"the user's choice is never written");

        cpu.SetMaximumOverride (false);
        Assert::IsTrue (cpu.GetEffectiveSpeedMode() == SpeedMode::Double);
    }
};
