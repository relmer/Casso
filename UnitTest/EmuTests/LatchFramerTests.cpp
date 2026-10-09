#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/LatchFramer.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LatchFramerTests
//
//  The disk inspector frames each track as the drive's read latch does. The
//  headline check (SC-002) runs Casso's own drive over the same track and
//  compares the nibbles it delivers on its second turn with the framer's.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LatchFramerTests)
{
public:

    static void FrameBits (const InspectorTrackBuilder & builder, FramedTrack & track)
    {
        LatchFramer::Frame (*builder.MakeBitCopy(), track);
    }



    static vector<Byte> GetValues (const FramedTrack & track)
    {
        vector<Byte>  values;



        for (const FramedNibble & n : track.nibbles)
        {
            values.push_back (n.value);
        }

        return values;
    }



    //  The nibbles Casso's drive delivers during its second turn of the track.
    static vector<Byte> ReadWithEngine (const TrackCopy & copy, size_t cells)
    {
        DiskImage          disk;
        Disk2NibbleEngine  engine;
        vector<Byte>       nibbles;
        uint8_t            nib    = 0;
        uint64_t           cycles = 0;
        uint64_t           c      = 0;



        disk.EnsureTrackSlots (1);
        disk.ClearQuarterTrackMap();

        if (copy.kind == TrackKind::Flux)
        {
            disk.SetFluxTrack (0, copy.fluxBytes);
        }
        else
        {
            disk.ResizeTrack (0, copy.bitCount);
            memcpy (disk.GetTrackBitsForWrite (0).data(), copy.bits.data(), min (copy.bits.size(), disk.GetTrackBitsForWrite (0).size()));
        }

        disk.SetQuarterTrackSlot (0, 0);

        engine.SetDiskImage    (&disk);
        engine.SetCurrentTrack (0);
        engine.SetMotorOn      (true);

        cycles = static_cast<uint64_t> (cells) * Disk2NibbleEngine::kCyclesPerBit;

        for (c = 0; c < 2 * cycles; c++)
        {
            engine.Tick (1);

            if (engine.ConsumeFreshNibble (nib) && c >= cycles)
            {
                nibbles.push_back (nib);
            }
        }

        return nibbles;
    }



    //  True when every nibble the drive delivered, but the first and last
    //  (which can straddle the turn's edges), appears in the same order as a
    //  run of the framer's nibbles taken round the track.
    static bool IsCyclicRun (const vector<Byte> & framed, const vector<Byte> & delivered)
    {
        vector<Byte>  doubled = framed;
        vector<Byte>  inner;
        bool          isRun   = delivered.size() >= 3;



        doubled.insert (doubled.end(), framed.begin(), framed.end());

        if (isRun)
        {
            inner.assign (delivered.begin() + 1, delivered.end() - 1);
            isRun = std::search (doubled.begin(), doubled.end(), inner.begin(), inner.end()) != doubled.end();
        }

        return isRun;
    }



    TEST_METHOD (AStandardTrackFramesAsWritten)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            track;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        FrameBits (builder, track);

        Assert::IsTrue (GetValues (track) == builder.GetNibbles());
        Assert::AreEqual (0u, static_cast<unsigned> (track.randomRegions.size()));
        Assert::AreEqual (static_cast<int> (InspectorTrackBuilder::kSyncWidth - 8), static_cast<int> (track.nibbles[0].extraZeroCells), L"10-cell sync has two extra zeros");
        Assert::AreEqual (0u, track.nibbles[0].startCell);
    }



    TEST_METHOD (ExtraZeroCellsAreCounted)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            track;



        builder.AppendSync   (20);
        builder.AppendNibble (0xD5, 1);
        builder.AppendNibble (0xAA, 2);
        builder.AppendNibble (0x96, 0);
        builder.AppendSync   (20);
        FrameBits (builder, track);

        Assert::AreEqual (static_cast<int> (0xD5), static_cast<int> (track.nibbles[20].value));
        Assert::AreEqual (1, static_cast<int> (track.nibbles[20].extraZeroCells));
        Assert::AreEqual (2, static_cast<int> (track.nibbles[21].extraZeroCells));
        Assert::AreEqual (0, static_cast<int> (track.nibbles[22].extraZeroCells));
    }



    TEST_METHOD (NoiseNibblesAreMarked)
    {
        Assert::IsTrue  (LatchFramer::IsNoiseNibble (0x88));
        Assert::IsTrue  (LatchFramer::IsNoiseNibble (0x81));
        Assert::IsFalse (LatchFramer::IsNoiseNibble (0xD5));
        Assert::IsFalse (LatchFramer::IsNoiseNibble (0x96));
        Assert::IsFalse (LatchFramer::IsNoiseNibble (0xFF));
    }



    TEST_METHOD (ALongZeroRunIsOneRandomRegion)
    {
        //  Two zeros trail the sync nibble before the run, so the run is 12
        //  zero cells long. Cell k is random when cells k-2..k+1 are zero,
        //  which takes cells 2 to 10 of the run: nine cells.
        InspectorTrackBuilder  builder;
        FramedTrack            track;
        size_t                 runStart = 0;



        builder.AppendSync (30);
        runStart = builder.GetCellCount() - 2;
        builder.AppendZeros (10);
        builder.AppendSync (30);
        FrameBits (builder, track);

        Assert::AreEqual (1, static_cast<int> (track.randomRegions.size()));
        Assert::AreEqual (static_cast<uint32_t> (runStart + 2), track.randomRegions[0].startCell);
        Assert::AreEqual (9u, track.randomRegions[0].cellCount);
        Assert::AreEqual (60, static_cast<int> (track.nibbles.size()), L"framing resumes at the next 1");
    }



    TEST_METHOD (ANibbleSpanningTheIndexIsReadWhole)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            plain;
        FramedTrack            moved;
        size_t                 shift = 0;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 3, InspectorTrackBuilder::FillPattern);
        FrameBits (builder, plain);

        //  Start the copy four cells into the 50th nibble, so that nibble is
        //  split across the end of the track.
        shift = plain.nibbles[49].startCell + 4;

        LatchFramer::Frame (*MakeRotated (builder, shift), moved);

        Assert::AreEqual (plain.nibbles.size(), moved.nibbles.size());
        Assert::AreEqual (static_cast<int> (plain.nibbles[49].value), static_cast<int> (moved.nibbles.front().value), L"the split nibble completes first after the index");
        Assert::IsTrue (moved.nibbles.front().startCell > moved.cellCount - 8, L"it starts before the index and ends after it");
    }


    static std::shared_ptr<const TrackCopy> MakeRotated (const InspectorTrackBuilder & builder, size_t shift)
    {
        auto                  copy  = std::make_shared<TrackCopy>();
        const vector<Byte> &  cells = builder.GetCells();
        size_t                n     = cells.size();
        size_t                i     = 0;



        copy->bitCount = n;
        copy->bits.assign ((n + 7) / 8, 0);

        for (i = 0; i < n; i++)
        {
            copy->bits[i >> 3] = static_cast<Byte> (copy->bits[i >> 3] | (cells[(i + shift) % n] << (7 - (i & 7))));
        }

        return copy;
    }



    TEST_METHOD (AFluxTrackFramesLikeItsBitTrack)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            bits;
        FramedTrack            flux;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 5, InspectorTrackBuilder::FillPattern);
        FrameBits (builder, bits);
        LatchFramer::Frame (*builder.MakeFluxCopy(), flux);

        Assert::IsTrue (flux.isFlux);
        Assert::IsTrue (GetValues (flux) == GetValues (bits));
        Assert::AreEqual (static_cast<size_t> (flux.cellCount), flux.cellTicks.size());
        Assert::AreEqual (31.29, flux.cellTicks[100], 0.5);
    }



    TEST_METHOD (TheDriveDeliversTheFramedNibblesOnABitTrack)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            track;
        vector<Byte>           delivered;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        builder.AppendNibble (0xD5, 2);
        builder.AppendNibble (0xE7, 1);
        builder.AppendSync   (10);
        FrameBits (builder, track);

        delivered = ReadWithEngine (*builder.MakeBitCopy(), builder.GetCellCount());

        Assert::IsTrue (delivered.size() + 2 >= track.nibbles.size() && delivered.size() <= track.nibbles.size() + 2);
        Assert::IsTrue (IsCyclicRun (GetValues (track), delivered));
    }



    TEST_METHOD (TheDriveDeliversTheFramedNibblesOnAFluxTrack)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            track;
        vector<Byte>           delivered;



        builder.AppendStandardTrack (DiskFieldKind::Thirteen, 254, 2, InspectorTrackBuilder::FillPattern);
        LatchFramer::Frame (*builder.MakeFluxCopy(), track);

        delivered = ReadWithEngine (*builder.MakeFluxCopy(), track.cellCount);

        Assert::IsTrue (IsCyclicRun (GetValues (track), delivered));
    }



    TEST_METHOD (ALongFluxGapIsRandom)
    {
        InspectorTrackBuilder  builder;
        FramedTrack            track;



        builder.AppendSync  (30);
        builder.AppendZeros (12);
        builder.AppendSync  (30);
        LatchFramer::Frame (*builder.MakeFluxCopy(), track);

        Assert::AreEqual (1, static_cast<int> (track.randomRegions.size()));
        Assert::AreEqual (60, static_cast<int> (track.nibbles.size()));
    }
};
