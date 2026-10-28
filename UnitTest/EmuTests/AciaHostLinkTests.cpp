#include "Pch.h"

#include "Debugger/Events/ICommEventSink.h"
#include "Debugger/IoEventLog.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Devices/Acia6551.h"
#include "Devices/IAciaEndpoint.h"
#include "Devices/Serial/SerialLineFormat.h"
#include "Shell/HostInputGate.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AciaHostLinkTestNs
{
    static constexpr Word  kBase      = 0xC0A8;
    static constexpr Word  kAddrData  = 0xC0A8;
    static constexpr Word  kAddrCmd   = 0xC0AA;
    static constexpr Word  kAddrCtrl  = 0xC0AB;
    static constexpr Byte  kCmdPolled = Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable | Acia6551::kCommandTicTxIrqOff;
    static constexpr Byte  kCtrl9600  = 0x1E;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  ScriptedHostLink
    //
    //  A host link whose next poll delivers what the test set up: bytes with
    //  errors, line levels, a connection. Its members list each poll's cycle
    //  count and what the chip passed to it.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ScriptedHostLink : public IAciaEndpoint
    {
    public:
        struct Arrival
        {
            Byte  value  = 0;
            Byte  errors = 0;
        };

        explicit ScriptedHostLink (Acia6551 & acia) : m_acia (acia) {}

        void  OnByteTransmitted (Byte value) override                   { sent.push_back (value); }
        void  OnFormatChanged   (const SerialLineFormat & format) override { formats.push_back (format); }
        bool  IsHostLink        () const override                       { return true; }

        bool  Poll (uint64_t cycles) override
        {
            bool  isBusy = !bytes.empty() || hasLines || hasConnection;



            polls.push_back (cycles);

            for (const Arrival & each : bytes)
            {
                m_acia.ReceiveFromHost (each.value, each.errors);
            }

            if (hasLines)
            {
                m_acia.SetHostLines (cts, dsr, dcd);
            }

            if (hasConnection)
            {
                m_acia.NoteHostConnection (isOpen, "host:23");
            }

            bytes.clear();
            hasLines      = false;
            hasConnection = false;

            return isBusy;
        }

        std::vector<Arrival>           bytes;
        bool                           hasLines      = false;
        bool                           cts           = true;
        bool                           dsr           = true;
        bool                           dcd           = true;
        bool                           hasConnection = false;
        bool                           isOpen        = false;
        std::vector<uint64_t>          polls;
        std::vector<Byte>              sent;
        std::vector<SerialLineFormat>  formats;

    private:
        Acia6551  & m_acia;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  CommEventRecorder
    //
    ////////////////////////////////////////////////////////////////////////////

    class CommEventRecorder : public ICommEventSink
    {
    public:
        void  OnCommEvent (const IoEvent & event, std::span<const Byte> bytes) override
        {
            events.push_back (event);
            texts.push_back (std::string (bytes.begin(), bytes.end()));
        }

        std::vector<IoEvent>      events;
        std::vector<std::string>  texts;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  AciaHostLinkTests
    //
    //  A 6551 with an attachment that brings input from the PC: what it
    //  takes is journaled where it is taken, a replay of the journal leads
    //  to the same guest state at the same instruction, and nothing is
    //  polled or sent behind live.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AciaHostLinkTests)
    {
    public:
        TEST_METHOD (PolledBytesAreJournaledWithTheirErrors)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);
            InputJournal      journal;
            uint64_t          cycle = 1000;



            journal.SetOn (true);
            journal.SetCycleSource (&cycle);
            acia.SetInputJournal (&journal, 1);
            acia.SetEndpoint (&link);

            link.bytes = { { 0x41, 0 }, { 0x42, Acia6551::kRxErrorFraming } };
            cycle     += 4;
            acia.Tick (4);

            Assert::AreEqual<size_t> (2, journal.GetEndIndex() - journal.GetBeginIndex());
            Assert::IsTrue (journal.GetRecord (0).kind == InputKind::AciaByte);
            Assert::AreEqual<Byte>     (0x41, journal.GetRecord (0).value);
            Assert::AreEqual<uint16_t> (1,    journal.GetRecord (0).detail, L"the chip's index");
            Assert::AreEqual<uint64_t> (1000, journal.GetRecord (0).cycle,  L"stamped where the instruction began");
            Assert::IsTrue (journal.GetRecord (0).isObserved);
            Assert::AreEqual<uint64_t> (Acia6551::kRxErrorFraming, journal.GetRecord (1).data, L"the errors are in the data");
            Assert::AreEqual<size_t>   (2, acia.GetQueuedReceiveCount());
        }


        TEST_METHOD (ALineChangeIsJournaledOnceAndAppliedInTheSameTick)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);
            InputJournal      journal;
            uint64_t          cycle = 0;



            journal.SetOn (true);
            journal.SetCycleSource (&cycle);
            acia.SetInputJournal (&journal, 0);
            acia.SetEndpoint (&link);

            link.hasLines = true;
            link.dcd      = false;
            acia.Tick (2);

            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusDcd) != 0, L"DCD off reads as a 1");
            Assert::AreEqual<size_t> (1, journal.GetEndIndex() - journal.GetBeginIndex());
            Assert::IsTrue (journal.GetRecord (0).kind == InputKind::AciaLines);
            Assert::AreEqual<Byte> (IoEventLog::kLineCts | IoEventLog::kLineDsr, journal.GetRecord (0).value);

            link.hasLines = true;
            acia.Tick (Acia6551::kActivePollCycles);

            Assert::AreEqual<size_t> (1, journal.GetEndIndex() - journal.GetBeginIndex(), L"the same lines are not journaled again");
        }


        TEST_METHOD (AReplayedLineChangeIsAppliedInTheTickLiveAppliedIt)
        {
            Acia6551          live     (kBase);
            Acia6551          replayed (kBase);
            ScriptedHostLink  link     (live);
            InputJournal      journal;
            uint64_t          cycle    = 500;



            journal.SetOn (true);
            journal.SetCycleSource (&cycle);
            live.SetInputJournal (&journal, 0);
            live.SetEndpoint (&link);

            link.hasLines = true;
            link.dsr      = false;
            cycle        += 3;
            live.Tick (3);

            Assert::IsTrue (replayed.ApplyInput (journal.GetRecord (0)));
            Assert::AreEqual (live.GetStatus() & ~Acia6551::kStatusDsr, (int) replayed.GetStatus(),
                              L"before the instruction's tick, the replay reads the lines as they were");

            replayed.Tick (3);
            Assert::AreEqual (live.GetStatus(), replayed.GetStatus(), L"after it, as live does");
        }


        TEST_METHOD (NothingIsPolledWhileTheGateIsHeld)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);
            HostInputGate     gate;
            int               i    = 0;



            acia.SetHostInputGate (&gate);
            acia.SetEndpoint (&link);
            gate.Hold();

            for (i = 0; i < 100; i++)
            {
                acia.Tick (Acia6551::kIdlePollCycles);
            }

            Assert::AreEqual<size_t> (0, link.polls.size(), L"behind live the journal supplies the input");

            gate.Release();
            acia.Tick (Acia6551::kActivePollCycles);

            Assert::AreEqual<size_t> (1, link.polls.size(), L"live again, the link is polled within an active interval");
        }


        TEST_METHOD (APollWithTrafficShortensTheInterval)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);
            uint64_t          i    = 0;



            acia.SetEndpoint (&link);

            for (i = 0; i < Acia6551::kIdlePollCycles * 4; i++)
            {
                acia.Tick (1);
            }

            Assert::AreEqual<size_t> (4, link.polls.size(), L"an idle link: one poll per idle interval");

            link.bytes = { { 1, 0 } };
            acia.Tick (Acia6551::kIdlePollCycles);
            link.polls.clear();

            for (i = 0; i < Acia6551::kActivePollCycles; i++)
            {
                acia.Tick (1);
            }

            Assert::AreEqual<size_t> (1, link.polls.size(), L"after traffic, the next poll is one active interval later");
        }


        TEST_METHOD (APollIsGivenTheElapsedCycles)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);



            acia.SetEndpoint (&link);
            acia.Tick (7);

            Assert::AreEqual<size_t>   (1, link.polls.size());
            Assert::AreEqual<uint64_t> (7, link.polls[0]);
            Assert::AreEqual<uint64_t> (7, acia.GetElapsedCycles());
        }


        TEST_METHOD (AMutedChipSendsNothingToAHostLink)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);



            acia.SetEndpoint (&link);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            acia.SetOutputMuted (true);

            acia.Write (kAddrData, 0x55);
            acia.Tick (100000);

            Assert::AreEqual<size_t> (0, link.sent.size());
        }


        TEST_METHOD (TheFormatIsPassedOnAttachAndOnEachChange)
        {
            Acia6551          acia (kBase);
            ScriptedHostLink  link (acia);



            acia.SetEndpoint (&link);
            Assert::AreEqual<size_t> (1, link.formats.size(), L"on attach");

            acia.Write (kAddrCtrl, kCtrl9600);
            Assert::AreEqual<size_t> (2, link.formats.size());
            Assert::AreEqual (9600.0, link.formats.back().baud, 0.001);

            acia.Write (kAddrCtrl, kCtrl9600);
            Assert::AreEqual<size_t> (2, link.formats.size(), L"the same format is not passed again");

            acia.Write (kAddrCmd, Acia6551::kCommandParityEnable | Acia6551::kCommandDtr);
            Assert::AreEqual<size_t> (3, link.formats.size());
            Assert::IsTrue (link.formats.back().parity == SerialParity::Odd);

            acia.Write (kAddrCmd, Acia6551::kCommandParityEnable);
            Assert::AreEqual<size_t> (3, link.formats.size(), L"DTR is not part of the format");
        }


        TEST_METHOD (AConnectionIsJournaledAndLogged)
        {
            Acia6551           acia (kBase);
            ScriptedHostLink   link (acia);
            InputJournal       journal;
            CommEventRecorder  recorder;
            Acia6551           replayed (kBase);
            CommEventRecorder  replayedRecorder;
            uint64_t           cycle = 0;



            journal.SetOn (true);
            journal.SetCycleSource (&cycle);
            acia.SetInputJournal (&journal, 0);
            acia.SetCommEventSink (&recorder);
            replayed.SetCommEventSink (&replayedRecorder);
            acia.SetEndpoint (&link);

            link.hasConnection = true;
            link.isOpen        = true;
            acia.Tick (1);

            Assert::AreEqual<size_t> (1, journal.GetEndIndex() - journal.GetBeginIndex());
            Assert::IsTrue (journal.GetRecord (0).kind == InputKind::AciaConnection);
            Assert::AreEqual<Byte> (1, journal.GetRecord (0).value);
            Assert::AreEqual (std::string ("host:23"), journal.GetRecord (0).payload);

            Assert::AreEqual<size_t> (1, recorder.events.size());
            Assert::IsTrue (recorder.events[0].kind == IoEventKind::SerialConnection);
            Assert::AreEqual (std::string ("host:23"), recorder.texts[0]);

            Assert::IsTrue (replayed.ApplyInput (journal.GetRecord (0)));
            Assert::AreEqual<size_t> (1, replayedRecorder.events.size(), L"a replay logs the connection too");
            Assert::AreEqual (std::string ("host:23"), replayedRecorder.texts[0]);
        }


        TEST_METHOD (TheNowMethodsAreNotJournaled)
        {
            Acia6551      acia (kBase);
            InputJournal  journal;
            uint64_t      cycle = 0;



            journal.SetOn (true);
            journal.SetCycleSource (&cycle);
            acia.SetInputJournal (&journal, 0);

            acia.ApplyHostLinesNow  (true, false, false);
            acia.ReceiveFromHostNow ('N', 0);

            Assert::AreEqual<size_t> (0, journal.GetEndIndex() - journal.GetBeginIndex());
            Assert::IsTrue ((acia.GetStatus() & (Acia6551::kStatusDcd | Acia6551::kStatusDsr)) == (Acia6551::kStatusDcd | Acia6551::kStatusDsr),
                            L"applied at once");
            Assert::AreEqual<size_t> (1, acia.GetQueuedReceiveCount());
        }
    };
}
