#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleEngineFluxTests
//
//  Playing a flux track by time: the sequencer reads the same nibbles from a
//  nominal flux track as from the bit track it came from, pulses land on the
//  clock their recorded time falls in, nothing drifts over many revolutions,
//  long gaps turn into noise, and the head keeps its place round the disk
//  when it moves between bit and flux tracks.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2NibbleEngineFluxTests)
{
public:

    static constexpr int     kBitBitQt     = 0;
    static constexpr int     kFluxQt       = 4;
    static constexpr double  kUnitsPerTick = 45.0;

    // A run of self-sync FF bytes, an address-style prologue, sixteen data
    // nibbles and an epilogue, repeated until the track is full length.
    static const vector<uint8_t> & GetDataNibbles()
    {
        static const vector<uint8_t>  data =
        {
            0xD5, 0xAA, 0x96, 0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
            0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3, 0xDE, 0xAA, 0xEB,
        };

        return data;
    }


    static void AppendByte (vector<uint8_t> & bits, uint8_t value, int width)
    {
        int  i = 0;

        for (i = 0; i < 8; i++)
        {
            bits.push_back (static_cast<uint8_t> ((value >> (7 - i)) & 1));
        }

        for (i = 8; i < width; i++)
        {
            bits.push_back (0);
        }
    }


    static void MakeTrackBits (size_t minBits, vector<Byte> & packed, size_t & bitCount)
    {
        vector<uint8_t>  bits;
        size_t           i = 0;

        while (bits.size() < minBits)
        {
            for (i = 0; i < 40; i++)
            {
                AppendByte (bits, 0xFF, 10);
            }

            for (i = 0; i < GetDataNibbles().size(); i++)
            {
                AppendByte (bits, GetDataNibbles()[i], 8);
            }
        }

        bitCount = bits.size();
        packed.assign ((bitCount + 7) / 8, 0);

        for (i = 0; i < bitCount; i++)
        {
            packed[i >> 3] = static_cast<Byte> (packed[i >> 3] | (bits[i] << (7 - (i & 7))));
        }
    }


    // Slot 0 is a bit track at quarter track 0; slot 1 is a flux track at
    // quarter track 4.
    static void MakeDisk (DiskImage & disk, const vector<Byte> & packed, size_t bitCount, const vector<Byte> & flux)
    {
        disk.EnsureTrackSlots (2);
        disk.ClearQuarterTrackMap();

        disk.ResizeTrack (0, bitCount);
        memcpy (disk.GetTrackBitsForWrite (0).data(), packed.data(),
                min (packed.size(), disk.GetTrackBitsForWrite (0).size()));
        disk.SetQuarterTrackSlot (kBitBitQt, 0);

        disk.SetFluxTrack (1, flux);
        disk.SetQuarterTrackSlot (kFluxQt, 1);
    }


    static vector<uint8_t> ReadNibbles (Disk2NibbleEngine & eng, uint32_t cycles)
    {
        vector<uint8_t>  nibbles;
        uint8_t          nib = 0;
        uint32_t         c   = 0;

        for (c = 0; c < cycles; c++)
        {
            eng.Tick (1);

            if (eng.ConsumeFreshNibble (nib))
            {
                nibbles.push_back (nib);
            }
        }

        return nibbles;
    }


    static bool Contains (const vector<uint8_t> & haystack, const vector<uint8_t> & needle)
    {
        return search (haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }


    static void StartOn (Disk2NibbleEngine & eng, DiskImage & disk, int quarterTrack)
    {
        eng.SetDiskImage    (&disk);
        eng.SetCurrentTrack (quarterTrack);
        eng.SetMotorOn      (true);
    }


    TEST_METHOD (NominalFluxReadsTheSameNibblesAsItsBitTrack)
    {
        DiskImage          disk;
        Disk2NibbleEngine  bitEng;
        Disk2NibbleEngine  fluxEng;
        vector<Byte>       packed;
        size_t             bitCount = 0;

        MakeTrackBits (51200, packed, bitCount);
        MakeDisk (disk, packed, bitCount, FluxTestImages::BitsToNominalFlux (packed, bitCount));

        StartOn (bitEng,  disk, kBitBitQt);
        StartOn (fluxEng, disk, kFluxQt);

        Assert::IsTrue (fluxEng.IsOnFluxTrack());
        Assert::IsTrue (Contains (ReadNibbles (bitEng,  40000), GetDataNibbles()));
        Assert::IsTrue (Contains (ReadNibbles (fluxEng, 40000), GetDataNibbles()),
                        L"a nominal flux track must read the nibbles it was made from");
    }


    TEST_METHOD (OffNominalStretchesStillDecode)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<Byte>       packed;
        size_t             bitCount = 0;
        vector<Byte>       flux;

        MakeTrackBits (51200, packed, bitCount);
        flux = FluxTestImages::BitsToFlux (packed, bitCount,
                   FluxTestImages::MakeAlternating (bitCount, 3000,
                                                    FluxTestImages::kNominalCellTicks * 1.03,
                                                    FluxTestImages::kNominalCellTicks * 0.97));
        MakeDisk (disk, packed, bitCount, flux);

        StartOn (eng, disk, kFluxQt);

        Assert::IsTrue (Contains (ReadNibbles (eng, 60000), GetDataNibbles()));
    }


    //  Every pulse is handed to the sequencer on the clock its recorded time
    //  falls in, so the spacing the sequencer sees is the recorded spacing to
    //  within one clock -- for 3.7 us cells as much as for 4.1 us ones.
    TEST_METHOD (PulsesLandWithinOneClockOfTheirRecordedTime)
    {
        constexpr size_t   kStretch = 1000;

        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<Byte>       packed;
        size_t             bitCount  = 4 * kStretch;
        vector<Byte>       flux;
        vector<uint64_t>   clocks;
        uint64_t           lastCount = 0;
        uint32_t           c         = 0;
        size_t             s         = 0;
        size_t             j         = 0;
        const double       kStep     = Disk2NibbleEngine::kFluxUnitsPerLssClock / kUnitsPerTick;

        // Every cell a 1, alternating 3.7 us and 4.1 us stretches.
        packed.assign (bitCount / 8, 0xFF);
        flux = FluxTestImages::BitsToFlux (packed, bitCount,
                   FluxTestImages::MakeAlternating (bitCount, kStretch,
                                                    FluxTestImages::kFastCellTicks,
                                                    FluxTestImages::kSlowCellTicks));
        MakeDisk (disk, packed, 8, flux);

        StartOn (eng, disk, kFluxQt);

        for (c = 0; c < 20000 && clocks.size() < 3 * kStretch; c++)
        {
            eng.Tick (1);

            if (eng.GetFluxPulseCount() != lastCount)
            {
                clocks.push_back (eng.GetLastFluxPulseClock());
                lastCount = eng.GetFluxPulseCount();
            }
        }

        Assert::AreEqual (3 * kStretch, clocks.size());

        // Pulse j is transition j, which is cell j of stretch j / 1000.
        for (s = 0; s < 3; s++)
        {
            double  cell = (s % 2 == 0) ? FluxTestImages::kFastCellTicks : FluxTestImages::kSlowCellTicks;
            double  sum  = 0;
            size_t  n    = 0;

            for (j = s * kStretch + 2; j < (s + 1) * kStretch - 2; j++)
            {
                double  spacing = (clocks[j] - clocks[j - 1]) / kUnitsPerTick;

                Assert::IsTrue (fabs (spacing - cell) <= kStep,
                                L"a pulse reached the sequencer more than one clock from its recorded time");
                sum += spacing;
                n++;
            }

            Assert::AreEqual (cell, sum / n, 0.05,
                              L"a stretch's cells must reach the sequencer at the length the image holds");
        }
    }


    TEST_METHOD (FluxTimeDoesNotDriftOverManyRevolutions)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<Byte>       flux (1000, 32);
        vector<Byte>       packed (1, 0);
        uint64_t           revUnits = 32000 * 45;
        uint64_t           start    = 0;
        uint64_t           cycles   = 0;
        uint64_t           pulse    = 0;

        MakeDisk (disk, packed, 8, flux);
        StartOn (eng, disk, kFluxQt);

        start  = eng.GetFluxTime();
        cycles = 1000 * revUnits / (2 * Disk2NibbleEngine::kFluxUnitsPerLssClock);

        eng.Tick (static_cast<uint32_t> (cycles));

        Assert::AreEqual (start + cycles * 2 * Disk2NibbleEngine::kFluxUnitsPerLssClock, eng.GetFluxTime());

        // A thousand transitions a revolution, a thousand revolutions.
        Assert::IsTrue (eng.GetFluxPulseCount() >= 999999 && eng.GetFluxPulseCount() <= 1000000,
                        L"transitions were lost or doubled over many revolutions");

        // The last pulse still sits on the 32-tick grid the track was
        // written on, a thousand revolutions later.
        pulse = eng.GetLastFluxPulseClock();
        Assert::IsTrue ((pulse % (32 * 45)) < Disk2NibbleEngine::kFluxUnitsPerLssClock,
                        L"pulses drifted off the recorded grid");
    }


    TEST_METHOD (LongGapReadsAsNoiseThatChangesEachRevolution)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<Byte>       flux (200, 32);
        vector<Byte>       packed (1, 0);
        uint64_t           revCycles = 0;
        uint64_t           first     = 0;
        uint64_t           second    = 0;
        uint64_t           before    = 0;

        // 200 cells of flux, then a gap of about 200 cells with nothing in it.
        flux.insert (flux.end(), 24, 255);
        flux.push_back (120);

        MakeDisk (disk, packed, 8, flux);
        StartOn (eng, disk, kFluxQt);

        revCycles = disk.GetFluxTrack (1).GetTotalTicks() * 45 / (2 * Disk2NibbleEngine::kFluxUnitsPerLssClock);

        before = eng.GetFluxPulseCount();
        eng.Tick (static_cast<uint32_t> (revCycles));
        first  = eng.GetFluxPulseCount() - before;

        before = eng.GetFluxPulseCount();
        eng.Tick (static_cast<uint32_t> (revCycles));
        second = eng.GetFluxPulseCount() - before;

        // 201 real transitions a revolution; the gap adds random ones.
        Assert::IsTrue (first  > 205, L"the gap must produce weak-bit pulses");
        Assert::IsTrue (second > 205, L"the gap must produce weak-bit pulses");
        Assert::AreNotEqual (first, second, L"weak bits must differ from one revolution to the next");
    }


    TEST_METHOD (TrackWithNoTransitionsReadsNoise)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<Byte>       packed (1, 0);
        vector<uint8_t>    nibbles;

        MakeDisk (disk, packed, 8, {});
        StartOn (eng, disk, kFluxQt);

        Assert::IsTrue (eng.IsOnFluxTrack());

        nibbles = ReadNibbles (eng, 20000);

        Assert::IsTrue (eng.GetFluxPulseCount() > 100, L"a blank flux track must still produce noise");
        Assert::IsTrue (nibbles.size() > 10);
    }


    TEST_METHOD (HeadKeepsItsAngleMovingBetweenBitAndFlux)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;
        vector<Byte>       packed;
        size_t             bitCount  = 0;
        double             fluxAngle = 0;
        double             bitAngle  = 0;
        uint64_t           revUnits  = 0;

        MakeTrackBits (51200, packed, bitCount);
        MakeDisk (disk, packed, bitCount, FluxTestImages::BitsToNominalFlux (packed, bitCount));

        StartOn (eng, disk, kBitBitQt);
        eng.Tick (static_cast<uint32_t> (bitCount * Disk2NibbleEngine::kCyclesPerBit / 3));

        bitAngle = static_cast<double> (eng.GetBitPosition()) / bitCount;

        eng.SetCurrentTrack (kFluxQt);
        Assert::IsTrue (eng.IsOnFluxTrack());

        revUnits  = disk.GetFluxTrack (1).GetRevolutionTicks() * 45;
        fluxAngle = static_cast<double> (eng.GetFluxTime() % revUnits) / revUnits;

        Assert::AreEqual (bitAngle, fluxAngle, 1.0 / bitCount, L"bit to flux must keep the angle");

        eng.Tick (static_cast<uint32_t> (bitCount * Disk2NibbleEngine::kCyclesPerBit / 5));
        fluxAngle = static_cast<double> (eng.GetFluxTime() % revUnits) / revUnits;

        eng.SetCurrentTrack (kBitBitQt);
        Assert::IsFalse (eng.IsOnFluxTrack());

        bitAngle = static_cast<double> (eng.GetBitPosition()) / bitCount;
        Assert::AreEqual (fluxAngle, bitAngle, 2.0 / bitCount, L"flux to bit must keep the angle");
    }


    TEST_METHOD (BitTrackStepsKeepTheirModuloRule)
    {
        DiskImage          disk;
        Disk2NibbleEngine  eng;

        disk.EnsureTrackSlots (2);
        disk.ClearQuarterTrackMap();
        disk.ResizeTrack (0, 1000);
        disk.ResizeTrack (1, 600);
        disk.SetQuarterTrackSlot (0, 0);
        disk.SetQuarterTrackSlot (4, 1);

        StartOn (eng, disk, 0);
        eng.Tick (800 * Disk2NibbleEngine::kCyclesPerBit);

        Assert::AreEqual (static_cast<size_t> (800), eng.GetBitPosition());

        eng.SetCurrentTrack (4);
        Assert::AreEqual (static_cast<size_t> (200), eng.GetBitPosition(),
                          L"bit to bit keeps the cursor modulo the new length, as before");
    }
};
