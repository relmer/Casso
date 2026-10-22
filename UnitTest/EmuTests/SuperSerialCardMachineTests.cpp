#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/IAciaEndpoint.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/IProcessProbe.h"
#include "Seams/InProcessByteStream.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace SuperSerialCardMachineTestNs
{
    static constexpr Word  kProgram    = 0x0300;
    static constexpr Word  kSlot2Page  = 0xC200;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingSerialEndpoint
    //
    ////////////////////////////////////////////////////////////////////////////

    class RecordingSerialEndpoint : public IAciaEndpoint
    {
    public:
        void    OnByteTransmitted (Byte value) override { m_bytes.push_back (value); }

        vector<Byte>    m_bytes;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SuperSerialCardMachineTests
    //
    //  The card in a whole machine built by MachineBuilder: slot 2 of a ][,
    //  ][+ and //e, routed through each machine's own $C100-$CFFF decoding,
    //  driven by guest code the CPU runs.
    //
    //  Apple's firmware is not in the test tree, so firmware tests install a
    //  synthetic image after the build. The real firmware is checked manually
    //  (quickstart.md).
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SuperSerialCardMachineTests)
    {
    public:
        TEST_METHOD (ShippedConfigsListTheCardTurnedOff)
        {
            for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
            {
                TestMachine  machine (id);

                Assert::IsNull (machine.GetRefs().superSerialCard,
                                TextEncoding::NarrowToWide (id).c_str());
            }
        }


        //  The //c has no slots, so its config lists no card for Settings to
        //  offer, and its serial ports are its own two built-in 6551s.
        TEST_METHOD (TheApple2cHasNoCard)
        {
            TestMachine  machine ("Apple2c");



            Assert::IsNull (machine.GetRefs().superSerialCard);

            for (const SlotConfig & slot : machine.GetConfig().slots)
            {
                Assert::AreNotEqual (std::string (kpszSuperSerialCardDevice), slot.device, L"no slot lists the card");
            }

            Assert::AreEqual ((size_t) 2, machine.GetRefs().serialChips.size(), L"the two built-in ports");
        }


        TEST_METHOD (TurnedOnTheCardIsInSlot2)
        {
            for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
            {
                TestMachine        machine (id, EnableCard);
                SuperSerialCard  * card = machine.GetRefs().superSerialCard;
                std::wstring       name = TextEncoding::NarrowToWide (id);



                Assert::IsNotNull (card, name.c_str());
                Assert::AreEqual (2, card->GetSlot());
                Assert::IsTrue (std::find (machine.GetRefs().serialChips.begin(),
                                           machine.GetRefs().serialChips.end(),
                                           &card->GetAcia()) != machine.GetRefs().serialChips.end(),
                                L"the card's 6551 is ticked with the machine");
                Assert::AreEqual (card->GetSwitches().GetDipSwitch1Byte (0) & 0xF3,
                                  machine.GetMemoryBus().ReadByte (0xC0A1) & 0xF3,
                                  L"DIPSW1 reads through the bus");
            }
        }


        TEST_METHOD (FirmwareIsMappedOnTheIIeAndIIPlus)
        {
            for (const char * id : { "Apple2Plus", "Apple2e" })
            {
                TestMachine        machine (id, EnableCard);
                SuperSerialCard  * card     = machine.GetRefs().superSerialCard;
                vector<Byte>       firmware = MakeIdFirmware();
                MemoryBus        & bus      = machine.GetMemoryBus();



                machine.PowerCycle();
                Assert::AreEqual (S_OK, card->SetFirmware (firmware));

                Assert::AreEqual<Byte> (0x38, bus.ReadByte (0xC205));
                Assert::AreEqual<Byte> (0x18, bus.ReadByte (0xC207));
                Assert::AreEqual<Byte> (0x01, bus.ReadByte (0xC20B));
                Assert::AreEqual<Byte> (0x31, bus.ReadByte (0xC20C), L"device signature: serial card");

                Assert::IsTrue (card->IsExpansionRomSelected(), L"the page access selects the card");
                Assert::AreEqual (firmware[0x000], bus.ReadByte (0xC800));
                Assert::AreEqual (firmware[0x6FF], bus.ReadByte (0xCEFF));

                (void) bus.ReadByte (0xCFFF);
                Assert::IsFalse (card->IsExpansionRomSelected(), L"$CFFF deselects");
            }
        }


        TEST_METHOD (IntCxRomHidesTheCardOnTheIIe)
        {
            TestMachine        machine ("Apple2e", EnableCard);
            SuperSerialCard  * card = machine.GetRefs().superSerialCard;
            MemoryBus        & bus  = machine.GetMemoryBus();



            machine.PowerCycle();
            Assert::AreEqual (S_OK, card->SetFirmware (MakeIdFirmware()));

            bus.WriteByte (0xC007, 0);          // SETINTCXROM
            (void) bus.ReadByte (0xC205);
            Assert::IsFalse (card->IsExpansionRomSelected(), L"INTCXROM: the page is internal ROM");

            bus.WriteByte (0xC006, 0);          // SETSLOTCXROM
            Assert::AreEqual<Byte> (0x38, bus.ReadByte (0xC205));
            Assert::IsTrue (card->IsExpansionRomSelected());
        }


        TEST_METHOD (DiskIiRomIsUnaffectedOnTheIIPlus)
        {
            TestMachine  machine ("Apple2Plus", EnableCard);



            // The Disk II boot ROM begins LDX #$20.
            Assert::AreEqual<Byte> (0xA2, machine.GetMemoryBus().ReadByte (0xC600));
            Assert::AreEqual<Byte> (0x20, machine.GetMemoryBus().ReadByte (0xC601));
        }


        TEST_METHOD (FirmwareRunsFromC800AndTransmits)
        {
            // $C200: LDA $CFFF / JMP $C800. $C800: program 19,200 baud, send
            // 'A', RTS. The page fetches select the card, so the JMP lands in
            // its expansion ROM.
            const Byte          page[]      = { 0xAD, 0xFF, 0xCF, 0x4C, 0x00, 0xC8 };
            const Byte          expansion[] = { 0xA9, 0x1F, 0x8D, 0xAB, 0xC0,
                                                0xA9, 0x0B, 0x8D, 0xAA, 0xC0,
                                                0xA9, 0x41, 0x8D, 0xA8, 0xC0,
                                                0x60 };
            const Byte          caller[]    = { 0x20, 0x00, 0xC2,          // JSR $C200
                                                0x4C, 0x03, 0x03 };        // JMP $0303
            TestMachine         machine ("Apple2e", EnableCard);
            SuperSerialCard   * card     = machine.GetRefs().superSerialCard;
            vector<Byte>        firmware (SuperSerialCard::kFirmwareSize, 0xEA);
            RecordingSerialEndpoint  recorder;



            copy (begin (page),      end (page),      firmware.begin() + SuperSerialCard::kFirmwarePageOffset);
            copy (begin (expansion), end (expansion), firmware.begin());

            machine.PowerCycle();
            Assert::AreEqual (S_OK, card->SetFirmware (firmware));
            card->GetAcia().SetEndpoint (&recorder);

            LoadProgram (machine, caller, size (caller));
            machine.RunCycles (20000);

            Assert::AreEqual<size_t> (1, recorder.m_bytes.size());
            Assert::AreEqual<Byte> (0x41, recorder.m_bytes.front());
        }


        TEST_METHOD (DirectLoopbackWorksWithoutFirmware)
        {
            // 19,200 baud 8N1 through $C0AB/$C0AA; send four bytes from $0340
            // and store what comes back at $0360.
            const Byte  program[] =
            {
                0xA9, 0x1F, 0x8D, 0xAB, 0xC0,       // LDA #$1F / STA $C0AB
                0xA9, 0x0B, 0x8D, 0xAA, 0xC0,       // LDA #$0B / STA $C0AA
                0xA2, 0x00,                         // LDX #$00
                0xAD, 0xA9, 0xC0,                   // $030C: LDA $C0A9
                0x29, 0x10,                         // AND #$10   TDRE
                0xF0, 0xF9,                         // BEQ $030C
                0xBD, 0x40, 0x03,                   // LDA $0340,X
                0x8D, 0xA8, 0xC0,                   // STA $C0A8
                0xAD, 0xA9, 0xC0,                   // $0319: LDA $C0A9
                0x29, 0x08,                         // AND #$08   RDRF
                0xF0, 0xF9,                         // BEQ $0319
                0xAD, 0xA8, 0xC0,                   // LDA $C0A8
                0x9D, 0x60, 0x03,                   // STA $0360,X
                0xE8,                               // INX
                0xE0, 0x04,                         // CPX #$04
                0xD0, 0xE1,                         // BNE $030C
                0x4C, 0x2B, 0x03,                   // $032B: JMP $032B
            };
            const Byte              data[] = { 0x12, 0x34, 0xAB, 0xFF };
            TestMachine             machine ("Apple2e", EnableCard);
            SuperSerialCard       * card   = machine.GetRefs().superSerialCard;
            AciaLoopbackEndpoint    loopback (&card->GetAcia());



            machine.PowerCycle();
            Assert::AreEqual (S_OK, card->SetFirmware ({}));
            Assert::IsFalse (card->HasFirmware());
            card->GetAcia().SetEndpoint (&loopback);

            for (size_t i = 0; i < size (data); ++i)
            {
                machine.GetCpu()->WriteByte (static_cast<Word> (0x0340 + i), data[i]);
            }

            LoadProgram (machine, program, size (program));
            machine.RunCycles (20000);

            for (size_t i = 0; i < size (data); ++i)
            {
                Assert::AreEqual (data[i], machine.GetCpu()->ReadByte (static_cast<Word> (0x0360 + i)),
                                  format (L"byte {}", i).c_str());
            }

            card->GetAcia().SetEndpoint (nullptr);
        }


        TEST_METHOD (BaudCodeZeroRunsAt115200)
        {
            TestMachine        machine ("Apple2e", EnableCard);
            Acia6551         & acia  = machine.GetRefs().superSerialCard->GetAcia();
            double             clock = static_cast<double> (machine.GetConfig().clockSpeed);



            machine.GetMemoryBus().WriteByte (0xC0AB, Acia6551::kControlRxClockBaud);

            Assert::AreEqual (clock * 10.0 / 115200.0, acia.GetTransmitCharacterCycles(), 0.01);
        }


        TEST_METHOD (Receives64KbAt19200WithoutOverrun)
        {
            constexpr size_t  kBytes = 0xFFFF;
            vector<Byte>      stream (kBytes);
            TestMachine       machine ("Apple2e", EnableCard);
            Acia6551        & acia   = machine.GetRefs().superSerialCard->GetAcia();
            uint64_t          budget = 0;



            for (size_t i = 0; i < kBytes; ++i)
            {
                stream[i] = static_cast<Byte> (i * 7);
            }

            machine.PowerCycle();
            LoadProgram (machine, kCountingReader, size (kCountingReader));

            acia.EnqueueReceivedBytes (stream.data(), stream.size());
            budget = static_cast<uint64_t> (static_cast<double> (machine.GetConfig().clockSpeed) * 10.0 / 19200.0 * (kBytes + 4));
            machine.RunCycles (budget);

            Assert::AreEqual<size_t> (0, acia.GetQueuedReceiveCount(), L"every byte was delivered");
            Assert::AreEqual<int> (static_cast<int> (kBytes), machine.GetCpu()->ReadByte (0x00) | (machine.GetCpu()->ReadByte (0x01) << 8),
                                   L"the guest read every byte");
            Assert::AreEqual<Byte> (0, static_cast<Byte> (machine.GetCpu()->ReadByte (0x02) & Acia6551::kStatusOverrun),
                                    L"no overrun at real-hardware speed");
        }


        TEST_METHOD (SlowReaderSeesOverrun)
        {
            // The counting reader with a ~1,200-cycle delay after each byte,
            // more than two character times at 19,200 baud.
            constexpr size_t  kBytes = 64;
            vector<Byte>      stream (kBytes, 0x55);
            vector<Byte>      program (begin (kCountingReader), end (kCountingReader));
            TestMachine       machine ("Apple2e", EnableCard);
            Acia6551        & acia = machine.GetRefs().superSerialCard->GetAcia();



            // Take the reader up to its BEQ ($0312-$031E), then read the data
            // register and spin before polling again.
            constexpr size_t  kThroughPoll = 0x1F;

            program.resize (kThroughPoll);
            program.insert (program.end(), { 0xAD, 0xA8, 0xC0,        // LDA $C0A8
                                             0xA0, 0xF0,              // LDY #$F0
                                             0x88,                    // DEY
                                             0xD0, 0xFD,              // BNE *-1
                                             0x4C, 0x12, 0x03 });     // JMP $0312

            machine.PowerCycle();
            LoadProgram (machine, program.data(), program.size());
            acia.EnqueueReceivedBytes (stream.data(), stream.size());
            machine.RunCycles (200000);

            Assert::AreEqual<Byte> (Acia6551::kStatusOverrun,
                                    static_cast<Byte> (machine.GetCpu()->ReadByte (0x02) & Acia6551::kStatusOverrun),
                                    L"a reader slower than the line reads an overrun");
        }

        //  The card's connector on a TCP port is written to Casso Explorer's
        //  file as a card's slot, and connected elsewhere it is empty, not
        //  looped back as a //c's port is (FR-095, FR-083b).
        TEST_METHOD (ACardOnTcp_IsRecordedAsItsSlot_AndEmptyOnceDisconnected)
        {
            struct Handle : IByteStream
            {
                std::shared_ptr<InProcessByteStream>  end;

                explicit Handle (std::shared_ptr<InProcessByteStream> e) : end (std::move (e)) {}

                HRESULT  Write            (const Byte * bytes, size_t count) override                                  { return end->Write (bytes, count); }
                HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return end->Read (buffer, capacity, timeoutMs, outRead); }
                size_t   GetReadableCount() override                                                                  { return end->GetReadableCount(); }
                bool     IsClosed() const override                                                                    { return end->IsClosed(); }
                void     Close() override                                                                             { end->Close(); }
            };

            struct Listener : IByteStreamFactory
            {
                std::vector<std::shared_ptr<InProcessByteStream>>  ends;
                uint16_t                                           port = 0;

                HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                      { outPorts.clear(); return S_OK; }
                HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override { return E_NOTIMPL; }
                HRESULT  ConnectTcp   (const std::string &, uint16_t, std::unique_ptr<IByteStream> &) override             { return E_NOTIMPL; }

                HRESULT  ListenTcp (uint16_t tcpPort, LinkTransport, std::unique_ptr<IByteStream> & outStream) override
                {
                    std::shared_ptr<InProcessByteStream>  machineEnd;
                    std::shared_ptr<InProcessByteStream>  pcEnd;

                    InProcessByteStream::CreatePair (machineEnd, pcEnd);
                    ends.push_back (pcEnd);
                    port      = tcpPort;
                    outStream = std::make_unique<Handle> (machineEnd);
                    return S_OK;
                }
            };

            struct Probe : IProcessProbe
            {
                bool  TryGetStartTime (DWORD, uint64_t & outStarted) override { outStarted = 77; return true; }
            };

            TestMachine                machine ("Apple2e", [] (MachineConfig & config)
            {
                for (SlotConfig & slot : config.slots)
                {
                    if (slot.device == kpszSuperSerialCardDevice)
                    {
                        slot.slot    = 5;
                        slot.enabled = true;
                    }
                }
            });
            InMemoryFileSystem         fs;
            Probe                      probe;
            Listener                   factory;
            RunningCassoRegistry       registry (fs, probe, L"C:\\Links");
            std::vector<RunningCasso>  running;
            Word                       data = 0xC0D8;   // slot 5's 6551



            machine.PowerCycle();
            machine.SetRunningRegistry (&registry);

            Assert::AreEqual (S_OK, machine.ConnectSerialPort (5, "tcp:6505", &factory, nullptr));
            Assert::AreEqual<uint16_t> (6505, factory.port);
            AssertSucceeded (registry.Enumerate (running));
            Assert::AreEqual ((size_t) 1, running.size());
            Assert::AreEqual (5, running[0].serialPort);
            Assert::IsTrue   (running[0].onCard);

            Assert::AreEqual (S_OK, machine.ConnectSerialPort (5, "", &factory, nullptr));

            machine.GetMemoryBus().WriteByte ((Word) (data + Acia6551::kRegCommand), 0x0B);
            machine.GetMemoryBus().WriteByte (data, 0x11);
            machine.RunCycles (20000);
            Assert::AreEqual<Byte> (0, (Byte) (machine.GetMemoryBus().ReadByte ((Word) (data + Acia6551::kRegStatus)) & Acia6551::kStatusRxFull),
                                    L"an empty connector, not a loopback plug");

            machine.SetRunningRegistry (nullptr);
        }


        //  On the card's connector the ImageWriter II gets what the card
        //  sends. The parallel card, listed first, would take it otherwise.
        TEST_METHOD (AnImageWriterOnTheCard_GetsWhatTheCardSends)
        {
            TestMachine        machine ("Apple2e", AttachPrinterToCard);
            PrinterByteRing  * ring    = machine.GetRefs().printerRing;
            Byte               out     = 0;



            Assert::IsTrue (ring == &machine.GetPrinterRing(), L"the card has the printer");

            machine.PowerCycle();
            StartTransmitter (machine);
            machine.GetMemoryBus().WriteByte (0xC0A8, 'P');
            machine.RunCycles (5000);

            Assert::IsTrue (ring->TryPop (out));
            Assert::AreEqual<Byte> ('P', out);
            Assert::IsFalse (machine.GetMemoryBus().ReadByte (0xC091) == PrinterCard::kStatusReady, L"the parallel card's connector is empty");
        }


        //  The printer's line drops near a full ring, which stops the card's
        //  transmitter, and rises once the ring has room.
        TEST_METHOD (TheSerialPrinter_DropsItsLineUntilTheRingHasRoom)
        {
            TestMachine        machine ("Apple2e", AttachPrinterToCard);
            PrinterByteRing  & ring    = machine.GetPrinterRing();
            Acia6551         & acia    = machine.GetRefs().superSerialCard->GetAcia();
            Byte               out     = 0;
            uint32_t           i       = 0;



            machine.PowerCycle();
            StartTransmitter (machine);

            for (i = 0; i < PrinterByteRing::kByteRingCapacity - AciaPrinterEndpoint::kBusyFreeBytes; i++)
            {
                Assert::IsTrue (ring.TryPush (0));
            }

            machine.GetMemoryBus().WriteByte (0xC0A8, 'Q');
            machine.RunCycles (5000);
            Assert::IsFalse (acia.IsCtsTrue(), L"busy near full");

            while (ring.TryPop (out))
            {
            }

            machine.RunCycles (5000);
            Assert::IsTrue (acia.IsCtsTrue(), L"ready once drained");
        }


        //  On a //c the printer is on one port at a time; unplugged from one,
        //  it can go on the other.
        TEST_METHOD (TheApple2csPrinter_IsOnOnePortAtATime)
        {
            TestMachine  machine ("Apple2c", [] (MachineConfig & config)
            {
                for (PortConfig & port : config.ports)
                {
                    port.device = (port.name == "serial1") ? std::string (kpszImageWriterDevice) : port.device;
                }
            });



            Assert::IsTrue (machine.GetRefs().printerRing == &machine.GetPrinterRing(), L"port 1 has the printer");
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BUSY), machine.ConnectSerialPort (2, kpszImageWriterDevice, nullptr, nullptr));

            Assert::AreEqual (S_OK, machine.ConnectSerialPort (1, "", nullptr, nullptr));
            Assert::IsNull (machine.GetRefs().printerRing, L"unplugged from port 1");

            Assert::AreEqual (S_OK, machine.ConnectSerialPort (2, kpszImageWriterDevice, nullptr, nullptr));
            Assert::IsTrue (machine.GetRefs().printerRing == &machine.GetPrinterRing(), L"now on port 2");
        }


        TEST_METHOD (AParallelCardListingNothing_HasThePrinter_AndOneListingAnEmptyPortDoesNot)
        {
            TestMachine  bare  ("Apple2e");
            TestMachine  empty ("Apple2e", [] (MachineConfig & config)
            {
                for (SlotConfig & slot : config.slots)
                {
                    if (slot.device == kpszParallelPrinterDevice)
                    {
                        slot.ports = { PortConfig {} };
                    }
                }
            });



            Assert::IsTrue (bare.GetRefs().printerRing == &bare.GetPrinterRing());
            Assert::AreEqual (PrinterCard::kStatusReady, bare.GetMemoryBus().ReadByte (0xC091));

            Assert::IsNull (empty.GetRefs().printerRing);
            Assert::AreEqual (PrinterCard::kStatusBusy, empty.GetMemoryBus().ReadByte (0xC091));
        }

    private:
        static void AttachPrinterToCard (MachineConfig & config)
        {
            for (SlotConfig & slot : config.slots)
            {
                if (slot.device == kpszSuperSerialCardDevice)
                {
                    slot.enabled = true;
                    slot.ports   = { PortConfig { "", kpszImageWriterDevice, "" } };
                }
                else if (slot.device == kpszParallelPrinterDevice)
                {
                    slot.ports = { PortConfig {} };
                }
            }
        }

        //  19,200 baud 8N1, DTR on and the transmitter on, at slot 2.
        static void StartTransmitter (TestMachine & machine)
        {
            machine.GetMemoryBus().WriteByte (0xC0AB, 0x1F);
            machine.GetMemoryBus().WriteByte (0xC0AA, 0x0B);
        }

        //  Programs 19,200 baud 8N1, then counts received bytes in $00/$01 and
        //  ORs every status read into $02.
        static constexpr Byte  kCountingReader[] =
        {
            0xA9, 0x1F, 0x8D, 0xAB, 0xC0,       // LDA #$1F / STA $C0AB
            0xA9, 0x0B, 0x8D, 0xAA, 0xC0,       // LDA #$0B / STA $C0AA
            0xA9, 0x00, 0x85, 0x00,             // LDA #0 / STA $00
            0x85, 0x01, 0x85, 0x02,             // STA $01 / STA $02
            0xAD, 0xA9, 0xC0,                   // $0312: LDA $C0A9
            0xAA,                               // TAX
            0x05, 0x02,                         // ORA $02
            0x85, 0x02,                         // STA $02
            0x8A,                               // TXA
            0x29, 0x08,                         // AND #$08
            0xF0, 0xF3,                         // BEQ $0312
            0xAD, 0xA8, 0xC0,                   // LDA $C0A8
            0xE6, 0x00,                         // INC $00
            0xD0, 0xEC,                         // BNE $0312
            0xE6, 0x01,                         // INC $01
            0x4C, 0x12, 0x03,                   // JMP $0312
        };

        static void EnableCard (MachineConfig & config)
        {
            for (SlotConfig & slot : config.slots)
            {
                if (slot.device == kpszSuperSerialCardDevice)
                {
                    slot.enabled = true;
                }
            }
        }

        static void LoadProgram (TestMachine & machine, const Byte * bytes, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                machine.GetCpu()->WriteByte (static_cast<Word> (kProgram + i), bytes[i]);
            }

            machine.GetCpu()->SetPC (kProgram);
        }

        static vector<Byte> MakeIdFirmware()
        {
            vector<Byte>  firmware (SuperSerialCard::kFirmwareSize);



            for (size_t i = 0; i < firmware.size(); ++i)
            {
                firmware[i] = static_cast<Byte> (i ^ (i >> 8) ^ 0x5A);
            }

            firmware[0x705] = 0x38;
            firmware[0x707] = 0x18;
            firmware[0x70B] = 0x01;
            firmware[0x70C] = 0x31;

            return firmware;
        }
    };
}
