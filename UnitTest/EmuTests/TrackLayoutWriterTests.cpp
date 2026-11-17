#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/TrackLayoutWriter.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriterTests
//
//  A track built to a measured layout: its bits and nibbles in a turn, where
//  sector 0 begins, and the volume, and a WOZ of such tracks whose sectors
//  read back as written.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackLayoutWriterTests)
{
public:

    using Image = std::vector<Byte>;

    //  A 140K image with different bytes in every sector.
    static Image  MakeSectors()
    {
        Image  sectors (NibblizationLayer::kImageByteSize);



        for (size_t at = 0; at < sectors.size(); at++)
        {
            sectors[at] = (Byte) (at * 7 + at / 256);
        }

        return sectors;
    }

    //  The nibbles in one turn of a track's bits, and where each begins.
    static std::vector<std::pair<Byte, size_t>>  ReadNibbles (const std::vector<uint8_t> & bits)
    {
        std::vector<std::pair<Byte, size_t>>  nibbles;
        size_t                                at    = 0;
        Byte                                  shift = 0;
        size_t                                start = 0;



        //  Two turns are read, so the nibble that wraps past the end is whole,
        //  and the nibbles that begin between the middles of the two are a turn's.
        while (at < bits.size() * 2)
        {
            if (shift == 0 && bits[at % bits.size()] == 0)
            {
                at++;
                continue;
            }

            start = (shift == 0) ? at : start;
            shift = (Byte) ((shift << 1) | bits[at % bits.size()]);
            at++;

            if ((shift & 0x80) != 0)
            {
                if (start >= bits.size() / 2 && start < bits.size() + bits.size() / 2)
                {
                    nibbles.push_back ({ shift, start % bits.size() });
                }

                shift = 0;
            }
        }

        return nibbles;
    }

    //  Wizardry I's boot disk on the owner's //c, track 10: 51,244 bits and
    //  6,338 nibbles in a turn, with sector 0 at bit 44,453 and volume 0.
    TEST_METHOD (Track_HasTheLayoutsBitsNibblesSector0AndVolume)
    {
        Image                                 sectors = MakeSectors();
        TrackLayout                           layout  = { 0, 6338, 51244, 44453 };
        std::vector<uint8_t>                  bits;
        std::vector<std::pair<Byte, size_t>>  nibbles;
        size_t                                sector0 = SIZE_MAX;
        Byte                                  volume  = 0xFF;



        AssertSucceeded (TrackLayoutWriter::BuildTrack (sectors, 10, layout, bits));

        nibbles = ReadNibbles (bits);

        for (size_t i = 0; i + 10 < nibbles.size() && sector0 == SIZE_MAX; i++)
        {
            bool  isAddress = nibbles[i].first == 0xD5 && nibbles[i + 1].first == 0xAA && nibbles[i + 2].first == 0x96;
            Byte  sector    = (Byte) (((nibbles[i + 7].first << 1) | 1) & nibbles[i + 8].first);

            if (isAddress && sector == 0)
            {
                sector0 = nibbles[i].second;
                volume  = (Byte) (((nibbles[i + 3].first << 1) | 1) & nibbles[i + 4].first);
            }
        }

        Assert::AreEqual ((size_t) 51244, bits.size(), L"Bits in a turn");
        Assert::AreEqual ((size_t) 6338, nibbles.size(), L"Nibbles in a turn");
        Assert::AreEqual ((size_t) 44453, sector0, L"Where sector 0's address field begins");
        Assert::AreEqual ((Byte) 0, volume);
    }

    //  Nibbles and bits too few for 16 sectors and the gaps' syncs.
    TEST_METHOD (Track_ALayoutTooShortForItsSectors_IsInvalidData)
    {
        std::vector<uint8_t>  bits;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), TrackLayoutWriter::BuildTrack (MakeSectors(), 0, TrackLayout { 254, 5904, 47424, 0 }, bits));
    }

    //  A WOZ of measured tracks, and of tracks with the volume alone, reads
    //  back the sectors it was built from.
    TEST_METHOD (Woz_ReadsBackItsSectors)
    {
        Image                     sectors = MakeSectors();
        std::vector<TrackLayout>  layouts (NibblizationLayer::kTrackCount);
        Image                     woz;
        Image                     back;
        SectorDecodeReport        report;



        for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            layouts[(size_t) track] = (track % 2 == 0) ? TrackLayout { 0, 6337, 51244, track * 1471LL } : TrackLayout { 0 };
        }

        AssertSucceeded (TrackLayoutWriter::BuildWoz (sectors, layouts, woz));
        AssertSucceeded (VolumeImage::Load (woz, "built.woz", back, report));

        Assert::IsTrue (back == sectors, L"Every sector reads back");
    }
};
