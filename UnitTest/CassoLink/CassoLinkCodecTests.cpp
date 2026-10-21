#include "Pch.h"

#include "CassoLinkCaptures.h"
#include "CassoLink/CassoLinkCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodecTests
//
//  The codec against ADTPro's own traffic: every packet in the captures
//  decodes, and every image data packet re-encodes to the bytes ADTPro sent,
//  so either end of a link built on the codec reads and writes what ADTPro
//  does. The fault captures check that damage is reported, not decoded.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkCodecTests)
{
public:

    static constexpr HRESULT  kDamaged    = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    static constexpr HRESULT  kIncomplete = HRESULT_FROM_WIN32 (ERROR_MORE_DATA);

    //  Decode, with a packet the input ends inside reported as kIncomplete.
    static HRESULT  DecodeOne (std::span<const Byte> in, size_t & consumed, CassoLinkPacket & packet)
    {
        bool     complete = false;
        HRESULT  hr       = CassoLinkCodec::Decode (in, consumed, packet, complete);

        return (SUCCEEDED (hr) && !complete) ? kIncomplete : hr;
    }

    struct Found
    {
        size_t           at       = 0;
        size_t           consumed = 0;
        HRESULT          hr       = S_OK;
        CassoLinkPacket  packet;
    };

    //  Every framed packet in a stream. Bytes that do not start one -- the
    //  host's unframed one-byte and three-byte replies -- are stepped over.
    static std::vector<Found>  FindPackets (const std::vector<Byte> & stream)
    {
        std::vector<Found>  found;
        size_t              pos = 0;

        while (pos < stream.size())
        {
            Found  f;

            if (stream[pos] != CassoLinkCodec::kStart)
            {
                pos++;
                continue;
            }

            f.at = pos;
            f.hr = DecodeOne (std::span<const Byte> (stream).subspan (pos), f.consumed, f.packet);

            pos += std::max<size_t> (f.consumed, 1);
            found.push_back (std::move (f));
        }

        return found;
    }

    static Word  StartBlock (const CassoLinkPacket & packet)
    {
        return (Word) (packet.payload[0] | (packet.payload[1] << 8));
    }

    static std::vector<Byte>  Bytes (std::initializer_list<int> values)
    {
        std::vector<Byte>  out;

        for (int v : values) { out.push_back ((Byte) v); }
        return out;
    }


    //  The Apple's side of a send is nothing but framed packets: the size
    //  query, the put, 140 two-block image packets in order, and the summary.
    TEST_METHOD (SentDiskDecodesPacketByPacket)
    {
        std::vector<Byte>   stream = CassoLinkCaptures::LoadDirection ("adtpro-send-disk.txt", '>');
        std::vector<Found>  found  = FindPackets (stream);
        size_t              images = 0;
        size_t              total  = 0;

        for (const Found & f : found)
        {
            Assert::AreEqual (S_OK, f.hr);
            total += f.consumed;

            if (f.packet.command == CassoLinkCodec::kImageData)
            {
                Assert::AreEqual<Word> ((Word) (images * 2), StartBlock (f.packet));
                Assert::AreEqual<size_t> (2 + 1024, f.packet.payload.size());
                images++;
            }
        }

        Assert::AreEqual<size_t> (140, images);
        Assert::AreEqual (stream.size(), total, L"every byte is part of a packet");
        Assert::AreEqual (CassoLinkCodec::kSize, found.front().packet.command);
        Assert::AreEqual (CassoLinkCodec::kSummary, found.back().packet.command);
    }


    //  The host's side of a receive: unframed replies and 140 image packets,
    //  each checked by its CRC.
    TEST_METHOD (ReceivedDiskDecodesWithEveryCrcGood)
    {
        std::vector<Found>  found  = FindPackets (CassoLinkCaptures::LoadDirection ("adtpro-receive-disk.txt", '<'));
        size_t              images = 0;

        for (const Found & f : found)
        {
            Assert::AreEqual (S_OK, f.hr);

            if (f.packet.command == CassoLinkCodec::kImageData)
            {
                Assert::AreEqual<Word> ((Word) (images * 2), StartBlock (f.packet));
                images++;
            }
        }

        Assert::AreEqual<size_t> (140, images);
    }


    //  This encoder's output matches ADTPro's byte for byte on every captured
    //  packet, in both directions.
    TEST_METHOD (ImageDataReEncodesToAdtProsBytes)
    {
        for (const char * fixture : { "adtpro-send-disk.txt", "adtpro-receive-disk.txt" })
        {
            for (char direction : { '>', '<' })
            {
                std::vector<Byte>  stream = CassoLinkCaptures::LoadDirection (fixture, direction);

                for (const Found & f : FindPackets (stream))
                {
                    std::vector<Byte>  encoded;

                    if (f.hr != S_OK || f.packet.command != CassoLinkCodec::kImageData)
                    {
                        continue;
                    }

                    AssertSucceeded (CassoLinkCodec::EncodeImageData (StartBlock (f.packet),
                                                                      std::span<const Byte> (f.packet.payload).subspan (2), encoded));
                    Assert::IsTrue (std::equal (encoded.begin(), encoded.end(), stream.begin() + (ptrdiff_t) f.at, stream.begin() + (ptrdiff_t) (f.at + f.consumed)),
                                    L"the re-encoded packet is ADTPro's");
                    Assert::AreEqual (f.consumed, encoded.size());
                }
            }
        }
    }


    //  A directory comes back as one image data packet of screen text.
    TEST_METHOD (DirectoryDecodes)
    {
        std::vector<Found>  apple = FindPackets (CassoLinkCaptures::LoadDirection ("adtpro-directory.txt", '>'));
        std::vector<Found>  host  = FindPackets (CassoLinkCaptures::LoadDirection ("adtpro-directory.txt", '<'));

        Assert::AreEqual (CassoLinkCodec::kDirectory, apple.front().packet.command);
        Assert::AreEqual (S_OK, host.front().hr);
        Assert::AreEqual (CassoLinkCodec::kImageData, host.front().packet.command);
        Assert::AreEqual<size_t> (2 + 1024, host.front().packet.payload.size());
    }


    //  With the server's packet for block 4 damaged every time, each copy
    //  fails its check, the client NAKs block 4 four times, and the server's
    //  Home has $13 where the EOR is $19, which is reported as damage.
    TEST_METHOD (DamagedPacketsInTheNakCaptureAreReported)
    {
        std::vector<Found>  host     = FindPackets (CassoLinkCaptures::LoadDirection ("adtpro-receive-nak.txt", '<'));
        std::vector<Found>  apple    = FindPackets (CassoLinkCaptures::LoadDirection ("adtpro-receive-nak.txt", '>'));
        size_t              damaged  = 0;
        size_t              naks     = 0;

        for (const Found & f : host)
        {
            damaged += f.hr == kDamaged;
        }

        for (const Found & f : apple)
        {
            Assert::AreEqual (S_OK, f.hr);
            naks += f.packet.command == CassoLinkCodec::kAcknowledge && f.packet.payload == Bytes ({ 0x15, 0x04, 0x00 });
        }

        Assert::AreEqual<size_t> (4, damaged, L"three damaged copies of block 4 and the server's Home");
        Assert::AreEqual<size_t> (4, naks);
    }


    TEST_METHOD (FramedPacketsRoundTrip)
    {
        const std::vector<Byte>  payloads[] = { {}, Bytes ({ 0x01 }), Bytes ({ 0xC3, 0xC1, 0x00 }), std::vector<Byte> (300, 0xA5) };

        for (const std::vector<Byte> & payload : payloads)
        {
            std::vector<Byte>  wire;
            CassoLinkPacket    packet;
            size_t             consumed = 0;

            AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kReady, payload, wire));
            Assert::AreEqual<size_t> (5 + payload.size() + (payload.empty() ? 0 : 1), wire.size());
            Assert::AreEqual (S_OK, DecodeOne (wire, consumed, packet));
            Assert::AreEqual (wire.size(), consumed);
            Assert::AreEqual (CassoLinkCodec::kReady, packet.command);
            Assert::IsTrue (packet.payload == payload);
        }
    }


    TEST_METHOD (HomeHasTheDocumentedEnvelope)
    {
        std::vector<Byte>  wire;

        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kHome, {}, wire));

        Assert::IsTrue (wire == Bytes ({ 0xC1, 0x00, 0x00, 0xD8, 0x19 }));
    }


    TEST_METHOD (SectorDataRoundTripsAndChecksItsCrc)
    {
        std::vector<Byte>  sector (256);
        std::vector<Byte>  wire;
        std::vector<Byte>  back (256);
        CassoLinkPacket    packet;
        size_t             consumed = 0;
        Byte               track    = 0;
        Byte               number   = 0;

        for (size_t i = 0; i < sector.size(); i++) { sector[i] = (Byte) (i * 7); }

        AssertSucceeded (CassoLinkCodec::EncodeSectorData (17, 15, sector, wire));
        Assert::AreEqual (S_OK, DecodeOne (wire, consumed, packet));
        AssertSucceeded (CassoLinkCodec::ParseSectorData (packet, track, number, back));

        Assert::AreEqual<Byte> (17, track);
        Assert::AreEqual<Byte> (15, number);
        Assert::IsTrue (back == sector);

        packet.payload[100] ^= 0x01;
        Assert::AreEqual (kDamaged, CassoLinkCodec::ParseSectorData (packet, track, number, back), L"a changed byte fails the CRC");
    }


    //  Every byte value in every place, through the data field's nibbles and
    //  back, as Write sectors sends a sector.
    TEST_METHOD (SectorNibblesRoundTripAndCheckTheirCrc)
    {
        std::vector<Byte>  sector (256);
        std::vector<Byte>  wire;
        std::vector<Byte>  back (256);
        CassoLinkPacket    packet;
        size_t             consumed = 0;
        Byte               track    = 0;
        Byte               number   = 0;

        for (int seed : { 0, 1, 85, 170 })
        {
            for (size_t i = 0; i < sector.size(); i++) { sector[i] = (Byte) (i + seed); }

            wire.clear();
            AssertSucceeded (CassoLinkCodec::EncodeSectorNibbles (20, 9, sector, wire));
            Assert::AreEqual (S_OK, DecodeOne (wire, consumed, packet));
            Assert::AreEqual<size_t> (343 + 4, packet.payload.size(), L"track, sector, 343 nibbles, CRC");
            AssertSucceeded (CassoLinkCodec::ParseSectorNibbles (packet, track, number, back));

            Assert::AreEqual<Byte> (20, track);
            Assert::AreEqual<Byte> (9, number);
            Assert::IsTrue (back == sector);
        }

        packet.payload[200] ^= 0x01;
        Assert::AreEqual (kDamaged, CassoLinkCodec::ParseSectorNibbles (packet, track, number, back), L"a changed nibble fails the CRC");
    }


    TEST_METHOD (AcknowledgeHasKindAndBlock)
    {
        std::vector<Byte>  wire;

        AssertSucceeded (CassoLinkCodec::EncodeAcknowledge (CassoLinkCodec::kAck, 4, wire));

        //  As the //c sent it in the receive capture.
        Assert::IsTrue (wire == Bytes ({ 0xC1, 0x03, 0x00, 0xCB, 0x09, 0x06, 0x04, 0x00, 0x02 }));
    }


    TEST_METHOD (CrcIsXmodem)
    {
        const std::string  check = "123456789";

        Assert::AreEqual<Word> (0x31C3, CassoLinkCodec::ComputeCrc (std::span<const Byte> ((const Byte *) check.data(), check.size())));
    }


    TEST_METHOD (PagesEncodeRepeatsAsRunsThatEndAtThePage)
    {
        std::vector<Byte>  zeros (1024, 0);
        std::vector<Byte>  directory (1024, 0);
        std::vector<Byte>  out;

        CassoLinkCodec::EncodePages (zeros, out);
        Assert::IsTrue (out == Bytes ({ 0, 0, 0, 0, 0, 0, 0, 0 }), L"one run per page");

        //  ProDOS volume directory blocks 4 and 5, as ADTPro encoded them.
        directory[0]   = 3;
        directory[2]   = 5;
        directory[512] = 4;
        out.clear();
        CassoLinkCodec::EncodePages (directory, out);
        Assert::IsTrue (out == Bytes ({ 0x03, 0xFD, 0x05, 0xFB, 0x00, 0x00, 0x00, 0x00, 0x04, 0xFC, 0x00, 0x00, 0x00, 0x00 }));
    }


    TEST_METHOD (PagesRoundTripForAnyData)
    {
        std::vector<Byte>  data (1000);
        std::vector<Byte>  encoded;
        std::vector<Byte>  decoded;
        size_t             consumed = 0;
        bool               complete = false;
        uint32_t           seed     = 12345;

        for (size_t i = 0; i < data.size(); i++)
        {
            seed    = seed * 1103515245 + 12345;
            data[i] = (seed >> 24) < 96 ? (Byte) (seed >> 16) : (i == 0 ? 0 : data[i - 1]);
        }

        CassoLinkCodec::EncodePages (data, encoded);
        Assert::AreEqual (S_OK, CassoLinkCodec::DecodePages (encoded, data.size(), consumed, decoded, complete));
        Assert::IsTrue (complete);
        Assert::AreEqual (encoded.size(), consumed);
        Assert::IsTrue (decoded == data);
    }


    TEST_METHOD (DamageIsReportedWithBytesToSkip)
    {
        std::vector<Byte>  wire;
        std::vector<Byte>  bad;
        CassoLinkPacket    packet;
        size_t             consumed = 0;
        std::vector<Byte>  data (512, 0x11);

        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kSize, Bytes ({ 0xC1, 0x00 }), wire));

        bad = wire; bad[0] = 0x41;
        Assert::AreEqual (kDamaged, DecodeOne (bad, consumed, packet), L"no start byte");
        Assert::AreEqual<size_t> (1, consumed);

        bad = wire; bad[4] ^= 0xFF;
        Assert::AreEqual (kDamaged, DecodeOne (bad, consumed, packet), L"a bad envelope check");
        Assert::AreEqual<size_t> (1, consumed);

        bad = wire; bad.back() ^= 0xFF;
        Assert::AreEqual (kDamaged, DecodeOne (bad, consumed, packet), L"a bad payload check");
        Assert::AreEqual (wire.size(), consumed, L"the whole packet is skipped");

        wire.clear();
        AssertSucceeded (CassoLinkCodec::EncodeImageData (0, data, wire));
        bad = wire; bad[bad.size() - 2] ^= 0x01;
        Assert::AreEqual (kDamaged, DecodeOne (bad, consumed, packet), L"a bad CRC");
        Assert::AreEqual (wire.size(), consumed);

        //  A run back to an offset already passed.
        bad = Bytes ({ 0xC1, 0x00, 0x01, 0xD3, 0xC1 ^ 0x01 ^ 0xD3, 0x00, 0x00, 0x05, 0x00, 0x01 });
        Assert::AreEqual (kDamaged, DecodeOne (bad, consumed, packet));
        Assert::AreEqual<size_t> (1, consumed);
    }


    TEST_METHOD (ShortInputAsksForMore)
    {
        std::vector<Byte>  wire;
        CassoLinkPacket    packet;
        size_t             consumed = 99;
        std::vector<Byte>  data (512, 0x22);

        AssertSucceeded (CassoLinkCodec::EncodeImageData (6, data, wire));

        for (size_t cut = 0; cut < wire.size(); cut++)
        {
            Assert::AreEqual (kIncomplete, DecodeOne (std::span<const Byte> (wire).first (cut), consumed, packet));
            Assert::AreEqual<size_t> (0, consumed);
        }
    }
};
