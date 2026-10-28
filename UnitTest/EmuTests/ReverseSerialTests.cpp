#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Core/MachineConfig.h"
#include "Devices/Acia6551.h"
#include "Devices/IAciaEndpoint.h"
#include "Shell/ScratchReplayMachine.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kSerialSliceCycles = 3000;
static constexpr size_t    s_kSerialSlices      = 240;
static constexpr size_t    s_kSerialSampleEvery = 7;
static constexpr size_t    s_kSerialShuffle     = 5;
static constexpr size_t    s_kSerialFeedEvery   = 4;
static constexpr Word      s_kSerialProgram     = 0x0800;
static constexpr Word      s_kSerialStore       = 0x0300;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSerialTests
//
//  Bytes a host attachment feeds a 6551 arrive from outside the machine, so
//  a replay has them only from the input journal. A //c runs a loop that
//  stores each byte its first serial port receives, while the test feeds
//  bytes between slices as a host attachment's thread would. After a seek
//  back to each sample, the machine's state must match the live state there.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseSerialTests)
{
public:

    //  An attachment that stores every byte the port sends.
    class CapturingEndpoint : public IAciaEndpoint
    {
    public:
        void  OnByteTransmitted (Byte value) override { bytes.push_back (value); }

        std::vector<Byte>  bytes;
    };

    struct Sample
    {
        uint64_t           position = 0;
        uint64_t           cycle    = 0;
        std::vector<Byte>  state;
    };


    TEST_METHOD (HostFedBytesAreReplayedFromTheJournal)
    {
        TestMachine          machine    ("Apple2c");
        ReverseController    controller (machine);
        std::vector<Sample>  samples;
        Sample               last;
        ReverseResult        result;
        HRESULT              hr         = S_OK;
        size_t               count      = 0;
        size_t               i          = 0;



        PrepareReceiver (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        samples = RecordSession (machine, true);
        last    = TakeSample (machine);
        count   = samples.size();

        Assert::IsTrue (CountRecords (machine, InputKind::AciaByte) > 0, L"the bytes the port took are journaled");
        Assert::AreNotEqual<Byte> (0, machine.GetMemoryBus().ReadByte (s_kSerialStore), L"the guest must have received bytes");

        for (i = 0; i < count; i++)
        {
            const Sample  & sample = samples[(i * s_kSerialShuffle) % count];

            hr = controller.SeekToCycle (sample.cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"a seek inside history moves");
            Assert::AreEqual<uint64_t> (sample.position, result.position, L"the seek lands on the sampled boundary");
            CheckState (sample.state, ReverseSessionRig::Save (machine), sample.position);
        }

        hr = controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");

        CheckState (last.state, ReverseSessionRig::Save (machine), last.position);
        Assert::IsFalse (controller.IsInHistory(), L"at the live end the machine is live again");
    }


    //  The //c's ports have a loopback plug: a byte the guest sends comes
    //  back to it. A replay sends it again, so the journal has none.
    TEST_METHOD (ALoopedBackByteIsNotJournaled)
    {
        TestMachine          machine    ("Apple2c", TestMachine::PlugInSerialLoopbacks);
        ReverseController    controller (machine);
        std::vector<Sample>  samples;
        ReverseResult        result;
        HRESULT              hr         = S_OK;
        size_t               count      = 0;
        size_t               i          = 0;



        PrepareLoopback (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        samples = RecordSession (machine, false);
        count   = samples.size();

        Assert::AreEqual<Byte> (1, machine.GetMemoryBus().ReadByte (s_kSerialStore + 1), L"the guest must have read back what it sent");
        Assert::AreEqual<size_t> (0, CountRecords (machine, InputKind::AciaByte), L"nothing from the plug is journaled");

        for (i = 0; i < count; i++)
        {
            const Sample  & sample = samples[(i * s_kSerialShuffle) % count];

            hr = controller.SeekToCycle (sample.cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");

            CheckState (sample.state, ReverseSessionRig::Save (machine), sample.position);
        }
    }


    //  Behind live the journal supplies the bytes, so one the host sends then
    //  waits: the seek to the live end arrives at exactly the machine that was
    //  live, and the byte is taken, and journaled, once the machine runs live.
    TEST_METHOD (ABytePushedBehindLiveWaitsUntilLive)
    {
        static constexpr Byte  kLate = 0xA5;
        TestMachine            machine    ("Apple2c");
        ReverseController      controller (machine);
        std::vector<Sample>    samples;
        Sample                 last;
        ReverseResult          result;
        HRESULT                hr         = S_OK;
        size_t                 before     = 0;
        size_t                 waiting    = 0;



        PrepareReceiver (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        samples = RecordSession (machine, true);
        last    = TakeSample (machine);
        before  = CountRecords (machine, InputKind::AciaByte);
        waiting = machine.GetRefs().serialChips[0]->GetQueuedReceiveCount();

        hr = controller.SeekToCycle (samples[samples.size() / 2].cycle, result);
        AssertSucceeded (hr, L"SeekToCycle");
        Assert::IsTrue (controller.IsInHistory());

        machine.GetRefs().serialChips[0]->EnqueueReceivedBytes (&kLate, 1);

        hr = controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");

        CheckState (last.state, ReverseSessionRig::Save (machine), last.position);
        Assert::AreEqual<size_t> (waiting + 1, machine.GetRefs().serialChips[0]->GetQueuedReceiveCount(), L"the byte waits in the queue");

        machine.RunCycles (s_kSerialSliceCycles);

        Assert::AreEqual<size_t> (before + 1, CountRecords (machine, InputKind::AciaByte), L"taken live and journaled");
    }


    //  A change made in the past makes the machine live there. The bytes the
    //  port took from the host after that point go to the receiver again,
    //  so the guest gets them without the host sending them again.
    TEST_METHOD (AResumeFromThePastReceivesTheBytesTakenSince)
    {
        static constexpr Byte  kBytes[] = { 'H', 'E', 'L', 'L', 'O' };
        static constexpr Word  kStored  = 0x0300;
        TestMachine            machine    ("Apple2c");
        ReverseController      controller (machine);
        ReverseResult          result;
        HRESULT                hr         = S_OK;
        uint64_t               point      = 0;
        size_t                 i          = 0;



        PrepareReceiver (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        RecordSession (machine, false);
        point = machine.GetCpu()->GetTotalCycles();

        machine.GetRefs().serialChips[0]->EnqueueReceivedBytes (kBytes, sizeof (kBytes));
        RecordSession (machine, false);

        Assert::AreEqual (kBytes[4], machine.GetMemoryBus().ReadByte (kStored + 4), L"the guest received them live");

        hr = controller.SeekToCycle (point, result);
        AssertSucceeded (hr, L"SeekToCycle");
        Assert::AreNotEqual (kBytes[0], machine.GetMemoryBus().ReadByte (kStored), L"and not yet at the point");

        hr = controller.OnMachineChanged();
        AssertSucceeded (hr, L"OnMachineChanged");

        RecordSession (machine, false);

        for (i = 0; i < sizeof (kBytes); i++)
        {
            Assert::AreEqual (kBytes[i], machine.GetMemoryBus().ReadByte ((Word) (kStored + i)), L"each byte, in order, received again");
        }
    }


    //  What the guest sends was sent live: a seek back and a seek to the live
    //  end, both replays, send nothing to the attachment, and the machine
    //  sends again once it runs live.
    TEST_METHOD (AReplaySendsNothingToTheAttachment)
    {
        TestMachine          machine    ("Apple2c");
        ReverseController    controller (machine);
        CapturingEndpoint    capture;
        ReverseResult        result;
        HRESULT              hr         = S_OK;
        size_t               sent       = 0;



        PrepareSender (machine);
        machine.GetRefs().serialChips[0]->SetEndpoint (&capture);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        RecordSession (machine, false);
        sent = capture.bytes.size();
        Assert::IsTrue (sent > 0, L"live, the port sends");

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition oldest");

        hr = controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");

        Assert::AreEqual (sent, capture.bytes.size(), L"the replays send nothing");

        machine.RunCycles (s_kSerialSliceCycles);
        Assert::IsTrue (capture.bytes.size() > sent, L"live again, the port sends");

        machine.GetRefs().serialChips[0]->SetEndpoint (nullptr);
    }


    //  A resume from the past drops the host's connections, the serial
    //  attachments' among them.
    TEST_METHOD (DroppingHostConnectionsReachesEachSerialAttachment)
    {
        class DroppingEndpoint : public IAciaEndpoint
        {
        public:
            void  OnByteTransmitted (Byte value) override { (void) value; }
            void  DropConnection    () override           { drops++; }

            int  drops = 0;
        };

        TestMachine       machine ("Apple2c");
        DroppingEndpoint  port1;
        DroppingEndpoint  port2;



        machine.GetRefs().serialChips[0]->SetEndpoint (&port1);
        machine.GetRefs().serialChips[1]->SetEndpoint (&port2);

        machine.DropHostConnections();

        Assert::AreEqual (1, port1.drops);
        Assert::AreEqual (1, port2.drops);

        machine.GetRefs().serialChips[0]->SetEndpoint (nullptr);
        machine.GetRefs().serialChips[1]->SetEndpoint (nullptr);
    }

    //  The heat map's and the call record's replays run on a scratch machine
    //  built from the running machine's config, attachments and all, and
    //  what it sends was sent live.
    TEST_METHOD (AScratchReplayMachineIsMuted)
    {
        ScratchReplayMachine  scratch;
        MachineConfig         config;
        HRESULT               hr      = S_OK;



        TestMachine::LoadConfig ("Apple2c", config);
        scratch.SetMachine (config, L"Apple2c");

        hr = scratch.Build();
        AssertSucceeded (hr, L"Build");

        Assert::IsTrue (scratch.GetMachine()->IsOutputMuted(), L"the scratch machine is muted");
    }


private:

    //  Port 1 at 9600 baud, eight bits and DTR on; the loop sends X each
    //  time the transmitter is empty, counting up.
    static void PrepareSender (TestMachine & machine)
    {
        static constexpr Byte  kLoop[] =
        {
            0x78,                   // 0800  SEI
            0xA2, 0x00,             // 0801  LDX #$00
            0xA9, 0x0B,             // 0803  LDA #$0B
            0x8D, 0x9A, 0xC0,       // 0805  STA $C09A    command: DTR on, receive IRQ off
            0xA9, 0x1E,             // 0808  LDA #$1E
            0x8D, 0x9B, 0xC0,       // 080A  STA $C09B    control: 9600 baud, 8 bits
            0xAD, 0x99, 0xC0,       // 080D  LDA $C099    status
            0x29, 0x10,             // 0810  AND #$10     transmitter empty
            0xF0, 0xF9,             // 0812  BEQ $080D
            0x8E, 0x98, 0xC0,       // 0814  STX $C098    send X
            0xE8,                   // 0817  INX
            0x4C, 0x0D, 0x08,       // 0818  JMP $080D
        };
        size_t  i = 0;



        machine.PowerCycle();

        for (i = 0; i < sizeof (kLoop); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (s_kSerialProgram + i), kLoop[i]);
        }

        machine.GetCpu()->SetPC (s_kSerialProgram);
    }


    //  Port 1 set to 9600 baud, eight bits, DTR on and its receive interrupt
    //  off; the loop then stores each byte it receives at $0300,X.
    static void PrepareReceiver (TestMachine & machine)
    {
        static constexpr Byte  kLoop[] =
        {
            0x78,                   // 0800  SEI
            0xA2, 0x00,             // 0801  LDX #$00
            0xA9, 0x0B,             // 0803  LDA #$0B
            0x8D, 0x9A, 0xC0,       // 0805  STA $C09A    command: DTR on, receive IRQ off
            0xA9, 0x1E,             // 0808  LDA #$1E
            0x8D, 0x9B, 0xC0,       // 080A  STA $C09B    control: 9600 baud, 8 bits
            0xAD, 0x99, 0xC0,       // 080D  LDA $C099    status
            0x29, 0x08,             // 0810  AND #$08     receiver full
            0xF0, 0xF9,             // 0812  BEQ $080D
            0xAD, 0x98, 0xC0,       // 0814  LDA $C098    data
            0x9D, 0x00, 0x03,       // 0817  STA $0300,X
            0xE8,                   // 081A  INX
            0x4C, 0x0D, 0x08,       // 081B  JMP $080D
        };
        size_t  i = 0;



        machine.PowerCycle();

        for (i = 0; i < sizeof (kLoop); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (s_kSerialProgram + i), kLoop[i]);
        }

        machine.GetCpu()->SetPC (s_kSerialProgram);
    }


    //  The same port and settings; the loop sends X, waits for the byte to
    //  come back through the plug and stores it at $0300,X.
    static void PrepareLoopback (TestMachine & machine)
    {
        static constexpr Byte  kLoop[] =
        {
            0x78,                   // 0800  SEI
            0xA2, 0x00,             // 0801  LDX #$00
            0xA9, 0x0B,             // 0803  LDA #$0B
            0x8D, 0x9A, 0xC0,       // 0805  STA $C09A    command: DTR on, receive IRQ off
            0xA9, 0x1E,             // 0808  LDA #$1E
            0x8D, 0x9B, 0xC0,       // 080A  STA $C09B    control: 9600 baud, 8 bits
            0xAD, 0x99, 0xC0,       // 080D  LDA $C099    status
            0x29, 0x10,             // 0810  AND #$10     transmitter empty
            0xF0, 0xF9,             // 0812  BEQ $080D
            0x8E, 0x98, 0xC0,       // 0814  STX $C098    send X
            0xAD, 0x99, 0xC0,       // 0817  LDA $C099
            0x29, 0x08,             // 081A  AND #$08     receiver full
            0xF0, 0xF9,             // 081C  BEQ $0817
            0xAD, 0x98, 0xC0,       // 081E  LDA $C098
            0x9D, 0x00, 0x03,       // 0821  STA $0300,X
            0xE8,                   // 0824  INX
            0x4C, 0x0D, 0x08,       // 0825  JMP $080D
        };
        size_t  i = 0;



        machine.PowerCycle();

        for (i = 0; i < sizeof (kLoop); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (s_kSerialProgram + i), kLoop[i]);
        }

        machine.GetCpu()->SetPC (s_kSerialProgram);
    }


    //  Every few slices, three bytes from the host at the start of a slice.
    static std::vector<Sample> RecordSession (TestMachine & machine, bool isFeeding)
    {
        std::vector<Sample>  samples;
        Byte                 bytes[3] = {};
        size_t               slice    = 0;



        for (slice = 0; slice < s_kSerialSlices; slice++)
        {
            if (slice % s_kSerialSampleEvery == 0)
            {
                samples.push_back (TakeSample (machine));
            }

            if (isFeeding && slice % s_kSerialFeedEvery == s_kSerialFeedEvery - 1)
            {
                bytes[0] = static_cast<Byte> (slice);
                bytes[1] = static_cast<Byte> (slice + 1);
                bytes[2] = static_cast<Byte> (slice + 2);

                machine.GetRefs().serialChips[0]->EnqueueReceivedBytes (bytes, sizeof (bytes));
            }

            machine.RunCycles (s_kSerialSliceCycles);
        }

        return samples;
    }


    static Sample TakeSample (MachineHost & machine)
    {
        Sample  sample;



        sample.position = machine.GetPosition();
        sample.cycle    = machine.GetCpu()->GetTotalCycles();
        sample.state    = ReverseSessionRig::Save (machine);

        return sample;
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
};
