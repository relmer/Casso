#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/TextScreenScraper.h"
#include "CassoLink/CassoLinkCodec.h"
#include "Core/TextEncoding.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShimTests
//
//  The shim on an emulated //c: its program load, and its speed after a
//  program of another version is sent back to it. The PC's side of each is
//  the real host, or packets the codec builds, on the test's thread.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShimTests)
{
public:

    static constexpr DWORD     kReplyMs      = 3000;
    static constexpr Byte      kFastControl  = 0x10;      // 115,200 baud
    static constexpr int       kRowStatus    = 0;
    static constexpr int       kRowPort      = 4;
    static constexpr int       kVersionAt    = 5;         // the program's version, in its Ready packet
    static constexpr int       kCheckAt      = 16;        // and the packet's check
    static constexpr uint64_t  kSettleCycles = 500'000;   // for the program to draw its screen after a request

    //  The bytes the shim sends, within the reply time in the machine's
    //  cycles.
    static std::vector<Byte>  Receive (ProgramRig & rig)
    {
        std::vector<Byte>  bytes (4096);
        size_t             read = 0;



        AssertSucceeded (rig.stream->Read (bytes.data(), bytes.size(), kReplyMs, read));
        bytes.resize (read);
        return bytes;
    }

    //  Received bytes until a whole packet with this command has arrived.
    static void  AwaitPacket (ProgramRig & rig, Byte command)
    {
        std::vector<Byte>  received;
        CassoLinkPacket    packet;
        size_t             consumed = 0;
        bool               complete = false;



        for (int reads = 0; reads < 20 && !(complete && packet.command == command); reads++)
        {
            std::vector<Byte>  more = Receive (rig);

            received.insert (received.end(), more.begin(), more.end());
            AssertSucceeded (CassoLinkCodec::Decode (received, consumed, packet, complete));
            received.erase (received.begin(), received.begin() + (std::ptrdiff_t) consumed);
        }

        Assert::IsTrue (complete && packet.command == command, L"the packet arrives");
    }

    static std::string  Row (ProgramRig & rig, int row)
    {
        return TextScreenScraper::Scrape40 (rig.machine.GetMemoryBus(), 0x0400)[(size_t) row];
    }

    //  Where the program's Ready packet is in memory, from its header.
    static Word  FindReady (ProgramRig & rig)
    {
        static constexpr Byte  kHeader[] = { 0xC1, 0x0B, 0x00, CassoLinkCodec::kReady };

        for (Word at = LinkProgram::kProgramAddress; at < LinkProgram::kZeroPageSave; at++)
        {
            bool  match = true;

            for (size_t i = 0; i < std::size (kHeader); i++)
            {
                match = match && rig.machine.GetCpu()->ReadByte ((Word) (at + i)) == kHeader[i];
            }

            if (match)
            {
                return at;
            }
        }

        return 0;
    }



    //  A program page numbered past the program's last page gets a NAK and is
    //  not stored, and the shim then takes page 0.
    TEST_METHOD (APagePastTheProgram_IsNaked_AndNotStored)
    {
        ProgramRig         rig ({});
        std::vector<Byte>  page   (CassoLinkCodec::kPageSize, 0xEE);
        std::vector<Byte>  before (CassoLinkCodec::kPageSize);
        std::vector<Byte>  after  (CassoLinkCodec::kPageSize);
        std::vector<Byte>  wire;
        std::vector<Byte>  reply;
        Byte               count = 1;



        for (size_t i = 0; i < before.size(); i++)
        {
            before[i] = rig.machine.GetCpu()->ReadByte ((Word) (LinkProgram::kProgramAddress + 0x100 + i));
        }

        AwaitPacket (rig, CassoLinkCodec::kReady);

        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kLoadProgram, std::span<const Byte> (&count, 1), wire));
        AssertSucceeded (CassoLinkCodec::EncodeSectorData (1, 0, page, wire));
        AssertSucceeded (rig.pcEnd->Write (wire.data(), wire.size()));

        reply = Receive (rig);
        Assert::IsFalse (reply.empty());
        Assert::AreEqual<int> (CassoLinkCodec::kNak, reply[0]);

        for (size_t i = 0; i < after.size(); i++)
        {
            after[i] = rig.machine.GetCpu()->ReadByte ((Word) (LinkProgram::kProgramAddress + 0x100 + i));
        }

        Assert::IsTrue (before == after, L"the page past the program is not stored");

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeSectorData (0, 0, page, wire));
        AssertSucceeded (rig.pcEnd->Write (wire.data(), wire.size()));

        reply = Receive (rig);
        Assert::IsFalse (reply.empty());
        Assert::AreEqual<int> (CassoLinkCodec::kAck, reply[0]);
    }



    //  A shim built at the link's speed takes the program, and the program's
    //  status is CONNECTED.
    TEST_METHOD (AShimAtTheLinksSpeed_LoadsTheProgram_AndConnects)
    {
        ProgramRig  rig ({});



        AssertSucceeded (rig.host->SetColumns (false));
        rig.machine.RunCycles (kSettleCycles);

        Assert::AreEqual<int> (LinkProgram::kProgramVersion, rig.host->GetClient().programVersion);
        Assert::IsTrue (Row (rig, kRowStatus).starts_with ("CASSOLINK: CONNECTED"), TextEncoding::NarrowToWide (Row (rig, kRowStatus)).c_str());
    }



    //  A program of another version is sent Load program and goes back to
    //  the shim. The shim waits at the speed port 2 is at, the link's, and
    //  not at the 9,600 it was built for, where the host is not.
    TEST_METHOD (AnOlderProgram_ReloadsAtTheLinksSpeed)
    {
        ProgramRig  rig ({}, {}, 9600);
        Word        ready   = 0;
        Byte        version = 0;
        bool        seen    = false;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        ready = FindReady (rig);
        Assert::AreNotEqual ((Word) 0, ready, L"the program's Ready packet");

        //  The Ready packet's version, and its check with it, as an older
        //  program's.
        version = rig.machine.GetCpu()->ReadByte ((Word) (ready + kVersionAt));
        rig.machine.GetCpu()->WriteByte ((Word) (ready + kVersionAt), (Byte) (version - 1));
        rig.machine.GetCpu()->WriteByte ((Word) (ready + kCheckAt), (Byte) (rig.machine.GetCpu()->ReadByte ((Word) (ready + kCheckAt)) ^ version ^ (Byte) (version - 1)));

        //  A new host, as after the PC connects again.
        rig.host = std::make_unique<CassoLinkHost> (rig.stream, rig.folder, [] () { return std::array<Byte, 4> {}; });
        rig.stream->SetStepHook ([&] () { seen = seen || Row (rig, kRowPort).starts_with ("SERIAL PORT 2, 115200"); });

        AssertSucceeded (rig.host->SetColumns (false));

        Assert::AreEqual<int> (version, rig.machine.GetCpu()->ReadByte ((Word) (ready + kVersionAt)), L"the current program, loaded again");
        Assert::IsTrue (seen, L"the shim waited at the link's speed");
    }
};
