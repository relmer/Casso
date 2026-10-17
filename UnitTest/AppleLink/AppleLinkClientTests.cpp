#include "Pch.h"

#include "InMemoryAppleLinkFolder.h"
#include "AppleLink/AppleLinkCodec.h"
#include "AppleLink/AppleLinkHost.h"
#include "AppleLink/LinkProgram.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkClientTests
//
//  The host against a scripted Casso client: the test sends and checks the
//  Apple's end of the line packet by packet, while the host's operation runs
//  on its own thread.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLinkClientTests)
{
public:

    struct Client
    {
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::shared_ptr<InProcessByteStream>  clientEnd;
        InMemoryAppleLinkFolder               folder;
        std::unique_ptr<AppleLinkHost>        host;
        std::vector<Byte>                     buffer;

        Client()
        {
            InProcessByteStream::CreatePair (hostEnd, clientEnd);

            host = std::make_unique<AppleLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetOperationTimeout (5000);
        }

        void  Send (std::span<const Byte> bytes)
        {
            Assert::AreEqual (S_OK, clientEnd->Write (bytes.data(), bytes.size()));
        }

        void  SendPacket (Byte command, std::vector<Byte> payload)
        {
            std::vector<Byte>  wire;

            Assert::AreEqual (S_OK, AppleLinkCodec::Encode (command, payload, wire));
            Send (wire);
        }

        void  SendReady (Word version)
        {
            SendPacket (AppleLinkCodec::kReady, { (Byte) (version & 0xFF), (Byte) (version >> 8), LinkProgram::kShimVersion, 0x03 });
        }

        void  SendSector (Byte track, Byte sector, std::span<const Byte> data)
        {
            std::vector<Byte>  wire;

            Assert::AreEqual (S_OK, AppleLinkCodec::EncodeSectorData (track, sector, data, wire));
            Send (wire);
        }

        void  SendStatus (Byte operation, Byte drive, Byte track, Byte done, Byte error)
        {
            SendPacket (AppleLinkCodec::kStatus, { operation, drive, track, done, error });
        }

        void  SendAcknowledge (Byte kind, Byte sector, Byte track)
        {
            SendPacket (AppleLinkCodec::kAcknowledge, { kind, sector, track });
        }

        bool  Fill (size_t count)
        {
            Byte    chunk[4096];
            size_t  got   = 0;
            auto    until = std::chrono::steady_clock::now() + std::chrono::seconds (5);

            while (buffer.size() < count && std::chrono::steady_clock::now() < until)
            {
                Assert::AreEqual (S_OK, clientEnd->Read (chunk, sizeof (chunk), 50, got));
                buffer.insert (buffer.end(), chunk, chunk + got);
            }

            return buffer.size() >= count;
        }

        Byte  ReceiveByte()
        {
            Byte  value = 0;

            Assert::IsTrue (Fill (1), L"a byte from the host");

            value = buffer[0];
            buffer.erase (buffer.begin());
            return value;
        }

        AppleLinkPacket  ReceivePacket()
        {
            AppleLinkPacket  packet;
            size_t           consumed = 0;
            bool             complete = false;

            for (;;)
            {
                Assert::AreEqual (S_OK, AppleLinkCodec::Decode (buffer, consumed, packet, complete));

                if (complete)
                {
                    buffer.erase (buffer.begin(), buffer.begin() + (ptrdiff_t) consumed);
                    return packet;
                }

                Assert::IsTrue (Fill (buffer.size() + 1), L"a packet from the host");
            }
        }

        bool  IsQuiet()
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (200));
            return buffer.empty() && clientEnd->GetReadableCount() == 0;
        }
    };

    static std::vector<Byte>  Pattern (size_t size, Byte seed)
    {
        std::vector<Byte>  bytes (size);

        for (size_t i = 0; i < size; i++)
        {
            bytes[i] = (Byte) (seed + i * 7);
        }

        return bytes;
    }

    static std::vector<Byte>  Program()
    {
        std::vector<Byte>  bytes;
        std::string        errors;

        Assert::AreEqual (S_OK, LinkProgram::AssembleProgram (bytes, errors));
        return bytes;
    }




    TEST_METHOD (Ready_FromTheCurrentProgram_HasIdleInReply)
    {
        Client           client;
        AppleLinkPacket  reply;

        client.SendReady (LinkProgram::kProgramVersion);
        Assert::AreEqual (S_OK, client.host->ServeOne (1000));

        reply = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kIdle, reply.command);
        Assert::IsTrue (client.host->HasClient());
        Assert::AreEqual<int> (0x03, client.host->GetClient().driveMap);
    }




    TEST_METHOD (Ready_FromTheShim_LoadsTheProgram_ResendingANakedPage)
    {
        Client             client;
        std::vector<Byte>  program = Program();
        std::vector<Byte>  loaded;
        AppleLinkPacket    packet;
        Byte               track   = 0;
        Byte               sector  = 0;
        std::vector<Byte>  page (256);
        size_t             pages   = (program.size() + 255) / 256;
        auto               serving = std::async (std::launch::async, [&] () { return client.host->ServeOne (1000); });

        client.SendReady (0);

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kLoadProgram, packet.command);
        Assert::AreEqual (pages, (size_t) packet.payload.at (0));

        for (size_t i = 0; i < pages; i++)
        {
            packet = client.ReceivePacket();
            Assert::AreEqual (S_OK, AppleLinkCodec::ParseSectorData (packet, track, sector, page));
            Assert::AreEqual (i, (size_t) track);

            if (i == 1)
            {
                client.Send (std::vector<Byte> { AppleLinkCodec::kNak });
                packet = client.ReceivePacket();
                Assert::AreEqual (S_OK, AppleLinkCodec::ParseSectorData (packet, track, sector, page));
                Assert::AreEqual (i, (size_t) track, L"the same page again");
            }

            loaded.insert (loaded.end(), page.begin(), page.end());
            client.Send (std::vector<Byte> { AppleLinkCodec::kAck });
        }

        Assert::AreEqual (S_OK, serving.get());

        loaded.resize (program.size());
        Assert::IsTrue (loaded == program);
        Assert::IsTrue (client.IsQuiet());
    }




    TEST_METHOD (Ready_FromAnotherProgram_ReturnsItToTheShim)
    {
        Client           client;
        AppleLinkPacket  packet;

        client.SendReady (LinkProgram::kProgramVersion + 1);
        Assert::AreEqual (S_OK, client.host->ServeOne (1000));

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kLoadProgram, packet.command);
        Assert::IsTrue (client.IsQuiet(), L"no pages before the shim's Ready");
    }




    TEST_METHOD (ReadSectors_ReturnsTheClientsData)
    {
        Client             client;
        std::vector<Byte>  first     = Pattern (256, 1);
        std::vector<Byte>  second    = Pattern (256, 99);
        std::vector<Byte>  data;
        AppleLinkStatus    status;
        AppleLinkPacket    request;
        const Byte         sectors[] = { 5, 9 };
        auto               reading = std::async (std::launch::async, [&] () { return client.host->ReadSectors (1, 17, sectors, data, status); });

        client.SendReady (LinkProgram::kProgramVersion);
        client.SendReady (LinkProgram::kProgramVersion);

        request = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kReadSectors, request.command);
        Assert::IsTrue (request.payload == std::vector<Byte> { 1, 17, 2, 5, 9, 0, 100 });

        client.SendSector (17, 5, first);
        client.SendSector (17, 9, second);
        client.SendStatus (1, 1, 17, 2, AppleLinkHost::kErrorNone);

        Assert::AreEqual (S_OK, reading.get());
        Assert::IsTrue (std::equal (first.begin(), first.end(), data.begin()));
        Assert::IsTrue (std::equal (second.begin(), second.end(), data.begin() + 256));
        Assert::AreEqual<int> (2, status.done);
        Assert::AreEqual<int> (AppleLinkHost::kErrorNone, status.error);
    }




    TEST_METHOD (ReadSectors_ADamagedSectorFails)
    {
        Client             client;
        std::vector<Byte>  wire;
        std::vector<Byte>  data;
        AppleLinkStatus    status;
        const Byte         sectors[] = { 0 };
        auto               reading = std::async (std::launch::async, [&] () { return client.host->ReadSectors (1, 0, sectors, data, status); });

        client.SendReady (LinkProgram::kProgramVersion);
        client.ReceivePacket();

        Assert::AreEqual (S_OK, AppleLinkCodec::EncodeSectorData (0, 0, Pattern (256, 3), wire));
        wire[wire.size() - 3] ^= 0xFF;
        client.Send (wire);

        HRESULT  hr = reading.get();

        Assert::IsTrue (FAILED (hr));
    }




    TEST_METHOD (ReadSectors_AnUnreadableSectorEndsWithItsStatus)
    {
        Client             client;
        std::vector<Byte>  data;
        AppleLinkStatus    status;
        const Byte         sectors[] = { 3, 4 };
        auto               reading = std::async (std::launch::async, [&] () { return client.host->ReadSectors (2, 30, sectors, data, status); });

        client.SendReady (LinkProgram::kProgramVersion);
        client.ReceivePacket();
        client.SendStatus (1, 2, 30, 0, AppleLinkHost::kErrorNotFound);

        Assert::AreEqual (S_OK, reading.get());
        Assert::AreEqual<int> (AppleLinkHost::kErrorNotFound, status.error);
    }




    TEST_METHOD (WriteSectors_SendsEachAcknowledgedSector_AgainAfterANak)
    {
        Client             client;
        std::vector<Byte>  data    = Pattern (512, 40);
        std::vector<Byte>  got (256);
        AppleLinkStatus  status;
        AppleLinkPacket  packet;
        Byte             track     = 0;
        Byte             sector    = 0;
        const Byte       sectors[] = { 2, 7 };
        auto               writing = std::async (std::launch::async, [&] () { return client.host->WriteSectors (1, 3, sectors, data, status); });

        client.SendReady (LinkProgram::kProgramVersion);

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kWriteSectors, packet.command);
        Assert::IsTrue (packet.payload == std::vector<Byte> { 1, 3, 2, 2, 7, 0, 100 });
        Assert::IsTrue (client.IsQuiet(), L"no data before the client's Acknowledge");

        client.SendAcknowledge (AppleLinkCodec::kAck, 2, 3);
        packet = client.ReceivePacket();
        Assert::AreEqual (S_OK, AppleLinkCodec::ParseSectorData (packet, track, sector, got));
        Assert::AreEqual<int> (2, sector);
        Assert::IsTrue (std::equal (got.begin(), got.end(), data.begin()));

        client.SendAcknowledge (AppleLinkCodec::kNak, 2, 3);
        packet = client.ReceivePacket();
        Assert::AreEqual (S_OK, AppleLinkCodec::ParseSectorData (packet, track, sector, got));
        Assert::AreEqual<int> (2, sector);

        client.SendAcknowledge (AppleLinkCodec::kAck, 7, 3);
        packet = client.ReceivePacket();
        Assert::AreEqual (S_OK, AppleLinkCodec::ParseSectorData (packet, track, sector, got));
        Assert::AreEqual<int> (7, sector);
        Assert::IsTrue (std::equal (got.begin(), got.end(), data.begin() + 256));

        client.SendStatus (2, 1, 3, 2, AppleLinkHost::kErrorNone);

        Assert::AreEqual (S_OK, writing.get());
        Assert::AreEqual<int> (2, status.done);
    }




    TEST_METHOD (WriteSectors_WriteProtected_ReturnsTheStatusError)
    {
        Client             client;
        std::vector<Byte>  data      = Pattern (256, 0);
        AppleLinkStatus    status;
        const Byte         sectors[] = { 0 };
        auto               writing = std::async (std::launch::async, [&] () { return client.host->WriteSectors (1, 0, sectors, data, status); });

        client.SendReady (LinkProgram::kProgramVersion);
        client.ReceivePacket();
        client.SendStatus (2, 1, 0, 0, AppleLinkHost::kErrorWriteProtected);

        Assert::AreEqual (S_OK, writing.get());
        Assert::AreEqual<int> (AppleLinkHost::kErrorWriteProtected, status.error);
        Assert::IsTrue (client.IsQuiet(), L"and no data sent");
    }




    TEST_METHOD (FormatTrack_SendsDriveTrackAndVolume)
    {
        Client           client;
        AppleLinkStatus  status;
        AppleLinkPacket  packet;
        auto             formatting = std::async (std::launch::async, [&] () { return client.host->FormatTrack (2, 34, 254, status); });

        client.SendReady (LinkProgram::kProgramVersion);

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kFormatTrack, packet.command);
        Assert::IsTrue (packet.payload == std::vector<Byte> { 2, 34, 254, 0, 100 });

        client.SendStatus (3, 2, 34, 1, AppleLinkHost::kErrorNone);

        Assert::AreEqual (S_OK, formatting.get());
        Assert::AreEqual<int> (34, status.track);
    }




    TEST_METHOD (SetSpeed_ReturnsAfterTheAcknowledge)
    {
        Client           client;
        AppleLinkPacket  packet;
        auto             setting = std::async (std::launch::async, [&] () { return client.host->SetSpeed (0x10); });

        client.SendReady (LinkProgram::kProgramVersion);

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kSetSpeed, packet.command);
        Assert::IsTrue (packet.payload == std::vector<Byte> { 0x10 });

        client.SendAcknowledge (AppleLinkCodec::kAck, 0, 0);

        Assert::AreEqual (S_OK, setting.get());
    }




    TEST_METHOD (Cancel_SendsCancel)
    {
        Client           client;
        AppleLinkPacket  packet;

        Assert::AreEqual (S_OK, client.host->Cancel());

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kCancel, packet.command);
        Assert::IsTrue (packet.payload.empty());
    }




    TEST_METHOD (Cancel_EndsTheOperationOnTheClientsReady_UsedByTheNextOne)
    {
        Client             client;
        std::vector<Byte>  data;
        AppleLinkStatus    status;
        AppleLinkPacket    packet;
        const Byte         sectors[] = { 0, 1, 2 };
        auto               reading = std::async (std::launch::async, [&] () { return client.host->ReadSectors (1, 4, sectors, data, status); });

        client.SendReady (LinkProgram::kProgramVersion);
        client.ReceivePacket();
        client.SendSector (4, 0, Pattern (256, 2));

        Assert::AreEqual (S_OK, client.host->Cancel());
        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kCancel, packet.command);

        client.SendReady (LinkProgram::kProgramVersion);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), reading.get());

        auto  formatting = std::async (std::launch::async, [&] () { return client.host->FormatTrack (1, 4, 254, status); });

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kFormatTrack, packet.command, L"sent in reply to the Ready that ended the read");

        client.SendStatus (3, 1, 4, 1, AppleLinkHost::kErrorNone);
        Assert::AreEqual (S_OK, formatting.get());
    }




    TEST_METHOD (AnOperation_LoadsTheProgramFirst_ForTheShim)
    {
        Client             client;
        std::vector<Byte>  data;
        AppleLinkStatus    status;
        AppleLinkPacket    packet;
        size_t             pages     = (Program().size() + 255) / 256;
        const Byte         sectors[] = { 0 };
        auto               reading = std::async (std::launch::async, [&] () { return client.host->ReadSectors (1, 0, sectors, data, status); });

        client.SendReady (0);

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kLoadProgram, packet.command);

        for (size_t i = 0; i < pages; i++)
        {
            client.ReceivePacket();
            client.Send (std::vector<Byte> { AppleLinkCodec::kAck });
        }

        client.SendReady (LinkProgram::kProgramVersion);

        packet = client.ReceivePacket();
        Assert::AreEqual<int> (AppleLinkCodec::kReadSectors, packet.command);

        client.SendSector (0, 0, Pattern (256, 8));
        client.SendStatus (1, 1, 0, 1, AppleLinkHost::kErrorNone);

        Assert::AreEqual (S_OK, reading.get());
    }
};
