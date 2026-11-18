#include "Pch.h"

#include "InMemoryCassoLinkFolder.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResendTests
//
//  A reply from the Apple damaged on the line: the host sends the request
//  again, and the read succeeds from the next reply.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkResendTests)
{
public:

    static constexpr Byte  kTrack = 12;

    struct Apple
    {
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::shared_ptr<InProcessByteStream>  appleEnd;
        InMemoryCassoLinkFolder               folder;
        std::unique_ptr<CassoLinkHost>        host;
        std::vector<Byte>                     buffer;
        std::vector<std::string>              activity;

        Apple()
        {
            InProcessByteStream::CreatePair (hostEnd, appleEnd);

            host = std::make_unique<CassoLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetOperationTimeout (5000);
            host->SetActivitySink ([this] (const std::string & text) { activity.push_back (text); });
        }

        void  Send (std::span<const Byte> bytes)
        {
            Assert::AreEqual (S_OK, appleEnd->Write (bytes.data(), bytes.size()));
        }

        void  SendPacket (Byte command, std::vector<Byte> payload)
        {
            std::vector<Byte>  wire;

            Assert::AreEqual (S_OK, CassoLinkCodec::Encode (command, payload, wire));
            Send (wire);
        }

        void  SendReady()
        {
            SendPacket (CassoLinkCodec::kReady, { (Byte) (LinkProgram::kProgramVersion & 0xFF), (Byte) (LinkProgram::kProgramVersion >> 8), LinkProgram::kShimVersion, 0x03 });
        }

        //  The sector's packet, with one byte of its data flipped when damaged.
        void  SendSector (Byte sector, bool damaged)
        {
            std::vector<Byte>  wire;

            Assert::AreEqual (S_OK, CassoLinkCodec::EncodeSectorData (kTrack, sector, Pattern (sector), wire));

            if (damaged)
            {
                wire[wire.size() / 2] ^= 0xFF;
            }

            Send (wire);
        }

        void  SendStatus (Byte done)
        {
            SendPacket (CassoLinkCodec::kStatus, { 1, 2, kTrack, done, CassoLinkHost::kErrorNone });
        }

        CassoLinkPacket  ReceivePacket()
        {
            CassoLinkPacket  packet;
            size_t           consumed = 0;
            bool             complete = false;
            Byte             chunk[4096];
            size_t           got      = 0;
            auto             until    = std::chrono::steady_clock::now() + std::chrono::seconds (5);

            for (;;)
            {
                Assert::AreEqual (S_OK, CassoLinkCodec::Decode (buffer, consumed, packet, complete));

                if (complete)
                {
                    buffer.erase (buffer.begin(), buffer.begin() + (ptrdiff_t) consumed);
                    return packet;
                }

                Assert::IsTrue (std::chrono::steady_clock::now() < until, L"a packet from the host");
                Assert::AreEqual (S_OK, appleEnd->Read (chunk, sizeof (chunk), 50, got));
                buffer.insert (buffer.end(), chunk, chunk + got);
            }
        }

        //  The Apple's side of one Read sectors, from its Ready to its Status.
        void  ServeRead (std::span<const Byte> sectors, int damagedIndex)
        {
            CassoLinkPacket  request;

            SendReady();

            request = ReceivePacket();
            Assert::AreEqual<int> (CassoLinkCodec::kReadSectors, request.command);

            for (size_t i = 0; i < sectors.size(); i++)
            {
                SendSector (sectors[i], (int) i == damagedIndex);
            }

            SendStatus ((Byte) sectors.size());
        }
    };

    static std::vector<Byte>  Pattern (Byte sector)
    {
        std::vector<Byte>  bytes (CassoLinkHost::kSectorSize);

        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = (Byte) (sector * 31 + i * 7);
        }

        return bytes;
    }



    TEST_METHOD (ADamagedSector_IsReadAgain)
    {
        Apple              apple;
        const Byte         sectors[] = { 0, 1, 2, 3 };
        std::vector<Byte>  data;
        std::vector<Byte>  expected;
        CassoLinkStatus    status;
        auto               reading   = std::async (std::launch::async, [&] () { return apple.host->ReadSectors (2, kTrack, sectors, data, status); });



        apple.ServeRead (sectors, 2);
        apple.ServeRead (sectors, -1);

        Assert::AreEqual (S_OK, reading.get());

        for (Byte sector : sectors)
        {
            std::vector<Byte>  one = Pattern (sector);

            expected.insert (expected.end(), one.begin(), one.end());
        }

        Assert::IsTrue (data == expected, L"every sector, from the second reply");
        Assert::AreEqual ((size_t) 1, apple.activity.size());
        Assert::IsTrue (apple.activity[0].find ("damaged") != std::string::npos);
    }



    TEST_METHOD (DamageOnEveryReply_FailsAfterTheResends)
    {
        Apple              apple;
        const Byte         sectors[] = { 0, 1 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        auto               reading   = std::async (std::launch::async, [&] () { return apple.host->ReadSectors (2, kTrack, sectors, data, status); });



        for (int i = 0; i <= CassoLinkHost::kMaxResends; i++)
        {
            apple.ServeRead (sectors, 0);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), reading.get());
    }



    //  A krunch runs on the Apple once it has the request, so a damaged
    //  progress packet is dropped and the next one read; the request is not
    //  sent again.
    TEST_METHOD (AKrunchsDamagedProgress_IsDropped_AndTheRequestNotSentAgain)
    {
        Apple              apple;
        CassoLinkStatus    status;
        CassoLinkPacket    request;
        std::vector<Byte>  wire;
        std::vector<int>   reported;
        auto               krunching = std::async (std::launch::async, [&] () { return apple.host->Krunch (1, 0x1234, [&] (Byte percent, Byte) { reported.push_back (percent); }, status); });



        apple.SendReady();
        request = apple.ReceivePacket();
        Assert::AreEqual<int> (CassoLinkCodec::kKrunch, request.command);
        Assert::IsTrue (request.payload == std::vector<Byte> { 1, 0x34, 0x12 }, L"the drive and the CRC, low byte first");

        Assert::AreEqual (S_OK, CassoLinkCodec::Encode (CassoLinkCodec::kKrunchProgress, std::vector<Byte> { 10, 0 }, wire));
        wire.back() ^= 0xFF;
        apple.Send (wire);
        apple.SendPacket (CassoLinkCodec::kKrunchProgress, { 60, 1 });
        apple.SendStatus (1);

        Assert::AreEqual (S_OK, krunching.get());
        Assert::IsTrue (reported == std::vector<int> { 60 });
        Assert::AreEqual<int> (1, status.done);
        Assert::IsTrue (apple.activity.empty(), L"not sent again");
    }



    //  Two Readies with no progress are a request the Apple didn't take, which
    //  is sent again.
    TEST_METHOD (AKrunchNotTaken_IsSentAgain)
    {
        Apple            apple;
        CassoLinkStatus  status;
        auto             krunching = std::async (std::launch::async, [&] () { return apple.host->Krunch (1, 0x1234, nullptr, status); });



        apple.SendReady();
        Assert::AreEqual<int> (CassoLinkCodec::kKrunch, apple.ReceivePacket().command);
        apple.SendReady();
        apple.SendReady();

        Assert::AreEqual<int> (CassoLinkCodec::kKrunch, apple.ReceivePacket().command, L"sent again");
        apple.SendPacket (CassoLinkCodec::kKrunchProgress, { 0, 0 });
        apple.SendStatus (0);

        Assert::AreEqual (S_OK, krunching.get());
        Assert::AreEqual ((size_t) 1, apple.activity.size());
    }



    //  Two Readies after progress are a Status lost on the line: the Apple
    //  took the request, and a second krunch would run against a directory
    //  that has changed, so it is not sent again.
    TEST_METHOD (AKrunchsLostStatus_IsDamage_AndTheRequestNotSentAgain)
    {
        Apple            apple;
        CassoLinkStatus  status;
        auto             krunching = std::async (std::launch::async, [&] () { return apple.host->Krunch (1, 0x1234, nullptr, status); });



        apple.SendReady();
        Assert::AreEqual<int> (CassoLinkCodec::kKrunch, apple.ReceivePacket().command);
        apple.SendPacket (CassoLinkCodec::kKrunchProgress, { 0, 0 });
        apple.SendReady();
        apple.SendReady();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), krunching.get());
        Assert::IsTrue (apple.activity.empty(), L"not sent again");
    }
};