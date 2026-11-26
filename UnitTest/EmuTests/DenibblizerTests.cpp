#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Denibblizer.h"
#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/SectorInterleave.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DenibblizerTests
//
//  Decodes of bit images built in memory one field at a time, so a sector
//  can be left out, damaged or repeated.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DenibblizerTests)
{
public:

    static constexpr int     kTracks      = NibbleFormat::kTrackCount;
    static constexpr int     kSectors     = NibbleFormat::kSectorsPerTrack;
    static constexpr size_t  kSectorBytes = NibbleFormat::kSectorByteSize;
    static constexpr int     kDamageTrack = 7;
    static constexpr int     kDamageSlot  = 4;



    enum class Damage
    {
        None,
        Missing,
        BadChecksum,
        Repeated
    };



    static vector<Byte> MakeSectors()
    {
        vector<Byte>  sectors (NibbleFormat::kImageByteSize, 0);
        size_t        i       = 0;



        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> ((i * 13 + i / kSectorBytes) & 0xFF);
        }

        return sectors;
    }



    //  A data field whose checksum nibble is one value off.
    static void AppendBadDataField (vector<Byte> & bits, size_t & bitPos, const Byte * sector)
    {
        vector<Byte>  body;
        Byte          checksum = 0;
        int           i        = 0;



        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, std::span<const Byte> (sector, kSectorBytes), body, checksum);
        checksum = DiskFieldFormat::TranslateValue (DiskFieldKind::Sixteen, static_cast<Byte> (DiskFieldFormat::TranslateNibble (DiskFieldKind::Sixteen, checksum) + 1));

        for (i = 0; i < NibbleFieldWriter::kDataPrologueGap; i++)
        {
            NibbleFieldWriter::PackSyncNibbleBits (bits, bitPos);
        }

        for (Byte nibble : { NibbleFormat::kProlog0, NibbleFormat::kProlog1, NibbleFormat::kDataProlog2 })
        {
            NibbleFieldWriter::PackNibbleBits (bits, bitPos, nibble);
        }

        for (Byte nibble : body)
        {
            NibbleFieldWriter::PackNibbleBits (bits, bitPos, nibble);
        }

        for (Byte nibble : { checksum, (Byte) 0xDE, (Byte) 0xAA, (Byte) 0xEB })
        {
            NibbleFieldWriter::PackNibbleBits (bits, bitPos, nibble);
        }
    }



    //  The DOS-ordered sectors as a bit image, with physical sector
    //  kDamageSlot of kDamageTrack damaged. Each track has room for a repeated
    //  sector.
    static void BuildImage (const vector<Byte> & sectors, Damage damage, DiskImage & image)
    {
        int     track    = 0;
        int     physical = 0;
        size_t  bitPos   = 0;
        size_t  offset   = 0;
        bool    isTarget = false;



        for (track = 0; track < kTracks; track++)
        {
            image.ResizeTrack (track, 2 * NibbleFormat::kTrackBitCapacity);
            bitPos = 0;

            for (physical = 0; physical < kSectors; physical++)
            {
                vector<Byte> &  bits = image.GetTrackBitsForWrite (track);

                offset   = (static_cast<size_t> (track) * kSectors + SectorInterleave::kDsk_PhysicalToFile[physical]) * kSectorBytes;
                isTarget = track == kDamageTrack && physical == kDamageSlot;

                if (isTarget && damage == Damage::Missing)
                {
                    continue;
                }

                NibbleFieldWriter::AppendAddressField (bits, bitPos, NibbleFormat::kDefaultVolume, static_cast<Byte> (track), static_cast<Byte> (physical));

                if (isTarget && damage == Damage::BadChecksum)
                {
                    AppendBadDataField (bits, bitPos, &sectors[offset]);
                    continue;
                }

                NibbleFieldWriter::AppendDataField (bits, bitPos, &sectors[offset]);

                if (isTarget && damage == Damage::Repeated)
                {
                    NibbleFieldWriter::AppendAddressField (bits, bitPos, NibbleFormat::kDefaultVolume, static_cast<Byte> (track), static_cast<Byte> (physical));
                    NibbleFieldWriter::AppendDataField    (bits, bitPos, &sectors[offset]);
                }
            }

            image.SetTrackBitCount (track, bitPos);
        }
    }



    static size_t GetDamagedOffset()
    {
        return (static_cast<size_t> (kDamageTrack) * kSectors + SectorInterleave::kDsk_PhysicalToFile[kDamageSlot]) * kSectorBytes;
    }



    TEST_METHOD (AWholeImageDecodesToItsSectors)
    {
        vector<Byte>      sectors = MakeSectors();
        vector<Byte>      out;
        DiskImage         image;
        DenibblizeReport  report;
        HRESULT           hr      = S_OK;



        BuildImage (sectors, Damage::None, image);

        hr = Denibblizer::Denibblize (image, DiskFormat::Dsk, out, report);

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (out == sectors);
        Assert::AreEqual (kTracks, report.tracksComplete);
        Assert::AreEqual (kTracks * kSectors, report.sectorsVerified);
        Assert::AreEqual (0, report.tracksPartial);
    }



    TEST_METHOD (AMissingSectorFailsTheStrictDecodeAndNotTheSalvage)
    {
        vector<Byte>      sectors = MakeSectors();
        vector<Byte>      out;
        DiskImage         image;
        DenibblizeReport  report;
        HRESULT           hr      = S_OK;



        BuildImage (sectors, Damage::Missing, image);

        hr = Denibblizer::Denibblize (image, DiskFormat::Dsk, out, report);
        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (1, report.tracksPartial);
        Assert::AreEqual (1, report.sectorsMissing);

        hr = Denibblizer::Denibblize (image, DiskFormat::Dsk, out);
        Assert::IsTrue   (FAILED (hr), L"the reportless decode fails too");

        hr = Denibblizer::SalvageSectors (image, DiskFormat::Dsk, out, report);
        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (std::all_of (out.begin() + (ptrdiff_t) GetDamagedOffset(), out.begin() + (ptrdiff_t) (GetDamagedOffset() + kSectorBytes), [] (Byte b) { return b == 0; }));
    }



    TEST_METHOD (ABadChecksumIsKeptOnlyBySalvage)
    {
        vector<Byte>      sectors = MakeSectors();
        vector<Byte>      out;
        DiskImage         image;
        DenibblizeReport  report;
        HRESULT           hr      = S_OK;



        BuildImage (sectors, Damage::BadChecksum, image);

        hr = Denibblizer::Denibblize (image, DiskFormat::Dsk, out, report);
        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (1, report.sectorsRecovered);

        hr = Denibblizer::SalvageSectors (image, DiskFormat::Dsk, out, report);
        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (1, report.sectorsRecovered);
        Assert::IsTrue   (out == sectors, L"the recovered sector's bytes are written");
    }



    TEST_METHOD (CoverageReportsARepeatedSector)
    {
        vector<Byte>        sectors = MakeSectors();
        vector<Byte>        out;
        DiskImage           image;
        SectorDecodeReport  coverage;
        HRESULT             hr      = S_OK;



        BuildImage (sectors, Damage::Repeated, image);

        hr = Denibblizer::Denibblize (image, DiskFormat::Dsk, out, coverage);

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (coverage.HasDataLoss(), L"a repeated sector is damage");

        hr = Denibblizer::Denibblize (image, DiskFormat::Dsk, out);
        Assert::IsTrue   (FAILED (hr));
    }



    TEST_METHOD (AWozFormatIsNotASectorOrder)
    {
        vector<Byte>      out;
        DiskImage         image;
        DenibblizeReport  report;
        HRESULT           hr      = S_OK;



        hr = Denibblizer::Denibblize (image, DiskFormat::Woz, out, report);

        Assert::AreEqual (E_INVALIDARG, hr);
    }
};
