#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkPacketText.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "FakeApple.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeAppleContractTests
//
//  The session's unit tests use FakeApple in place of the program, so its
//  replies have to be the program's. Each case runs the same host requests
//  against the program on an emulated //c and against FakeApple, with the
//  same disks, and compares the packets each sends back. A change to the
//  protocol on one side and not the other fails here, with the first packet
//  that differs.
//
//  Ready is left out of the comparison: the program sends it again each
//  second it is idle, so how many arrive depends on timing, not protocol.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FakeAppleContractTests)
{
public:

    static constexpr Byte  kFastControl = 0x10;     // 115,200 baud
    static constexpr Byte  kTrack       = 17;

    //  The host's end of a line, with a copy of every byte the Apple sends.
    struct RecordingStream : IByteStream
    {
        std::shared_ptr<IByteStream>  inner;
        std::vector<Byte>             received;

        explicit RecordingStream (std::shared_ptr<IByteStream> i) : inner (std::move (i)) {}

        HRESULT  Write            (const Byte * data, size_t count) override { return inner->Write (data, count); }
        size_t   GetReadableCount () override                               { return inner->GetReadableCount(); }
        bool     IsClosed         () const override                         { return inner->IsClosed(); }
        void     Close            () override                               { inner->Close(); }

        HRESULT  Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override
        {
            HRESULT  hr = inner->Read (buffer, capacity, timeoutMs, outRead);



            if (SUCCEEDED (hr))
            {
                received.insert (received.end(), buffer, buffer + outRead);
            }

            return hr;
        }
    };

    //  One step of the host's: a name for failures, and the request.
    struct Step
    {
        const wchar_t *                           what;
        std::function<HRESULT (CassoLinkHost &)>  run;
    };

    //  The packets in `bytes`, Ready left out, each as its text.
    static std::vector<std::string>  Packets (const std::vector<Byte> & bytes)
    {
        std::vector<std::string>  packets;
        std::vector<Byte>         rest = bytes;



        while (!rest.empty())
        {
            CassoLinkPacket  packet;
            size_t           consumed = 0;
            bool             complete = false;

            if (FAILED (CassoLinkCodec::Decode (rest, consumed, packet, complete)) || consumed == 0)
            {
                packets.push_back (std::format ("undecoded byte ${:02X}", rest[0]));
                rest.erase (rest.begin());
                continue;
            }

            if (complete && packet.command != CassoLinkCodec::kReady)
            {
                packets.push_back (CassoLinkPacketText::Describe (packet.command, packet.payload) + " " + Hex (packet.payload));
            }

            rest.erase (rest.begin(), rest.begin() + (ptrdiff_t) consumed);
        }

        return packets;
    }

    static std::string  Hex (const std::vector<Byte> & bytes)
    {
        std::string  text;



        for (Byte b : bytes)
        {
            text += std::format ("{:02X}", b);
        }

        return text;
    }

    static std::vector<Byte>  Dos33()
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        return std::vector<Byte> (bytes.begin(), bytes.end());
    }

    //  The packets each step brings back from the program, and from
    //  FakeApple, in order.
    static void  Compare (const std::vector<Step> & steps)
    {
        std::vector<Byte>  disk = Dos33();
        ProgramRig         rig (disk, disk);
        auto               real = std::make_shared<RecordingStream> (rig.stream);
        CassoLinkHost      realHost (real, rig.folder, [] () { return std::array<Byte, 4> {}; });

        FakeApple                             apple;
        std::shared_ptr<InProcessByteStream>  appleEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::shared_ptr<RecordingStream>      fake;
        InMemoryCassoLinkFolder               folder;



        InProcessByteStream::CreatePair (appleEnd, pcEnd);
        apple.drives[0]   = disk;
        apple.drives[1]   = disk;
        apple.startAsShim = true;
        apple.Attach (appleEnd);

        fake = std::make_shared<RecordingStream> (pcEnd);

        {
            CassoLinkHost  fakeHost (fake, folder, [] () { return std::array<Byte, 4> {}; });

            for (const Step & step : steps)
            {
                std::vector<std::string>  fromProgram;
                std::vector<std::string>  fromFake;

                real->received.clear();
                fake->received.clear();

                AssertSucceeded (step.run (realHost));
                AssertSucceeded (step.run (fakeHost));

                fromProgram = Packets (real->received);
                fromFake    = Packets (fake->received);

                Assert::IsFalse (fromProgram.empty(), std::format (L"{}: the program's reply", step.what).c_str());

                for (size_t i = 0; i < std::max (fromProgram.size(), fromFake.size()); i++)
                {
                    std::string  p = i < fromProgram.size() ? fromProgram[i] : "(none)";
                    std::string  f = i < fromFake.size()    ? fromFake[i]    : "(none)";

                    Assert::AreEqual (p, f, std::format (L"{}: packet {}, the program's and FakeApple's", step.what, i).c_str());
                }
            }
        }

        apple.Detach();
    }



    TEST_METHOD (TheLoadAndASpeedChange_AreAnsweredAlike)
    {
        Compare ({ { L"Set speed, which loads the program", [] (CassoLinkHost & host) { return host.SetSpeed (kFastControl); } } });
    }



    TEST_METHOD (ReadsAndWrites_AreAnsweredAlike)
    {
        static std::vector<Byte>  s_sectors = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        static std::vector<Byte>  s_one     = { 0 };
        static std::vector<Byte>  s_pattern (256, 0x3C);

        CassoLinkStatus  status;



        Compare ({ { L"Set speed",            [] (CassoLinkHost & host) { return host.SetSpeed (kFastControl); } },
                   { L"Read a whole track",   [&] (CassoLinkHost & host) { std::vector<Byte> data; return host.ReadSectors (1, kTrack, s_sectors, data, status); } },
                   { L"Write a sector",       [&] (CassoLinkHost & host) { return host.WriteSectors (2, 20, s_one, s_pattern, status); } },
                   { L"Read it back",         [&] (CassoLinkHost & host) { std::vector<Byte> data; return host.ReadSectors (2, 20, s_one, data, status); } },
                   { L"Read CRCs",            [&] (CassoLinkHost & host) { std::vector<Word> crcs; std::vector<bool> read; return host.ReadCrcs (1, kTrack, s_sectors, crcs, read, status); } } });
    }
};
