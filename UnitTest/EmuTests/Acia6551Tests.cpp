#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/IAciaEndpoint.h"
#include "Core/InterruptController.h"
#include "Core/ComponentRegistry.h"
#include "Core/MemoryBus.h"
#include "Core/MachineConfig.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Events/ICommEventSink.h"
#include "ICpu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace Acia6551TestNs
{
    // Slot-1 register addresses ($C088 + 1*16 = $C098).
    static constexpr Word    kBase       = 0xC098;
    static constexpr Word    kAddrData   = 0xC098;
    static constexpr Word    kAddrStatus = 0xC099;
    static constexpr Word    kAddrCmd    = 0xC09A;
    static constexpr Word    kAddrCtrl   = 0xC09B;

    // DTR on, receiver IRQ off, transmitter on with RTS asserted and its IRQ
    // off: what the //c and SSC firmware program for polled I/O.
    static constexpr Byte    kCmdPolled  = Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable |
                                           Acia6551::kCommandTicTxIrqOff;

    // 9,600 baud, receiver at the same rate, 8 data bits, 1 stop bit.
    static constexpr Byte    kCtrl9600   = 0x1E;
    static constexpr Byte    kCtrl19200  = 0x1F;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  AciaTestCpu
    //
    //  Minimal ICpu double recording the maskable IRQ line state.
    //
    ////////////////////////////////////////////////////////////////////////////

    class AciaTestCpu : public ICpu
    {
    public:
        HRESULT     Reset         () override                     { return S_OK; }
        HRESULT     Step          (uint32_t & outCycles) override { outCycles = 0; return S_OK; }
        uint64_t    GetCycleCount() const override               { return 0; }

        void        SetInterruptLine (CpuInterruptKind kind, bool asserted) override
        {
            if (kind == CpuInterruptKind::kMaskable)
            {
                m_irqAsserted = asserted;
            }
        }

        bool        IrqAsserted() const { return m_irqAsserted; }

    private:
        bool    m_irqAsserted = false;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingEndpoint
    //
    //  Captures every transmitted byte and modem-control change in memory.
    //
    ////////////////////////////////////////////////////////////////////////////

    class RecordingEndpoint : public IAciaEndpoint
    {
    public:
        void    OnByteTransmitted     (Byte value) override       { m_bytes.push_back (value); }
        void    OnModemControlChanged (bool dtr, bool rts) override { m_dtr = dtr; m_rts = rts; ++m_controlCalls; }
        void    OnBreakChanged        (bool active) override      { m_break = active; }

        const vector<Byte> &    Bytes() const { return m_bytes; }

        bool    m_dtr          = false;
        bool    m_rts          = false;
        bool    m_break        = false;
        int     m_controlCalls = 0;

    private:
        vector<Byte>    m_bytes;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  CommEventRecorder
    //
    //  The serial log entries a chip reports.
    //
    ////////////////////////////////////////////////////////////////////////////

    class CommEventRecorder : public ICommEventSink
    {
    public:
        void  OnCommEvent (const IoEvent & event, std::span<const Byte> bytes) override
        {
            (void) bytes;
            m_events.push_back (event);
        }

        bool  HasReceived (Byte value, Byte errors) const
        {
            return std::any_of (m_events.begin(), m_events.end(), [value, errors] (const IoEvent & event)
            {
                return event.kind == IoEventKind::SerialReceived && event.value == value && event.reg == errors;
            });
        }

    private:
        std::vector<IoEvent>  m_events;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  CopyState
    //
    //  One chip's machine state saved and loaded into another.
    //
    ////////////////////////////////////////////////////////////////////////////

    static HRESULT CopyState (const Acia6551 & from, Acia6551 & to)
    {
        HRESULT      hr = S_OK;
        StateWriter  writer;



        hr = from.SaveState (writer);
        CHR (hr);

        {
            StateReader  reader (writer.GetBytes());

            hr = to.LoadState (reader);
            CHR (hr);
        }

    Error:
        return hr;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Acia6551Tests
    //
    //  The 6551 ACIA: the four registers, the status bits, interrupt
    //  generation, and the character timing both directions follow.
    //
    //  The register map is pinned first because two of the four addresses do
    //  DIFFERENT things on read and write. Timing is checked against the
    //  character time the chip reports for its own format, and that figure is
    //  checked separately against the bit count and baud rate, so a wrong
    //  formula cannot hide behind a test that uses it.
    //
    //  Written from the datasheet.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Acia6551Tests)
    {
    public:
        TEST_METHOD (ResetLeavesTransmitterEmpty)
        {
            Acia6551    acia (kBase);



            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusTxEmpty) != 0,
                            L"TDRE must be set after reset");
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusRxFull) == 0,
                            L"RDRF must be clear after reset");
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusIrq) == 0,
                            L"IRQ latch must be clear after reset");
            Assert::AreEqual<Byte> (0, acia.GetCommand());
            Assert::AreEqual<Byte> (0, acia.GetControl());
        }


        TEST_METHOD (AddressWindowIsFourRegisters)
        {
            Acia6551    acia (kBase);



            Assert::AreEqual<Word> (kBase, acia.GetStart());
            Assert::AreEqual<Word> (kAddrCtrl, acia.GetEnd());
        }


        TEST_METHOD (BaudCodeZeroIsCrystalOverSixteen)
        {
            Assert::AreEqual (115200.0, Acia6551::GetBaudRate (0), 1e-9);
            Assert::AreEqual (9600.0,   Acia6551::GetBaudRate (14), 1e-9);
            Assert::AreEqual (19200.0,  Acia6551::GetBaudRate (15), 1e-9);
            Assert::AreEqual (50.0,     Acia6551::GetBaudRate (1), 1e-9);
        }


        TEST_METHOD (CharacterTimeCountsStartDataParityAndStopBits)
        {
            Acia6551    acia (kBase);
            double      clock = static_cast<double> (Acia6551::kDefaultCpuClockHz);



            // 8N1: 10 bits.
            acia.Write (kAddrCtrl, kCtrl9600);
            Assert::AreEqual (clock * 10.0 / 9600.0, acia.GetTransmitCharacterCycles(), 0.01);

            // 7 data bits, parity, 2 stop bits: 1 + 7 + 1 + 2 = 11 bits.
            acia.Write (kAddrCtrl, static_cast<Byte> (kCtrl9600 | 0x20 | Acia6551::kControlStopBits));
            acia.Write (kAddrCmd,  Acia6551::kCommandParityEnable);
            Assert::AreEqual (clock * 11.0 / 9600.0, acia.GetTransmitCharacterCycles(), 0.01);

            // 5 data bits, no parity, "2" stop bits: 1 + 5 + 1.5 = 7.5 bits.
            acia.Write (kAddrCtrl, static_cast<Byte> (kCtrl9600 | 0x60 | Acia6551::kControlStopBits));
            acia.Write (kAddrCmd,  0);
            Assert::AreEqual (clock * 7.5 / 9600.0, acia.GetTransmitCharacterCycles(), 0.01);
        }


        TEST_METHOD (TransmittedByteArrivesOneCharacterTimeLater)
        {
            RecordingEndpoint   endpoint;
            Acia6551            acia (kBase);
            uint32_t            charCycles = 0;



            acia.SetEndpoint (&endpoint);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.Write (kAddrData, 0x41);
            acia.Tick (charCycles - 2);
            Assert::AreEqual<size_t> (0, endpoint.Bytes().size(), L"nothing until the character has shifted out");

            acia.Tick (2);
            Assert::AreEqual<size_t> (1, endpoint.Bytes().size());
            Assert::AreEqual<Byte> (0x41, endpoint.Bytes().front());
        }


        TEST_METHOD (TransmitterIsDoubleBuffered)
        {
            RecordingEndpoint   endpoint;
            Acia6551            acia (kBase);
            uint32_t            charCycles = 0;



            acia.SetEndpoint (&endpoint);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.Write (kAddrData, 0x01);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusTxEmpty) != 0,
                            L"the first byte moves into the idle shift register at once");

            acia.Write (kAddrData, 0x02);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusTxEmpty) == 0,
                            L"the second byte waits in the transmit data register");

            acia.Tick (charCycles);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusTxEmpty) != 0,
                            L"TDRE sets when the first byte finishes and the second moves in");
            Assert::AreEqual<size_t> (1, endpoint.Bytes().size());

            acia.Tick (charCycles);
            Assert::AreEqual<size_t> (2, endpoint.Bytes().size());
            Assert::AreEqual<Byte> (0x02, endpoint.Bytes()[1]);
        }


        TEST_METHOD (TransmitTimingMatchesEveryBaudCode)
        {
            constexpr int     kBytes      = 1000;
            constexpr int     kBaudCodes  = 16;
            constexpr double  kTolerance  = 0.01;



            for (int code = 0; code < kBaudCodes; ++code)
            {
                RecordingEndpoint   endpoint;
                Acia6551            acia (kBase);
                double              expected = 0.0;
                uint64_t            elapsed  = 0;
                uint32_t            step     = 0;
                int                 written  = 0;



                acia.SetEndpoint (&endpoint);
                acia.Write (kAddrCtrl, static_cast<Byte> (Acia6551::kControlRxClockBaud | code));
                acia.Write (kAddrCmd,  kCmdPolled);

                expected = acia.GetTransmitCharacterCycles() * kBytes;
                step     = max<uint32_t> (1, static_cast<uint32_t> (acia.GetTransmitCharacterCycles() / 64.0));

                while (endpoint.Bytes().size() < static_cast<size_t> (kBytes))
                {
                    if (written < kBytes && (acia.GetStatus() & Acia6551::kStatusTxEmpty))
                    {
                        acia.Write (kAddrData, static_cast<Byte> (written));
                        ++written;
                    }

                    acia.Tick (step);
                    elapsed += step;
                }

                Assert::AreEqual (expected, static_cast<double> (elapsed), expected * kTolerance,
                                  format (L"baud code {}", code).c_str());
            }
        }


        TEST_METHOD (TransmitterOffAndCtsFalseHoldTheByte)
        {
            RecordingEndpoint   endpoint;
            Acia6551            acia (kBase);
            uint32_t            longWait = 0;



            acia.SetEndpoint (&endpoint);
            acia.Write (kAddrCtrl, kCtrl19200);
            longWait = static_cast<uint32_t> (acia.GetTransmitCharacterCycles() * 4);

            // Transmitter control 00: transmitter off.
            acia.Write (kAddrCmd,  Acia6551::kCommandDtr);
            acia.Write (kAddrData, 0x55);
            acia.Tick (longWait);
            Assert::AreEqual<size_t> (0, endpoint.Bytes().size(), L"transmitter off sends nothing");

            // Transmitter on, but CTS false.
            acia.SetModemInputs (false, true, true);
            acia.Tick (1);      // modem inputs apply at the next tick
            acia.Write (kAddrCmd, kCmdPolled);
            acia.Tick (longWait);
            Assert::AreEqual<size_t> (0, endpoint.Bytes().size(), L"CTS false stops the transmitter");

            acia.SetModemInputs (true, true, true);
            acia.Tick (longWait);
            Assert::AreEqual<size_t> (1, endpoint.Bytes().size(), L"CTS true releases the byte");
        }


        TEST_METHOD (BreakAndModemControlReachTheEndpoint)
        {
            RecordingEndpoint   endpoint;
            Acia6551            acia (kBase);



            acia.SetEndpoint (&endpoint);
            Assert::AreEqual (1, endpoint.m_controlCalls, L"attaching reports the current DTR and RTS");
            Assert::IsFalse (endpoint.m_dtr);
            Assert::IsFalse (endpoint.m_rts);

            acia.Write (kAddrCmd, kCmdPolled);
            Assert::IsTrue (endpoint.m_dtr);
            Assert::IsTrue (endpoint.m_rts);

            acia.Write (kAddrCmd, static_cast<Byte> (Acia6551::kCommandDtr | Acia6551::kCommandTicBreak));
            Assert::IsTrue (endpoint.m_break, L"transmitter control 11 sends break");

            acia.Write (kAddrCmd, kCmdPolled);
            Assert::IsFalse (endpoint.m_break);

            acia.Reset();
            Assert::IsFalse (endpoint.m_dtr, L"a reset drops DTR");
            Assert::IsFalse (endpoint.m_rts, L"a reset drops RTS");
        }


        TEST_METHOD (QueuedBytesLandOneCharacterTimeApart)
        {
            const Byte  bytes[] = { 0x10, 0x20, 0x30 };
            Acia6551    acia (kBase);
            uint32_t    charCycles = 0;



            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.EnqueueReceivedBytes (bytes, size (bytes));
            Assert::AreEqual<size_t> (3, acia.GetQueuedReceiveCount());

            acia.Tick (1);      // the receiver starts timing the first byte

            for (Byte expected : bytes)
            {
                acia.Tick (charCycles - 3);
                Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusRxFull) == 0,
                                L"no byte before its character time");

                acia.Tick (3);
                Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusRxFull) != 0,
                                L"the byte lands after one character time");
                Assert::AreEqual<Byte> (expected, acia.Read (kAddrData));
            }

            Assert::AreEqual<size_t> (0, acia.GetQueuedReceiveCount());
        }


        TEST_METHOD (PacingIsTheSameForAnyTickSize)
        {
            // The same bytes, ticked one cycle at a time and in 97-cycle
            // chunks (no common factor with a character time), arrive with
            // the same spacing: delivery follows emulated time, not tick
            // size, so maximum speed and normal speed produce the same timing.
            const Byte  bytes[]   = { 1, 2, 3, 4, 5 };
            uint64_t    fine[5]   = {};
            uint64_t    coarse[5] = {};



            TickAndStamp (bytes, size (bytes), 1,  fine);
            TickAndStamp (bytes, size (bytes), 97, coarse);

            // The first byte's countdown starts on the first tick, which is up
            // to one chunk later; after that the spacing must match to within
            // a chunk.
            for (size_t i = 1; i < size (bytes); ++i)
            {
                int64_t  fineGap   = static_cast<int64_t> (fine[i]   - fine[0]);
                int64_t  coarseGap = static_cast<int64_t> (coarse[i] - coarse[0]);

                Assert::IsTrue (llabs (coarseGap - fineGap) < 97,
                                format (L"byte {} arrives at the same spacing", i).c_str());
            }
        }

        TEST_METHOD (UnreadByteIsOverrun)
        {
            const Byte  bytes[] = { 0x11, 0x22 };
            Acia6551    acia (kBase);
            uint32_t    charCycles = 0;



            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (1);
            acia.Tick (charCycles);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusOverrun) == 0);

            acia.Tick (charCycles);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusOverrun) != 0,
                            L"a byte landing over an unread one sets overrun");
            Assert::AreEqual<Byte> (0x22, acia.Read (kAddrData), L"the new byte replaces the unread one");
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusOverrun) == 0,
                            L"reading the data register clears overrun");
        }


        TEST_METHOD (ErroredByteSetsItsErrorBitsAndAGoodByteClearsThem)
        {
            Acia6551    acia (kBase);
            uint32_t    charCycles = 0;



            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.ReceiveFromHostNow (0x55, Acia6551::kRxErrorFraming | Acia6551::kRxErrorParity);
            acia.ReceiveFromHostNow (0x66, 0);
            acia.Tick (1);
            acia.Tick (charCycles);

            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusFramingError) != 0, L"framing error with the byte");
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusParityError)  != 0, L"parity error with the byte");
            Assert::AreEqual<Byte> (0x55, acia.Read (kAddrData));

            acia.Tick (charCycles);
            Assert::IsTrue ((acia.GetStatus() & (Acia6551::kStatusFramingError | Acia6551::kStatusParityError)) == 0,
                            L"a byte without errors clears both bits");
            Assert::AreEqual<Byte> (0x66, acia.Read (kAddrData));
        }


        TEST_METHOD (HostOverrunSetsTheOverrunBitWithItsByte)
        {
            Acia6551    acia (kBase);
            uint32_t    charCycles = 0;



            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.ReceiveFromHostNow (0x31, Acia6551::kRxErrorOverrun);
            acia.Tick (1);
            acia.Tick (charCycles);

            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusOverrun) != 0, L"bytes lost on the PC side read as overrun");
            Assert::AreEqual<Byte> (0x31, acia.Read (kAddrData));
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusOverrun) == 0, L"reading the data register clears overrun");
        }


        TEST_METHOD (ReceiveErrorsAreInTheSerialLog)
        {
            Acia6551           acia (kBase);
            CommEventRecorder  recorder;
            uint32_t           charCycles = 0;



            acia.SetCommEventSink (&recorder, 0);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.ReceiveFromHostNow (0x00, Acia6551::kRxErrorFraming);
            acia.Tick (1);
            acia.Tick (charCycles);

            Assert::IsTrue (recorder.HasReceived (0x00, Acia6551::kRxErrorFraming));
        }


        TEST_METHOD (WaitingErrorsSurviveASaveAndLoad)
        {
            Acia6551    acia     (kBase);
            Acia6551    restored (kBase);
            uint32_t    charCycles = 0;
            HRESULT     hr         = S_OK;



            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));
            acia.ReceiveFromHostNow (0x12, Acia6551::kRxErrorParity);

            hr = CopyState (acia, restored);
            Assert::IsTrue (SUCCEEDED (hr));

            restored.Tick (1);
            restored.Tick (charCycles);
            Assert::IsTrue ((restored.GetStatus() & Acia6551::kStatusParityError) != 0);
        }

        TEST_METHOD (DtrOffPausesDelivery)
        {
            const Byte  bytes[] = { 0x77 };
            Acia6551    acia (kBase);
            uint32_t    charCycles = 0;



            acia.Write (kAddrCtrl, kCtrl9600);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (charCycles * 10);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusRxFull) == 0, L"DTR off: nothing lands");
            Assert::AreEqual<size_t> (1, acia.GetQueuedReceiveCount(), L"DTR off: the queue is not emptied");

            acia.Write (kAddrCmd, kCmdPolled);
            acia.Tick (1);
            acia.Tick (charCycles);
            Assert::AreEqual<Byte> (0x77, acia.Read (kAddrData));
        }


        TEST_METHOD (ReceivedByteIsMaskedToWordLength)
        {
            const Byte  bytes[] = { 0xFF };
            Acia6551    acia (kBase);



            acia.Write (kAddrCtrl, static_cast<Byte> (kCtrl9600 | 0x20));   // 7 data bits
            acia.Write (kAddrCmd,  kCmdPolled);
            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (1);
            acia.Tick (static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles())));

            Assert::AreEqual<Byte> (0x7F, acia.Read (kAddrData));
        }


        TEST_METHOD (ReceiverIrqAssertsAndStatusReadClearsIt)
        {
            const Byte              bytes[] = { 0x5A };
            AciaTestCpu             cpu;
            HRESULT                 hr      = S_OK;
            Byte                    status  = 0;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            // DTR set, receiver IRQ enabled (bit 1 clear).
            acia.Write (kAddrCtrl, kCtrl19200);
            acia.Write (kAddrCmd,  Acia6551::kCommandDtr);
            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (1);
            acia.Tick (static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles())));

            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusIrq) != 0,
                            L"Receiver-full must latch the IRQ bit");
            Assert::IsTrue (cpu.IrqAsserted(), L"IRQ line must be asserted");

            status = acia.Read (kAddrStatus);
            Assert::IsTrue ((status & Acia6551::kStatusIrq) != 0,
                            L"the status snapshot has the IRQ bit set");
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Reading status must de-assert the IRQ line");
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusIrq) == 0,
                            L"IRQ latch cleared after status read");
        }


        TEST_METHOD (ReceiverIrqSuppressedWhenDisabled)
        {
            const Byte   bytes[] = { 0x5A };
            AciaTestCpu  cpu;
            HRESULT      hr      = S_OK;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.Write (kAddrCtrl, kCtrl19200);
            acia.Write (kAddrCmd,  kCmdPolled);
            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (1);
            acia.Tick (static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles())));

            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Disabled receiver IRQ must not drive the line");
        }


        TEST_METHOD (TransmitIrqAssertsWhenEnabled)
        {
            AciaTestCpu             cpu;
            RecordingEndpoint       endpoint;
            HRESULT                 hr = S_OK;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.SetEndpoint (&endpoint);

            // DTR set, transmitter control = 01 (IRQ enabled, RTS low).
            acia.Write (kAddrCmd, Acia6551::kCommandDtr | Acia6551::kCommandTicTxIrqOn);
            acia.Write (kAddrData, 0x21);

            Assert::IsTrue (cpu.IrqAsserted(),
                            L"Transmitter-empty must assert the IRQ when enabled");
        }


        TEST_METHOD (DcdChangeRaisesIrq)
        {
            AciaTestCpu             cpu;
            HRESULT                 hr = S_OK;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.Write (kAddrCmd, Acia6551::kCommandDtr);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusDcd) == 0, L"DCD true reads 0");

            acia.SetModemInputs (true, true, false);
            acia.Tick (1);

            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusDcd) != 0, L"DCD false reads 1");
            Assert::IsTrue (cpu.IrqAsserted(), L"a DCD change interrupts");
        }


        TEST_METHOD (DisconnectedIrqLatchesStatusOnly)
        {
            const Byte   bytes[] = { 0x01 };
            AciaTestCpu  cpu;
            HRESULT      hr      = S_OK;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.SetIrqConnected (false);
            acia.Write (kAddrCtrl, kCtrl19200);
            acia.Write (kAddrCmd,  Acia6551::kCommandDtr);
            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (1);
            acia.Tick (static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles())));

            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusIrq) != 0, L"status bit 7 latches");
            Assert::IsFalse (cpu.IrqAsserted(), L"the line is not driven while disconnected");

            acia.SetIrqConnected (true);
            Assert::IsTrue (cpu.IrqAsserted(), L"connecting a set latch drives the line");
        }


        TEST_METHOD (ProgramResetClearsCommandButKeepsParityAndControl)
        {
            const Byte  bytes[]    = { 0x01, 0x02 };
            Byte        parityBits = Acia6551::kCommandParityMask;
            Acia6551    acia (kBase);



            acia.Write (kAddrCmd, static_cast<Byte> (parityBits | Acia6551::kCommandDtr));
            acia.Write (kAddrCtrl, 0x1E);
            acia.EnqueueReceivedBytes (bytes, size (bytes));

            // A write to the status address is a program reset.
            acia.Write (kAddrStatus, 0x00);

            Assert::AreEqual<Byte> (parityBits, acia.GetCommand(),
                                    L"Program reset clears all but the parity bits");
            Assert::AreEqual<Byte> (0x1E, acia.GetControl(),
                                    L"Program reset does not alter control");
            Assert::AreEqual<size_t> (2, acia.GetQueuedReceiveCount(),
                                      L"bytes on the wire are not in the chip");
        }


        TEST_METHOD (HardwareResetClearsRegistersButNotTheQueue)
        {
            const Byte  bytes[] = { 0x01 };
            Acia6551    acia (kBase);



            acia.Write (kAddrCmd,  kCmdPolled);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrData, 0x33);
            acia.EnqueueReceivedBytes (bytes, size (bytes));

            acia.SoftReset();

            Assert::AreEqual<Byte> (0, acia.GetCommand());
            Assert::AreEqual<Byte> (0, acia.GetControl());
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusTxEmpty) != 0);
            Assert::AreEqual<size_t> (1, acia.GetQueuedReceiveCount());
        }


        TEST_METHOD (ControlRegisterSelectsWordLength)
        {
            Acia6551    acia (kBase);



            // Bits 6-5 = 00 -> 8-bit words.
            acia.Write (kAddrCtrl, 0x00);
            Assert::AreEqual (8, acia.GetWordLengthBits());

            // Bits 6-5 = 01 -> 7-bit words.
            acia.Write (kAddrCtrl, 0x20);
            Assert::AreEqual (7, acia.GetWordLengthBits());

            // Bits 6-5 = 11 -> 5-bit words.
            acia.Write (kAddrCtrl, 0x60);
            Assert::AreEqual (5, acia.GetWordLengthBits());
        }


        TEST_METHOD (LoopbackPlugReturnsBytesAndWiresModemLines)
        {
            Acia6551                acia (kBase);
            AciaLoopbackEndpoint    loopback (&acia);
            uint32_t                charCycles = 0;



            acia.SetEndpoint (&loopback);
            acia.Write (kAddrCtrl, kCtrl19200);
            acia.Write (kAddrCmd,  kCmdPolled);
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.Tick (1);
            Assert::IsTrue (acia.IsCtsTrue(), L"RTS is wired to CTS");
            Assert::IsTrue ((acia.GetStatus() & (Acia6551::kStatusDsr | Acia6551::kStatusDcd)) == 0,
                            L"DTR is wired to DSR and DCD");

            acia.Write (kAddrData, 0x37);
            acia.Tick (charCycles);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusRxFull) == 0,
                            L"one character time out, one more back in");

            acia.Tick (charCycles);
            Assert::AreEqual<Byte> (0x37, acia.Read (kAddrData));
        }


        TEST_METHOD (EchoModeRetransmitsReceivedBytes)
        {
            const Byte          bytes[] = { 0x48 };
            RecordingEndpoint   endpoint;
            Acia6551            acia (kBase);
            uint32_t            charCycles = 0;



            acia.SetEndpoint (&endpoint);
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  static_cast<Byte> (Acia6551::kCommandDtr | Acia6551::kCommandEcho));
            charCycles = static_cast<uint32_t> (ceil (acia.GetTransmitCharacterCycles()));

            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.Tick (1);
            acia.Tick (charCycles);
            acia.Tick (charCycles);

            Assert::AreEqual<size_t> (1, endpoint.Bytes().size());
            Assert::AreEqual<Byte> (0x48, endpoint.Bytes().front());
        }


        TEST_METHOD (DetachingEmptiesTheQueue)
        {
            const Byte          bytes[] = { 0x01, 0x02 };
            RecordingEndpoint   endpoint;
            Acia6551            acia (kBase);



            acia.SetEndpoint (&endpoint);
            acia.EnqueueReceivedBytes (bytes, size (bytes));
            acia.SetEndpoint (nullptr);

            Assert::AreEqual<size_t> (0, acia.GetQueuedReceiveCount());
        }


        TEST_METHOD (FactoryPlacesAciaInSlotIoSpace)
        {
            ComponentRegistry           registry;
            MemoryBus                   bus;
            DeviceConfig                config;
            unique_ptr<MemoryDevice>    device = nullptr;



            ComponentRegistry::RegisterBuiltinDevices (registry);
            Assert::IsTrue (registry.IsRegistered ("acia-6551"),
                            L"acia-6551 must be a registered device type");

            config.type    = "acia-6551";
            config.slot    = 1;
            config.hasSlot = true;

            device = registry.Create ("acia-6551", config, bus);
            Assert::IsNotNull (device.get());
            Assert::AreEqual<Word> (0xC098, device->GetStart());
            Assert::AreEqual<Word> (0xC09B, device->GetEnd());
        }

    private:
        //  Ticks a fresh 9,600-baud ACIA in `step`-cycle chunks and stores the
        //  cycle count at which each byte is read.
        static void TickAndStamp (const Byte * bytes, size_t count, uint32_t step, uint64_t * outStamps)
        {
            Acia6551  acia (kBase);
            uint64_t  now  = 0;
            size_t    got  = 0;



            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  kCmdPolled);
            acia.EnqueueReceivedBytes (bytes, count);

            while (got < count && now < 1000000)
            {
                acia.Tick (step);
                now += step;

                if (acia.GetStatus() & Acia6551::kStatusRxFull)
                {
                    (void) acia.Read (kAddrData);
                    outStamps[got] = now;
                    ++got;
                }
            }

            Assert::AreEqual (count, got, L"every byte arrived");
        }    };
}
