#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Core/MachineConfig.h"
#include "Debugger/Events/ICommEventSink.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Devices/Acia6551.h"
#include "HResultAssert.h"
#include "Shell/MachineStateFile.h"
#include "../NetworkTests/FakeHostNetwork.h"
#include "../SerialTests/FakeHostSerialPorts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ReverseSerialLinkTestNs
{
    static constexpr uint64_t  kSliceCycles   = 3000;
    static constexpr size_t    kSlices        = 240;
    static constexpr size_t    kSampleEvery   = 7;
    static constexpr size_t    kShuffle       = 5;
    static constexpr Word      kProgram       = 0x0800;
    static constexpr Word      kText          = 0x0900;
    static constexpr uint16_t  kListenPort    = 1977;
    static constexpr uint32_t  kBbs           = 0x0A00000B;
    static constexpr uint64_t  kOneKeyframe   = 100000;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  Sample, CommEventRecorder
    //
    ////////////////////////////////////////////////////////////////////////////

    struct Sample
    {
        uint64_t           position = 0;
        uint64_t           cycle    = 0;
        std::vector<Byte>  state;
    };

    struct LoggedEvent
    {
        IoEventKind  kind   = IoEventKind::SerialSent;
        Byte         value  = 0;
        Byte         reg    = 0;
        std::string  bytes;

        bool operator== (const LoggedEvent & other) const = default;
    };

    class CommEventRecorder : public ICommEventSink
    {
    public:
        void  OnCommEvent (const IoEvent & event, std::span<const Byte> bytes) override
        {
            bool  isHostInput = event.kind == IoEventKind::SerialReceived || event.kind == IoEventKind::SerialConnection;



            if (isHostInput)
            {
                events.push_back ({ event.kind, event.value, event.reg, std::string (bytes.begin(), bytes.end()) });
            }
        }

        std::vector<LoggedEvent>  events;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  ReverseSerialLinkTests
    //
    //  A session is recorded on a //c whose port 1 is on TCP, the modem or a
    //  COM port: the far end connects, sends bytes with errors and line
    //  changes, and goes away. After each seek back the machine must match
    //  its live state at that point, no replay may send anything to the PC,
    //  and a resume from the past must not drop the line.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReverseSerialLinkTests)
    {
    public:
        //  Port 1 at 9600 baud: each byte received is stored at $0300,X and
        //  sent back.
        static void PrepareEcho (TestMachine & machine)
        {
            static constexpr Byte  kLoop[] =
            {
                0x78,                   // 0800  SEI
                0xA2, 0x00,             // 0801  LDX #$00
                0xA9, 0x0B,             // 0803  LDA #$0B
                0x8D, 0x9A, 0xC0,       // 0805  STA $C09A
                0xA9, 0x1E,             // 0808  LDA #$1E
                0x8D, 0x9B, 0xC0,       // 080A  STA $C09B
                0xAD, 0x99, 0xC0,       // 080D  LDA $C099
                0x29, 0x08,             // 0810  AND #$08
                0xF0, 0xF9,             // 0812  BEQ $080D
                0xAD, 0x98, 0xC0,       // 0814  LDA $C098
                0x9D, 0x00, 0x03,       // 0817  STA $0300,X
                0x8D, 0x98, 0xC0,       // 081A  STA $C098
                0xE8,                   // 081D  INX
                0x4C, 0x0D, 0x08,       // 081E  JMP $080D
            };

            Load (machine, kLoop, sizeof (kLoop));
        }


        //  Port 1 at 9600 baud: the zero-terminated text at $0900 is sent,
        //  then each byte received is stored at $0300,X.
        static void PrepareDialer (TestMachine & machine, const std::string & text)
        {
            static constexpr Byte  kLoop[] =
            {
                0x78,                   // 0800  SEI
                0xA9, 0x0B,             // 0801  LDA #$0B
                0x8D, 0x9A, 0xC0,       // 0803  STA $C09A
                0xA9, 0x1E,             // 0806  LDA #$1E
                0x8D, 0x9B, 0xC0,       // 0808  STA $C09B
                0xA2, 0x00,             // 080B  LDX #$00
                0xBD, 0x00, 0x09,       // 080D  LDA $0900,X
                0xF0, 0x0F,             // 0810  BEQ $0821
                0x48,                   // 0812  PHA
                0xAD, 0x99, 0xC0,       // 0813  LDA $C099
                0x29, 0x10,             // 0816  AND #$10     transmitter empty
                0xF0, 0xF9,             // 0818  BEQ $0813
                0x68,                   // 081A  PLA
                0x8D, 0x98, 0xC0,       // 081B  STA $C098
                0xE8,                   // 081E  INX
                0xD0, 0xEC,             // 081F  BNE $080D
                0xA2, 0x00,             // 0821  LDX #$00
                0xAD, 0x99, 0xC0,       // 0823  LDA $C099
                0x29, 0x08,             // 0826  AND #$08     receiver full
                0xF0, 0xF9,             // 0828  BEQ $0823
                0xAD, 0x98, 0xC0,       // 082A  LDA $C098
                0x9D, 0x00, 0x03,       // 082D  STA $0300,X
                0xE8,                   // 0830  INX
                0x4C, 0x23, 0x08,       // 0831  JMP $0823
            };
            size_t  i = 0;



            Load (machine, kLoop, sizeof (kLoop));

            for (i = 0; i <= text.size(); i++)
            {
                machine.GetMemoryBus().WriteByte (static_cast<Word> (kText + i), (i < text.size()) ? (Byte) text[i] : 0);
            }
        }


        static std::string ReadReceived (MachineHost & machine)
        {
            constexpr Word  kStore  = 0x0300;
            constexpr int   kLength = 256;
            std::string     text;
            int             i       = 0;



            for (i = 0; i < kLength; i++)
            {
                text += (char) machine.GetMemoryBus().ReadByte ((Word) (kStore + i));
            }

            return text;
        }

        static void Load (TestMachine & machine, const Byte * program, size_t size)
        {
            size_t  i = 0;



            machine.PowerCycle();

            for (i = 0; i < size; i++)
            {
                machine.GetMemoryBus().WriteByte (static_cast<Word> (kProgram + i), program[i]);
            }

            machine.GetCpu()->SetPC (kProgram);
        }


        static void AttachPort1 (MachineConfig & config, const PortConfig & attachment)
        {
            for (PortConfig & port : config.ports)
            {
                if (port.name == kpszSerialPort1Name)
                {
                    port.device = attachment.device;
                    port.serial = attachment.serial;
                }
            }
        }


        static PortConfig MakeAttachment (const char * device)
        {
            PortConfig  port;



            port.device                 = device;
            port.serial.listen          = true;
            port.serial.tcpPort         = kListenPort;
            port.serial.comPort         = "COM3";

            return port;
        }


        static Sample TakeSample (MachineHost & machine)
        {
            Sample  sample;



            sample.position = machine.GetPosition();
            sample.cycle    = machine.GetCpu()->GetTotalCycles();
            sample.state    = ReverseSessionRig::Save (machine);

            return sample;
        }


        static std::vector<Sample> Record (TestMachine & machine, const std::function<void (size_t)> & feed)
        {
            std::vector<Sample>  samples;
            size_t               slice = 0;



            for (slice = 0; slice < kSlices; slice++)
            {
                if (slice % kSampleEvery == 0)
                {
                    samples.push_back (TakeSample (machine));
                }

                feed (slice);
                machine.RunCycles (kSliceCycles);
            }

            return samples;
        }


        static size_t CountRecords (MachineHost & machine, InputKind kind)
        {
            const InputJournal  & journal = machine.GetInputJournal();
            size_t                count   = 0;
            size_t                i       = 0;



            for (i = journal.GetBeginIndex(); i < journal.GetEndIndex(); i++)
            {
                count += journal.GetRecord (i).kind == kind ? 1 : 0;
            }

            return count;
        }


        //  Seeks to every sample in a shuffled order and to the live end,
        //  comparing the machine with what it was live.
        static void SeekEverywhere (MachineHost & machine, ReverseController & controller, const std::vector<Sample> & samples, const Sample & last)
        {
            ReverseResult  result;
            HRESULT        hr     = S_OK;
            size_t         count  = samples.size();
            size_t         i      = 0;



            Assert::IsTrue (count > 0);

            for (i = 0; i < count; i++)
            {
                const Sample  & sample = samples[(i * kShuffle) % count];

                hr = controller.SeekToCycle (sample.cycle, result);
                AssertSucceeded (hr, L"SeekToCycle");
                Assert::AreEqual<uint64_t> (sample.position, result.position);
                CheckState (sample.state, ReverseSessionRig::Save (machine), sample.position);
            }

            hr = controller.SeekToCycle (UINT64_MAX, result);
            AssertSucceeded (hr, L"SeekToCycle live");
            CheckState (last.state, ReverseSessionRig::Save (machine), last.position);
        }


        static void CheckState (const std::vector<Byte> & expected, const std::vector<Byte> & actual, uint64_t position)
        {
            size_t  size  = std::min (expected.size(), actual.size());
            size_t  first = 0;



            Assert::AreEqual (expected.size(), actual.size(), std::format (L"state size at position {}", position).c_str());

            while (first < size && expected[first] == actual[first])
            {
                first++;
            }

            Assert::AreEqual (size, first, std::format (L"the replayed state at position {} differs from live first at byte {}", position, first).c_str());
        }


        //  A client connects, sends three bytes every fourth slice and goes
        //  away. Its bytes come back, live only.
        static std::function<void (size_t)> MakeTcpClient (FakeHostNetwork & network, std::shared_ptr<FakeSocketState> & client, size_t closeAt)
        {
            return [&network, &client, closeAt] (size_t slice)
            {
                constexpr size_t  kConnectAt = 10;
                constexpr size_t  kFeedEvery = 4;

                if (slice == kConnectAt)
                {
                    client = network.OfferConnection (network.FindBound (HostSocketKind::Tcp, kListenPort), { 0x7F000001, 50000 });
                }

                if (client != nullptr && slice > kConnectAt && slice < closeAt && slice % kFeedEvery == 0)
                {
                    client->inbound.insert (client->inbound.end(), { (Byte) slice, 0xFF, (Byte) (slice + 1) });
                }

                if (client != nullptr && slice == closeAt)
                {
                    client->inboundEof = true;
                }
            };
        }


        TEST_METHOD (ATcpSessionReplaysAndSendsNothing)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine    ("Apple2c", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialTcp));
                services.hostNetwork = &network;
            });
            ReverseController                 controller (machine);
            std::shared_ptr<FakeSocketState>  client;
            std::vector<Sample>               samples;
            Sample                            last;
            size_t                            sent       = 0;
            HRESULT                           hr         = S_OK;



            PrepareEcho (machine);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, MakeTcpClient (network, client, kSlices - 40));
            last    = TakeSample (machine);
            sent    = client->sent.size();

            Assert::IsTrue (sent > 0,                                            L"live, the client received the guest's echo");
            Assert::IsTrue (CountRecords (machine, InputKind::AciaByte)       > 0, L"the bytes are journaled");
            Assert::IsTrue (CountRecords (machine, InputKind::AciaLines)      > 0, L"the line changes are journaled");
            Assert::AreEqual<size_t> (2, CountRecords (machine, InputKind::AciaConnection), L"the connection opening and closing");

            SeekEverywhere (machine, controller, samples, last);

            Assert::AreEqual (sent, client->sent.size(), L"no replay sent the client anything");
        }


        TEST_METHOD (AModemSessionReplaysAndSendsNothing)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine    ("Apple2c", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialModem));
                services.hostNetwork = &network;
            });
            ReverseController                 controller (machine);
            std::shared_ptr<FakeSocketState>  server;
            std::vector<Sample>               samples;
            Sample                            last;
            size_t                            sent       = 0;
            HRESULT                           hr         = S_OK;



            network.SetHostAddress ("bbs.example.org", kBbs);
            PrepareDialer (machine, "ATDT bbs.example.org\r");

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, [&network, &server] (size_t slice)
            {
                constexpr size_t  kTextAt    = 60;
                constexpr size_t  kCloseAt   = 150;
                constexpr size_t  kFeedEvery = 4;

                if (server == nullptr)
                {
                    server = network.FindConnected ({ kBbs, 23 });

                    if (server != nullptr)
                    {
                        server->connect = FakeSocketState::ConnectOutcome::Accepted;
                    }
                }

                if (server != nullptr && slice == kTextAt)
                {
                    server->inbound = { 'W', 'e', 'l', 'c', 'o', 'm', 'e' };
                }

                if (server != nullptr && slice > kTextAt && slice < kCloseAt && slice % kFeedEvery == 0)
                {
                    server->inbound.insert (server->inbound.end(), { (Byte) ('a' + slice % 26), '.' });
                }

                if (server != nullptr && slice == kCloseAt)
                {
                    server->inboundEof = true;
                }
            });
            last = TakeSample (machine);

            Assert::IsNotNull (server.get(), L"the guest dialed");
            Assert::IsFalse (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"the BBS hung up live");
            Assert::IsTrue (ReadReceived (machine).find ("CONNECT 9600\r\nWelcome") != std::string::npos, L"the guest received the modem's output");

            sent = server->sent.size();
            SeekEverywhere (machine, controller, samples, last);

            Assert::AreEqual (sent, server->sent.size(), L"no replay sent the BBS anything");
        }


        TEST_METHOD (AComPortSessionReplaysAndSendsNothing)
        {
            FakeHostSerialPorts                   ports;
            std::shared_ptr<FakeSerialPortState>  com3       = ports.AddPort ("COM3");
            TestMachine                           machine    ("Apple2c", TestMachine::Slots::AsShipped, [&ports] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialComPort));
                services.hostSerialPorts = &ports;
            });
            ReverseController                     controller (machine);
            std::vector<Sample>                   samples;
            Sample                                last;
            size_t                                written    = 0;
            size_t                                configured = 0;
            HRESULT                               hr         = S_OK;



            PrepareEcho (machine);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, [&com3] (size_t slice)
            {
                constexpr size_t  kFeedEvery  = 4;
                constexpr size_t  kLinesEvery = 30;

                if (slice % kFeedEvery == 0)
                {
                    com3->inbound.insert (com3->inbound.end(), { (Byte) slice, (Byte) (slice + 1) });
                    com3->errors.isFraming = (slice % (kFeedEvery * 3)) == 0;
                }

                if (slice % kLinesEvery == 0)
                {
                    com3->lines.dcd = !com3->lines.dcd;
                }
            });
            last       = TakeSample (machine);
            written    = com3->written.size();
            configured = com3->formats.size();

            Assert::IsTrue (written > 0);
            Assert::IsTrue (CountRecords (machine, InputKind::AciaLines) > 0);

            SeekEverywhere (machine, controller, samples, last);

            Assert::AreEqual (written,    com3->written.size(), L"no replay wrote to the port");
            Assert::AreEqual (configured, com3->formats.size(), L"no replay set the port's format");
        }


        //  FR-019: the machine goes live again at a point in the past before
        //  the client connected. The real connection is open, DCD matches it
        //  at the next poll, and what the client sent since and sends next is
        //  received and echoed.
        TEST_METHOD (TheTcpConnectionStaysOpenAcrossAResume)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine    ("Apple2c", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialTcp));
                services.hostNetwork = &network;
            });
            ReverseController                 controller (machine);
            std::shared_ptr<FakeSocketState>  client;
            std::vector<Sample>               samples;
            ReverseResult                     result;
            HRESULT                           hr         = S_OK;



            PrepareEcho (machine);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, MakeTcpClient (network, client, kSlices + 1));

            hr = controller.SeekToCycle (samples[0].cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::IsFalse (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"no client at that point");

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");
            Assert::IsFalse (client->destroyed, L"the real connection is open");

            machine.RunCycles (kSliceCycles);
            Assert::IsTrue (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"DCD on at the next poll");

            client->inbound.insert (client->inbound.end(), { 'H', 'I' });
            machine.RunCycles (kSlices / 2 * kSliceCycles);

            Assert::IsTrue (ReadReceived (machine).find ("HI") != std::string::npos, L"the client's bytes arrive at the guest");
            Assert::IsTrue (std::string (client->sent.begin(), client->sent.end()).ends_with ("HI"), L"and are echoed");
        }


        //  The call is up at a point in the past and live. Resuming there
        //  doesn't send NO CARRIER, and the BBS's next bytes arrive at the guest.
        TEST_METHOD (TheModemCallStaysUpAcrossAResume)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine    ("Apple2c", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialModem));
                services.hostNetwork = &network;
            });
            ReverseController                 controller (machine);
            std::shared_ptr<FakeSocketState>  server;
            std::vector<Sample>               samples;
            ReverseResult                     result;
            HRESULT                           hr         = S_OK;
            size_t                            waiting    = 0;



            network.SetHostAddress ("bbs.example.org", kBbs);
            PrepareDialer (machine, "ATDT bbs.example.org\r");

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, [&network, &server] (size_t slice)
            {
                (void) slice;

                if (server == nullptr)
                {
                    server = network.FindConnected ({ kBbs, 23 });

                    if (server != nullptr)
                    {
                        server->connect = FakeSocketState::ConnectOutcome::Accepted;
                    }
                }
            });

            hr = controller.SeekToCycle (samples[samples.size() - 2].cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::IsTrue (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"the call is up at that point");
            waiting = machine.GetRefs().serialChips[0]->GetQueuedReceiveCount();

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::IsFalse  (server->destroyed, L"the call is up on the PC");
            Assert::IsTrue   (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"DCD on for the guest");
            Assert::AreEqual (waiting, machine.GetRefs().serialChips[0]->GetQueuedReceiveCount(), L"no NO CARRIER");

            server->inbound.insert (server->inbound.end(), { 'H', 'I' });
            machine.RunCycles (8 * kSliceCycles);

            Assert::IsTrue (ReadReceived (machine).find ("HI") != std::string::npos, L"the BBS's bytes arrive at the guest");
        }


        //  The port is open across a resume, and DCD changed on the PC while
        //  the machine was behind live is in the 6551 at the next poll.
        TEST_METHOD (TheComPortStaysOpenAcrossAResume)
        {
            FakeHostSerialPorts                   ports;
            std::shared_ptr<FakeSerialPortState>  com3       = ports.AddPort ("COM3");
            TestMachine                           machine    ("Apple2c", TestMachine::Slots::AsShipped, [&ports] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialComPort));
                services.hostSerialPorts = &ports;
            });
            ReverseController                     controller (machine);
            std::vector<Sample>                   samples;
            ReverseResult                         result;
            int                                   opens      = 0;
            HRESULT                               hr         = S_OK;



            PrepareEcho (machine);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, [] (size_t slice) { (void) slice; });
            opens   = com3->opens;

            hr = controller.SeekToCycle (samples[samples.size() / 2].cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::IsTrue (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"DCD on at that point");

            com3->lines.dcd = false;

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::IsTrue   (com3->isOpen, L"the port is open");
            Assert::AreEqual (opens, com3->opens, L"and was not opened again");

            machine.RunCycles (kSliceCycles);
            Assert::IsFalse (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"DCD off at the next poll");

            com3->inbound.insert (com3->inbound.end(), { 'H', 'I' });
            machine.RunCycles (8 * kSliceCycles);

            Assert::IsTrue (ReadReceived (machine).find ("HI") != std::string::npos, L"the port's bytes arrive at the guest");
        }


        //  The bytes the port took after the point are received again on a
        //  resume, in order and each with its receive errors, as live.
        TEST_METHOD (AResumeFromThePastReceivesTheBytesAgainWithTheirErrors)
        {
            FakeHostSerialPorts                   ports;
            std::shared_ptr<FakeSerialPortState>  com3       = ports.AddPort ("COM3");
            TestMachine                           machine    ("Apple2c", TestMachine::Slots::AsShipped, [&ports] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialComPort));
                services.hostSerialPorts = &ports;
            });
            ReverseController                     controller (machine);
            CommEventRecorder                     recorder;
            std::vector<Sample>                   samples;
            std::vector<LoggedEvent>              live;
            std::vector<LoggedEvent>              again;
            ReverseResult                         result;
            size_t                                point      = (kSlices / kSampleEvery / 2) * kSampleEvery;
            size_t                                atPoint    = 0;
            size_t                                run        = 0;
            HRESULT                               hr         = S_OK;



            PrepareEcho (machine);
            machine.AttachCommEventSink (&recorder);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            samples = Record (machine, [&com3, &recorder, &atPoint, point] (size_t slice)
            {
                constexpr size_t  kFeedEvery = 4;

                if (slice == point)
                {
                    atPoint = recorder.events.size();
                }

                if (slice % kFeedEvery == 0)
                {
                    com3->inbound.insert (com3->inbound.end(), { (Byte) slice, (Byte) (slice + 1) });
                    com3->errors.isFraming = (slice % (kFeedEvery * 3)) == 0;
                }
            });

            live.assign (recorder.events.begin() + atPoint, recorder.events.end());
            Assert::IsTrue (std::ranges::any_of (live, [] (const LoggedEvent & each) { return each.reg != 0; }), L"bytes with errors after the point");

            hr = controller.SeekToCycle (samples[point / kSampleEvery].cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");

            recorder.events.clear();

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            for (run = point; run < kSlices; run++)
            {
                machine.RunCycles (kSliceCycles);
            }

            again.assign (recorder.events.begin(), recorder.events.begin() + std::min (recorder.events.size(), live.size()));
            Assert::AreEqual (live.size(), again.size(), L"each byte received again");
            Assert::IsTrue   (live == again, L"in order, each with its errors");

            machine.AttachCommEventSink (nullptr);
        }


        //  A state file saved with the line up has no connection behind it once
        //  loaded, so DCD is off, and the connection open before the load is
        //  closed.
        TEST_METHOD (ALoadedStateFileClosesTheTcpConnection)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine    ("Apple2c", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialTcp));
                services.hostNetwork = &network;
            });
            std::shared_ptr<FakeSocketState>  client;
            std::function<void (size_t)>      feed       = MakeTcpClient (network, client, kSlices + 1);
            std::vector<Byte>                 bytes;
            MachineStateContents              contents;
            MachineStateError                 error;
            size_t                            slice      = 0;
            HRESULT                           hr         = S_OK;



            PrepareEcho (machine);

            for (slice = 0; slice < kSlices / 2; slice++)
            {
                feed (slice);
                machine.RunCycles (kSliceCycles);
            }

            Assert::IsTrue (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"connected when the state is saved");

            hr = MachineStateFile::Build (machine, bytes, error);
            AssertSucceeded (hr, L"Build");

            machine.RunCycles (4 * kSliceCycles);

            hr = MachineStateFile::Parse (bytes, contents, error);
            AssertSucceeded (hr, L"Parse");

            hr = MachineStateFile::Apply (machine, contents, error);
            AssertSucceeded (hr, L"Apply");

            Assert::IsTrue  (client->destroyed, L"the real connection is closed");
            Assert::IsFalse (machine.GetRefs().serialChips[0]->IsDcdTrue(), L"DCD off for the guest");
        }


        //  FR-020: the serial log has each attachment's traffic, and a replay
        //  lists it again as live did.
        TEST_METHOD (ASerialLogReplayListsWhatLiveListed)
        {
            FakeHostNetwork                   network;
            TestMachine                       machine    ("Apple2c", TestMachine::Slots::AsShipped, [&network] (MachineConfig & config, MachineBuildServices & services)
            {
                AttachPort1 (config, MakeAttachment (kpszSerialTcp));
                services.hostNetwork = &network;
            });
            ReverseController                 controller (machine);
            CommEventRecorder                 recorder;
            std::shared_ptr<FakeSocketState>  client;
            std::vector<LoggedEvent>          live;
            ReverseResult                     result;
            HRESULT                           hr         = S_OK;



            PrepareEcho (machine);
            machine.AttachCommEventSink (&recorder);

            hr = controller.Start (ReverseSessionRig::MakeSettings (kOneKeyframe));
            AssertSucceeded (hr, L"Start");

            (void) Record (machine, MakeTcpClient (network, client, kSlices - 40));
            live = recorder.events;

            Assert::IsTrue (std::ranges::count_if (live, [] (const LoggedEvent & each) { return each.kind == IoEventKind::SerialConnection; }) == 2);
            Assert::IsTrue (std::ranges::count_if (live, [] (const LoggedEvent & each) { return each.kind == IoEventKind::SerialReceived; }) > 0);

            hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
            AssertSucceeded (hr, L"SeekToPosition oldest");

            recorder.events.clear();
            hr = controller.SeekToCycle (UINT64_MAX, result);
            AssertSucceeded (hr, L"SeekToCycle live");

            Assert::IsTrue (live == recorder.events, L"the replay lists the same connections and bytes in the same order");

            machine.AttachCommEventSink (nullptr);
        }
    };
}
