#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/TrackSectorDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorDecoderTests
//
//  Sector decodes over tracks of whole nibbles built in memory. Nibbles are
//  eight bits with the high bit set, so a track's bytes are its bits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackSectorDecoderTests)
{
public:

    static constexpr Byte  kVolume   = 254;
    static constexpr Byte  kSync     = 0xFF;
    static constexpr int   kSyncRun  = 5;
    static constexpr int   kSectors  = NibbleFormat::kSectorsPerTrack;



    struct Track
    {
        vector<Byte>               bytes;
        TrackBitReader::TrackBits  src;
    };



    static void AppendSync (vector<Byte> & out)
    {
        out.insert (out.end(), kSyncRun, kSync);
    }



    static void Append44 (vector<Byte> & out, Byte value)
    {
        Byte  odd  = 0;
        Byte  even = 0;



        DiskFieldFormat::Encode44 (value, odd, even);
        out.push_back (odd);
        out.push_back (even);
    }



    static void AppendAddressField (vector<Byte> & out, Byte track, Byte sector, Byte checksumDelta)
    {
        AppendSync (out);
        out.insert (out.end(), { NibbleFormat::kProlog0, NibbleFormat::kProlog1, NibbleFormat::kAddressProlog2 });
        Append44 (out, kVolume);
        Append44 (out, track);
        Append44 (out, sector);
        Append44 (out, static_cast<Byte> ((kVolume ^ track ^ sector) ^ checksumDelta));
        out.insert (out.end(), { 0xDE, 0xAA, 0xEB });
    }



    static void AppendDataField (vector<Byte> & out, const vector<Byte> & sector, bool spoilChecksum)
    {
        vector<Byte>  body;
        Byte          checksum = 0;



        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, sector, body, checksum);

        if (spoilChecksum)
        {
            checksum = DiskFieldFormat::TranslateValue (DiskFieldKind::Sixteen, static_cast<Byte> (DiskFieldFormat::TranslateNibble (DiskFieldKind::Sixteen, checksum) + 1));
        }

        AppendSync (out);
        out.insert (out.end(), { NibbleFormat::kProlog0, NibbleFormat::kProlog1, NibbleFormat::kDataProlog2 });
        out.insert (out.end(), body.begin(), body.end());
        out.push_back (checksum);
        out.insert (out.end(), { 0xDE, 0xAA, 0xEB });
    }



    static vector<Byte> MakeSector (Byte seed)
    {
        vector<Byte>  sector (NibbleFormat::kSectorByteSize, 0);
        size_t        i      = 0;



        for (i = 0; i < sector.size(); i++)
        {
            sector[i] = static_cast<Byte> (seed + i * 7);
        }

        return sector;
    }



    static void Finish (Track & track)
    {
        AppendSync (track.bytes);

        track.src.bits     = &track.bytes;
        track.src.bitCount = track.bytes.size() * 8;
    }



    TEST_METHOD (AGoodSectorIsVerified)
    {
        Track          track;
        vector<Byte>   sector                              = MakeSector (3);
        Byte           data[NibbleFormat::kSectorByteSize] = {};
        Byte           number                              = 0;
        size_t         bitPos                              = 0;
        size_t         field                               = 0;
        size_t         dataAt                              = 0;
        SectorOutcome  outcome                             = SectorOutcome::Lost;
        HRESULT        hr                                  = S_OK;



        AppendAddressField (track.bytes, 5, 9, 0);
        AppendDataField    (track.bytes, sector, false);
        Finish (track);

        hr = TrackSectorDecoder::DecodeOneSector (track.src, bitPos, number, data, outcome, field, dataAt);

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (outcome == SectorOutcome::Verified);
        Assert::AreEqual ((int) 9, (int) number);
        Assert::AreEqual ((size_t) kSyncRun * 8, field, L"address field after the sync");
        Assert::IsTrue   (dataAt > field && dataAt != SIZE_MAX);
        Assert::IsTrue   (std::equal (sector.begin(), sector.end(), data));
    }



    TEST_METHOD (ABadDataChecksumIsRecovered)
    {
        Track          track;
        vector<Byte>   sector                              = MakeSector (11);
        Byte           data[NibbleFormat::kSectorByteSize] = {};
        Byte           number                              = 0;
        size_t         bitPos                              = 0;
        size_t         field                               = 0;
        size_t         dataAt                              = 0;
        SectorOutcome  outcome                             = SectorOutcome::Lost;



        AppendAddressField (track.bytes, 0, 2, 0);
        AppendDataField    (track.bytes, sector, true);
        Finish (track);

        Assert::AreEqual (S_OK, TrackSectorDecoder::DecodeOneSector (track.src, bitPos, number, data, outcome, field, dataAt));
        Assert::IsTrue   (outcome == SectorOutcome::Recovered);
        Assert::IsTrue   (std::equal (sector.begin(), sector.end(), data), L"the bytes decode anyway");
    }



    TEST_METHOD (ABadAddressChecksumIsLost)
    {
        Track          track;
        Byte           data[NibbleFormat::kSectorByteSize] = {};
        Byte           number                              = 0;
        size_t         bitPos                              = 0;
        size_t         field                               = 0;
        size_t         dataAt                              = 0;
        SectorOutcome  outcome                             = SectorOutcome::Verified;



        AppendAddressField (track.bytes, 0, 2, 1);
        AppendDataField    (track.bytes, MakeSector (0), false);
        Finish (track);

        Assert::AreEqual (S_OK, TrackSectorDecoder::DecodeOneSector (track.src, bitPos, number, data, outcome, field, dataAt));
        Assert::IsTrue   (outcome == SectorOutcome::Lost);
        Assert::AreEqual (SIZE_MAX, dataAt);
    }



    //  A sector with no data field before the next address field is lost, and
    //  the next decode starts on that address field.
    TEST_METHOD (AMissingDataFieldDoesNotTakeTheNextSectors)
    {
        Track          track;
        vector<Byte>   second                              = MakeSector (40);
        Byte           data[NibbleFormat::kSectorByteSize] = {};
        Byte           number                              = 0;
        size_t         bitPos                              = 0;
        size_t         field                               = 0;
        size_t         dataAt                              = 0;
        SectorOutcome  outcome                             = SectorOutcome::Verified;



        AppendAddressField (track.bytes, 0, 1, 0);
        AppendAddressField (track.bytes, 0, 2, 0);
        AppendDataField    (track.bytes, second, false);
        Finish (track);

        Assert::AreEqual (S_OK, TrackSectorDecoder::DecodeOneSector (track.src, bitPos, number, data, outcome, field, dataAt));
        Assert::AreEqual ((int) 1, (int) number);
        Assert::IsTrue   (outcome == SectorOutcome::Lost);

        Assert::AreEqual (S_OK, TrackSectorDecoder::DecodeOneSector (track.src, bitPos, number, data, outcome, field, dataAt));
        Assert::AreEqual ((int) 2, (int) number);
        Assert::IsTrue   (outcome == SectorOutcome::Verified);
        Assert::IsTrue   (std::equal (second.begin(), second.end(), data));
    }



    TEST_METHOD (ATrackWithNoAddressFieldFails)
    {
        Track          track;
        Byte           data[NibbleFormat::kSectorByteSize] = {};
        Byte           number                              = 0;
        size_t         bitPos                              = 0;
        size_t         field                               = 0;
        size_t         dataAt                              = 0;
        SectorOutcome  outcome                             = SectorOutcome::Verified;
        HRESULT        hr                                  = S_OK;



        track.bytes.assign (64, kSync);
        Finish (track);

        hr = TrackSectorDecoder::DecodeOneSector (track.src, bitPos, number, data, outcome, field, dataAt);

        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (TrackSectorDecoder::HasDataField (track.src));
    }



    TEST_METHOD (HasDataFieldFindsADataPrologue)
    {
        Track  track;



        AppendDataField (track.bytes, MakeSector (1), false);
        Finish (track);

        Assert::IsTrue (TrackSectorDecoder::HasDataField (track.src));
    }



    TEST_METHOD (FindSectorBaseIsTheLowestNumberOfAnOffsetTrack)
    {
        constexpr Byte  kBase = 0x11;



        Track  offsetTrack;
        Track  standardTrack;
        int    s             = 0;



        for (s = 0; s < kSectors; s++)
        {
            AppendAddressField (offsetTrack.bytes,   17, static_cast<Byte> (kBase + s), 0);
            AppendDataField    (offsetTrack.bytes,   MakeSector (static_cast<Byte> (s)), false);
            AppendAddressField (standardTrack.bytes, 17, static_cast<Byte> (s), 0);
            AppendDataField    (standardTrack.bytes, MakeSector (static_cast<Byte> (s)), false);
        }

        Finish (offsetTrack);
        Finish (standardTrack);

        Assert::AreEqual ((int) kBase, (int) TrackSectorDecoder::FindSectorBase (offsetTrack.src));
        Assert::AreEqual ((int) 0,     (int) TrackSectorDecoder::FindSectorBase (standardTrack.src));
    }



    TEST_METHOD (PopCountCountsSetBits)
    {
        Assert::AreEqual (0,  TrackSectorDecoder::PopCount (0));
        Assert::AreEqual (1,  TrackSectorDecoder::PopCount (0x8000));
        Assert::AreEqual (16, TrackSectorDecoder::PopCount (0xFFFF));
        Assert::AreEqual (8,  TrackSectorDecoder::PopCount (0xAAAA));
    }
};
