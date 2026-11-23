#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/FluxBitView.h"
#include "Machines/Apple2/Common/FluxSectorWriter.h"
#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "Machines/Apple2/Common/TrackBitReader.h"
#include "Machines/Apple2/Common/TrackSectorDecoder.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxSectorWriterTests
//
//  Sector writes to one flux track of an in-memory disk. The track is built
//  from a standard bit track at the nominal cell, and read back at the
//  controller's cell.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FluxSectorWriterTests)
{
public:

    static constexpr int     kTrack       = 0;
    static constexpr int     kSectors     = NibbleFormat::kSectorsPerTrack;
    static constexpr size_t  kSectorBytes = NibbleFormat::kSectorByteSize;



    static vector<Byte> MakeSectors()
    {
        vector<Byte>  sectors (NibbleFormat::kImageByteSize, 0);
        size_t        i       = 0;



        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> ((i * 7 + i / kSectorBytes) & 0xFF);
        }

        return sectors;
    }



    //  Track 0 of the sectors as flux, cut to keepBits.
    static void SetStandardFluxTrack (const vector<Byte> & sectors, size_t keepBits, DiskImage & disk)
    {
        vector<Byte>  bits     (NibbleFormat::kTrackBitCapacity / 8, 0);
        size_t        bitCount = 0;
        size_t        offset   = 0;
        int           physical = 0;



        for (physical = 0; physical < kSectors; physical++)
        {
            offset = static_cast<size_t> (SectorInterleave::kDsk_PhysicalToFile[physical]) * kSectorBytes;

            NibbleFieldWriter::AppendAddressField (bits, bitCount, NibbleFormat::kDefaultVolume, kTrack, static_cast<Byte> (physical));
            NibbleFieldWriter::AppendDataField    (bits, bitCount, &sectors[offset]);
        }

        disk.SetFluxTrack (kTrack, FluxTestImages::BitsToNominalFlux (bits, std::min (bitCount, keepBits)));
    }



    //  Each physical sector's bytes as decoded from track 0, in file order;
    //  a sector that doesn't verify is left as zeros.
    static vector<Byte> DecodeTrack (const DiskImage & disk, int & outVerified)
    {
        vector<Byte>               out    (static_cast<size_t> (kSectors) * kSectorBytes, 0);
        FluxBitView                view;
        TrackBitReader::TrackBits  src                                 = TrackBitReader::GetTrackBits (disk, kTrack, view);
        Byte                       data[NibbleFormat::kSectorByteSize] = {};
        Byte                       number                              = 0;
        size_t                     bitPos                              = 0;
        size_t                     field                               = 0;
        size_t                     dataAt                              = 0;
        SectorOutcome              outcome                             = SectorOutcome::Lost;
        uint16_t                   seen                                = 0;
        int                        i                                   = 0;
        HRESULT                    hr                                  = S_OK;



        for (i = 0; i < kSectors * 2; i++)
        {
            hr = TrackSectorDecoder::DecodeOneSector (src, bitPos, number, data, outcome, field, dataAt);

            if (FAILED (hr) || outcome != SectorOutcome::Verified || number >= kSectors)
            {
                continue;
            }

            seen = static_cast<uint16_t> (seen | (1 << number));
            memcpy (&out[static_cast<size_t> (SectorInterleave::kDsk_PhysicalToFile[number]) * kSectorBytes], data, kSectorBytes);
        }

        outVerified = TrackSectorDecoder::PopCount (seen);

        return out;
    }



    TEST_METHOD (AChangedSectorIsWrittenAndTheOthersAreKept)
    {
        vector<Byte>  sectors  = MakeSectors();
        vector<Byte>  edited   = sectors;
        vector<Byte>  decoded;
        DiskImage     disk;
        int           verified = 0;
        HRESULT       hr       = S_OK;



        SetStandardFluxTrack (sectors, SIZE_MAX, disk);
        memset (&edited[9 * kSectorBytes], 0xA5, kSectorBytes);

        hr = FluxSectorWriter::WriteFluxTrackSectors (edited, SectorInterleave::kDsk_PhysicalToFile, kTrack, disk);
        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (disk.GetTrackKind (kTrack) == TrackKind::Flux, L"the track stays flux");

        decoded = DecodeTrack (disk, verified);

        Assert::AreEqual (kSectors, verified);
        Assert::IsTrue   (std::equal (decoded.begin(), decoded.end(), edited.begin()));
    }



    TEST_METHOD (AnUnchangedTrackIsLeftAlone)
    {
        vector<Byte>  sectors = MakeSectors();
        vector<Byte>  before;
        DiskImage     disk;
        HRESULT       hr      = S_OK;



        SetStandardFluxTrack (sectors, SIZE_MAX, disk);
        before = disk.GetFluxTrack (kTrack).GetBytes();

        hr = FluxSectorWriter::WriteFluxTrackSectors (sectors, SectorInterleave::kDsk_PhysicalToFile, kTrack, disk);

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (disk.GetFluxTrack (kTrack).GetBytes() == before);
    }



    TEST_METHOD (AChangedSectorThatIsNotThereFailsAndKeepsTheTrack)
    {
        constexpr size_t  kKeepBits = 20000;



        vector<Byte>  sectors = MakeSectors();
        vector<Byte>  edited  = sectors;
        vector<Byte>  before;
        DiskImage     disk;
        HRESULT       hr      = S_OK;



        //  Only the first sectors' fields are on the track.
        SetStandardFluxTrack (sectors, kKeepBits, disk);
        before = disk.GetFluxTrack (kTrack).GetBytes();

        memset (&edited[static_cast<size_t> (SectorInterleave::kDsk_PhysicalToFile[14]) * kSectorBytes], 0x5A, kSectorBytes);

        hr = FluxSectorWriter::WriteFluxTrackSectors (edited, SectorInterleave::kDsk_PhysicalToFile, kTrack, disk);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND), hr);
        Assert::IsTrue   (disk.GetFluxTrack (kTrack).GetBytes() == before);
    }



    TEST_METHOD (ATrackWithNoSectorsGetsAStandardTrack)
    {
        constexpr size_t  kCells   = 60000;
        constexpr Byte    kOneCell = 32;



        vector<Byte>  sectors  = MakeSectors();
        vector<Byte>  decoded;
        DiskImage     disk;
        int           verified = 0;
        HRESULT       hr       = S_OK;



        //  A transition every cell: sync bytes and nothing else.
        disk.SetFluxTrack (kTrack, vector<Byte> (kCells, kOneCell));

        hr = FluxSectorWriter::WriteFluxTrackSectors (sectors, SectorInterleave::kDsk_PhysicalToFile, kTrack, disk);
        Assert::AreEqual (S_OK, hr);

        decoded = DecodeTrack (disk, verified);

        Assert::AreEqual (kSectors, verified);
        Assert::IsTrue   (std::equal (decoded.begin(), decoded.end(), sectors.begin()));
    }
};
