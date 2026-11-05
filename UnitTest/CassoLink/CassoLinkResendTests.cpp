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
};