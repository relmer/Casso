#include "Pch.h"

#include "Audio/TapeAudioSource.h"
#include "Devices/Tape/TapeDeck.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeckTests
//
//  The clock and the sample rate are both 1 MHz here, so one bus cycle is one
//  sample and positions read directly.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeDeckTests)
{
public:

    static constexpr double    kClock  = 1000000.0;
    static constexpr uint32_t  kRate   = 1000000;
    static constexpr uint64_t  kLength = 1000;


    static void Load (TapeDeck & deck, bool isWritable = true)
    {
        TapeImage  image;



        image.path                 = "tape.wav";
        image.isWritable           = isWritable;
        image.signal.sampleRate    = kRate;
        image.signal.lengthSamples = kLength;
        image.signal.initialLevel  = false;
        image.signal.transitions   = { 100.5, 200.5, 300.5, 400.5 };

        deck.SetCpuClock (kClock);
        deck.Insert (std::move (image));
    }


    TEST_METHOD (StartsEmptyAndInsertStopsAtZero)
    {
        TapeDeck  deck;



        Assert::IsTrue (deck.GetTransport() == TapeTransport::Empty);
        Assert::IsNull (deck.GetImage());

        Load (deck);

        Assert::IsTrue    (deck.GetTransport() == TapeTransport::Stopped);
        Assert::IsNotNull (deck.GetImage());
        Assert::AreEqual  (0.0, deck.GetPositionSamples (12345));
    }


    TEST_METHOD (LevelFollowsTransitionsAtBusCycles)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (5000);

        Assert::IsFalse (deck.ReadInputLevel (5000 + 100));
        Assert::IsTrue  (deck.ReadInputLevel (5000 + 101));
        Assert::IsTrue  (deck.ReadInputLevel (5000 + 200));
        Assert::IsFalse (deck.ReadInputLevel (5000 + 201));
        Assert::IsTrue  (deck.ReadInputLevel (5000 + 350));
        Assert::IsFalse (deck.ReadInputLevel (5000 + 900));
    }


    TEST_METHOD (InitialLevelHighInvertsTheReadings)
    {
        TapeDeck   deck;
        TapeImage  image;



        image.signal.sampleRate    = kRate;
        image.signal.lengthSamples = kLength;
        image.signal.initialLevel  = true;
        image.signal.transitions   = { 100.5 };
        deck.SetCpuClock (kClock);
        deck.Insert (std::move (image));
        deck.Play (0);

        Assert::IsTrue  (deck.ReadInputLevel (50));
        Assert::IsFalse (deck.ReadInputLevel (150));
    }


    TEST_METHOD (StoppedDeckReadsLowAndRecordsNoAccess)
    {
        TapeDeck  deck;



        Load (deck);

        Assert::IsFalse (deck.ReadInputLevel (150));
        Assert::IsFalse (deck.HasBeenAccessed());
    }


    TEST_METHOD (PositionAdvancesOnlyWithCycles)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (1000);

        Assert::AreEqual (0.0,   deck.GetPositionSamples (1000));
        Assert::AreEqual (250.0, deck.GetPositionSamples (1250));

        // A paused machine runs no cycles; asking again at the same cycle
        // gives the same position however much host time has gone by.
        Assert::AreEqual (250.0, deck.GetPositionSamples (1250));
    }


    TEST_METHOD (StopHoldsPositionAndPlayResumesFromIt)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.Stop (150);

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual (150.0, deck.GetPositionSamples (99999));

        deck.Play (10000);
        Assert::IsTrue  (deck.ReadInputLevel (10000));
        Assert::IsFalse (deck.ReadInputLevel (10000 + 60));
    }


    TEST_METHOD (RewindStopsAndReturnsToZero)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        Assert::IsTrue (deck.ReadInputLevel (350));

        deck.Rewind (350);
        Assert::IsTrue (deck.GetTransport() == TapeTransport::Rewinding, L"rewind winds, it does not jump");

        deck.Update (400);   // 350 samples at twenty times the speed is under 18 cycles

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::Stopped, L"and stops at the start");
        Assert::AreEqual (0.0, deck.GetPositionSamples (400));

        deck.Play (1000);
        Assert::IsFalse (deck.ReadInputLevel (1000 + 50));
        Assert::IsTrue  (deck.ReadInputLevel (1000 + 150));
    }


    TEST_METHOD (SeekStopsAtThePositionAndPlaysOnFromThere)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);

        deck.Seek (50, 250.0 / kRate);

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual (250.0, deck.GetPositionSamples (5000), L"stopped, so it holds there");

        deck.Play (1000);
        Assert::IsFalse (deck.ReadInputLevel (1000 + 10), L"two transitions behind it");
        Assert::IsTrue  (deck.ReadInputLevel (1000 + 60), L"the third comes at 300.5");

        deck.Seek (2000, 1.0);
        Assert::AreEqual ((double) kLength, deck.GetPositionSamples (2000), L"past the end winds to the end");
    }


    TEST_METHOD (FastForwardWindsToTheEndAndStops)
    {
        TapeDeck  deck;



        Load (deck);
        deck.FastForward (0);

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::FastForwarding);
        Assert::AreEqual (200.0, deck.GetPositionSamples (10), L"twenty samples a cycle");

        deck.Update (100);

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual ((double) kLength, deck.GetPositionSamples (100));

        deck.FastForward (200);
        Assert::IsTrue (deck.GetTransport() == TapeTransport::Stopped, L"nowhere left to wind");
    }


    TEST_METHOD (RecordStartsByItselfAndStopReleasesIt)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Record (0);
        Assert::IsTrue (deck.GetTransport() == TapeTransport::Recording, L"no Play needed");

        deck.Play (10);
        Assert::IsTrue (deck.GetTransport() == TapeTransport::Recording, L"Play does nothing while recording");

        deck.Stop (100);
        Assert::IsTrue  (deck.GetTransport() == TapeTransport::Stopped);
        Assert::IsFalse (deck.GetSnapshot().isRecordArmed, L"the record key comes back up");
        Assert::IsTrue  (deck.HasPendingRecording());
    }


    TEST_METHOD (IdleStopWaitsForAReadThenStopsWhenReadingEnds)
    {
        TapeDeck  deck;
        uint64_t  idle = (uint64_t) (7.0 * kClock);



        Load (deck);
        deck.SetAutoStop (false);   // the test tape is far shorter than the idle wait
        deck.Play (0);

        deck.Update (idle);
        Assert::IsTrue (deck.GetTransport() == TapeTransport::Playing, L"nothing has read it yet, so it plays on");

        deck.Stop (idle);
        deck.Play (idle);
        deck.ReadInputLevel (idle + 10);
        deck.Update (idle + 10 + idle);

        Assert::IsTrue (deck.GetTransport() == TapeTransport::Stopped, L"read, then left alone, it stops");

        deck.SetIdleStop (false);
        deck.Play (3 * idle);
        deck.ReadInputLevel (3 * idle + 10);
        deck.Update (5 * idle);
        Assert::IsTrue (deck.GetTransport() == TapeTransport::Playing, L"with the setting off, it plays on");
    }


    TEST_METHOD (IdleStopOutlastsTheMonitorsPauseInsideALoad)
    {
        // The Monitor's READ finds the tape's first edge, then waits about
        // 3.5 seconds in HEADR without reading before it reads the record --
        // and a BASIC LOAD calls READ twice. A deck that stopped in that wait
        // left LOAD waiting forever.
        TapeDeck  deck;
        uint64_t  pause = (uint64_t) (3.6 * kClock);



        Load (deck);
        deck.SetAutoStop (false);
        deck.Play (0);
        deck.ReadInputLevel (10);
        deck.Update (10 + pause);

        Assert::IsTrue (deck.GetTransport() == TapeTransport::Playing, L"still playing through the Monitor's pause");
    }


    TEST_METHOD (CursorReseeksWhenPositionGoesBackward)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        Assert::IsFalse (deck.ReadInputLevel (450));

        deck.Stop (450);
        deck.Rewind (450);
        deck.Update (1000);
        deck.Play (0);
        Assert::IsTrue (deck.ReadInputLevel (150));
    }


    TEST_METHOD (EndOfTapeStopsAtTheLength)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.Update (kLength + 500);

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual ((double) kLength, deck.GetPositionSamples (kLength + 900));
    }


    TEST_METHOD (EjectEmptiesTheDeck)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.Eject (100);

        Assert::IsTrue (deck.GetTransport() == TapeTransport::Empty);
        Assert::IsNull (deck.GetImage());
        Assert::IsFalse (deck.ReadInputLevel (150));
    }


    TEST_METHOD (RecordArmsOnlyOnAWritableStoppedTape)
    {
        TapeDeck  writable;
        TapeDeck  protectedDeck;



        Load (writable, true);
        writable.SetRecordArmed (true);
        Assert::IsTrue (writable.GetSnapshot().isRecordArmed);

        Load (protectedDeck, false);
        protectedDeck.SetRecordArmed (true);
        Assert::IsFalse (protectedDeck.GetSnapshot().isRecordArmed);

        writable.SetRecordArmed (false);
        writable.Play (0);
        writable.SetRecordArmed (true);
        Assert::IsFalse (writable.GetSnapshot().isRecordArmed);
    }


    TEST_METHOD (ArmedPlayRecordsAndCapturesToggles)
    {
        TapeDeck  deck;



        Load (deck);
        deck.SetRecordArmed (true);
        deck.Play (2000);

        Assert::IsTrue (deck.GetTransport() == TapeTransport::Recording);

        deck.OnOutputToggle (2100);
        deck.OnOutputToggle (2300);

        Assert::AreEqual (size_t (2), deck.GetCapturedToggles().size());
        Assert::AreEqual (uint64_t (2000), deck.GetRecordStartCycle());
        Assert::AreEqual (0.0, deck.GetRecordStartSample());
        Assert::AreEqual (uint64_t (2300), deck.GetLastAccessCycle());
    }


    TEST_METHOD (TogglesWithoutRecordingAreDiscarded)
    {
        TapeDeck  deck;



        Load (deck);
        deck.Play (0);
        deck.OnOutputToggle (100);

        Assert::IsTrue  (deck.GetCapturedToggles().empty());
        Assert::IsFalse (deck.HasBeenAccessed());
    }


    TEST_METHOD (SnapshotTracksTransportAndLength)
    {
        TapeDeck            deck;
        TapeDeck::Snapshot  snapshot;



        Load (deck);
        deck.Play (0);
        deck.Update (400);
        snapshot = deck.GetSnapshot();

        Assert::IsTrue   (snapshot.transport == TapeTransport::Playing);
        Assert::AreEqual (400.0, snapshot.positionSamples);
        Assert::AreEqual (kLength, snapshot.lengthSamples);
        Assert::AreEqual (kRate, snapshot.sampleRate);
        Assert::IsTrue   (snapshot.isWritable);
    }

    TEST_METHOD (TapeSoundFollowsTheLevelOnlyWhilePlaying)
    {
        TapeDeck         deck;
        TapeAudioSource  source;
        uint64_t         now     = 0;
        float            pcm[4]  = {};



        Load (deck);
        source.Attach (&deck, [&now] () { return now; });

        source.GeneratePCM (pcm, 4);
        Assert::AreEqual (0.0f, pcm[3], L"silent while stopped");

        deck.Play (0);
        now = 400;
        source.GeneratePCM (pcm, 4);

        // Samples land at cycles 100, 200, 300 and 400: low before the first
        // edge at 100.5, then high, low and high.
        Assert::AreEqual (-TapeAudioSource::kAmplitude, pcm[0]);
        Assert::AreEqual ( TapeAudioSource::kAmplitude, pcm[1]);
        Assert::AreEqual (-TapeAudioSource::kAmplitude, pcm[2]);
        Assert::AreEqual ( TapeAudioSource::kAmplitude, pcm[3]);
        Assert::IsFalse  (deck.HasBeenAccessed(), L"listening is not the guest reading the tape");
    }

    TEST_METHOD (WithAutoStopOffTheTapeRunsOnPastTheEnd)
    {
        TapeDeck  deck;



        Load (deck);
        deck.SetAutoStop (false);
        deck.Play (0);
        deck.Update (kLength + 500);

        Assert::IsTrue   (deck.GetTransport() == TapeTransport::Playing);
        Assert::AreEqual ((double) kLength + 500.0, deck.GetPositionSamples (kLength + 500));
    }
};
