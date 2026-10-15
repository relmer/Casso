#include "Pch.h"

#include "Debugger/DiskLogicalMap.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogicalMapTests
//
//  The map against the nibblizers: a DOS-order image and a ProDOS-order
//  image, each with bytes that name their sector or block, nibblized, then
//  each physical sector decoded from the track's nibbles and compared with
//  the sector or block the map puts there.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiskLogicalMapTests)
    {
    public:

        static constexpr size_t  kSectorBytes = 256;
        static constexpr int     kTrackCount  = NibblizationLayer::kTrackCount;

        using TrackSectors = std::array<std::vector<Byte>, DiskLogicalMap::kSectorsPerTrack>;

        //  Every physical sector of a track, decoded from two revolutions of
        //  its nibbles: each address field's sector number, then its data
        //  field's bytes.
        static TrackSectors DecodeTrack (const DiskImage & image, int track)
        {
            constexpr int      kBody   = DiskFieldFormat::kBodyLength62;
            TrackSectors       sectors;
            std::vector<Byte>  nibbles;
            size_t             bitPos  = 0;
            int                sector  = -1;



            for (size_t i = 0; i < image.GetTrackBitCount (track) / 4; i++)
            {
                nibbles.push_back (NibblizationLayer::ReadNibbleAt (image, track, bitPos));
            }

            for (size_t i = 0; i + 3 + kBody < nibbles.size(); i++)
            {
                bool  isAddress = nibbles[i] == 0xD5 && nibbles[i + 1] == 0xAA && nibbles[i + 2] == 0x96;
                bool  isData    = nibbles[i] == 0xD5 && nibbles[i + 1] == 0xAA && nibbles[i + 2] == 0xAD && sector >= 0;



                if (isAddress)
                {
                    sector = DiskFieldFormat::Decode44 (nibbles[i + 7], nibbles[i + 8]);
                }

                if (isData)
                {
                    DataFieldDecode  decode;



                    DiskFieldFormat::DecodeData (DiskFieldKind::Sixteen, std::span<const Byte> (nibbles).subspan (i + 3, kBody), nibbles[i + 3 + kBody], decode);
                    Assert::IsTrue (decode.isChecksumGood);
                    sectors[sector].assign (decode.bytes.begin(), decode.bytes.end());
                    sector = -1;
                }
            }

            return sectors;
        }

        TEST_METHOD (EachDosSectorIsThePhysicalSectorTheMapGives)
        {
            std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
            DiskImage          image;



            for (size_t at = 0; at < raw.size(); at++)
            {
                raw[at] = static_cast<Byte> ((at / kSectorBytes) + at * 3);
            }

            Assert::AreEqual (S_OK, NibblizationLayer::NibblizeDsk (raw, image));

            for (int track : { 0, 17, 34 })
            {
                TrackSectors  sectors = DecodeTrack (image, track);



                for (int logical = 0; logical < DiskLogicalMap::kSectorsPerTrack; logical++)
                {
                    size_t  from = ((size_t) track * DiskLogicalMap::kSectorsPerTrack + logical) * kSectorBytes;



                    Assert::IsTrue (std::equal (raw.begin() + from, raw.begin() + from + kSectorBytes, sectors[DiskLogicalMap::GetDosPhysical (logical)].begin()),
                                    std::format (L"track {} logical sector {}", track, logical).c_str());
                }
            }
        }

        TEST_METHOD (EachBlockIsTheTwoPhysicalSectorsTheMapGives)
        {
            constexpr size_t   kBlockBytes = 2 * kSectorBytes;
            std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
            DiskImage          image;
            TrackSectors       sectors;
            int                decoded = -1;



            for (size_t at = 0; at < raw.size(); at++)
            {
                raw[at] = static_cast<Byte> ((at / kBlockBytes) * 7 + at);
            }

            Assert::AreEqual (S_OK, NibblizationLayer::NibblizePo (raw, image));

            for (Word block = 0; block < DiskLogicalMap::kBlockCount; block++)
            {
                int     track  = 0;
                int     first  = 0;
                int     second = 0;
                size_t  from   = (size_t) block * kBlockBytes;



                DiskLogicalMap::GetBlockPhysical (block, track, first, second);

                if (track != decoded)
                {
                    sectors = DecodeTrack (image, track);
                    decoded = track;
                }

                Assert::IsTrue (std::equal (raw.begin() + from, raw.begin() + from + kSectorBytes, sectors[first].begin()),
                                std::format (L"block {} first half", block).c_str());
                Assert::IsTrue (std::equal (raw.begin() + from + kSectorBytes, raw.begin() + from + kBlockBytes, sectors[second].begin()),
                                std::format (L"block {} second half", block).c_str());
            }
        }

        TEST_METHOD (APastSectorHasNoPhysicalSector)
        {
            Assert::AreEqual (-1, DiskLogicalMap::GetDosPhysical (16));
        }
    };
}
