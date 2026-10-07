#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatCountDelta.h"
#include "Debugger/Reverse/SnapshotCompressor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatCountDeltaTests
//
//  The packed counts the heat map keeps beside every keyframe: what is added
//  comes back exactly, forward or taken away again, runs of the same count
//  cost a few bytes however long, and bytes that are not a delta fail
//  rather than being read past.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatCountDeltaTests)
    {
    public:

        static constexpr size_t  kEntries = AccessHeatMap::kEntryCount;


        //  Scattered counts of every size, single entries and runs, with
        //  short and long gaps between them.
        static std::vector<int64_t> MakeCounts (uint32_t seed)
        {
            constexpr uint32_t  kMultiplier = 1103515245;
            constexpr uint32_t  kIncrement  = 12345;
            constexpr uint32_t  kSparse     = 37;
            constexpr int       kShift      = 16;
            constexpr uint32_t  kLarge      = 0xFFFF;
            std::vector<int64_t>  counts (kEntries, 0);
            uint32_t              state    = seed;



            for (size_t i = 0; i < kEntries; i++)
            {
                state = state * kMultiplier + kIncrement;

                if ((state >> kShift) % kSparse == 0)
                {
                    counts[i] = (int64_t) ((state >> kShift) & kLarge) * ((i % 3) + 1);
                }
            }

            return counts;
        }



        TEST_METHOD (AddedCountsComeBackAndTakenAwayLeaveTheStart)
        {
            constexpr size_t      kRunStart = 0x2000;
            constexpr size_t      kRunEnd   = 0x4000;
            constexpr int64_t     kRunCount = 10;
            std::vector<int64_t>  earlier   = MakeCounts (1);
            std::vector<int64_t>  added     = MakeCounts (2);
            std::vector<int64_t>  later     = earlier;
            std::vector<int64_t>  rebuilt   = earlier;
            std::vector<Byte>     bytes;
            bool                  isApplied = false;



            for (size_t i = 0; i < kEntries; i++)
            {
                later[i] += added[i];
            }

            for (size_t i = kRunStart; i < kRunEnd; i++)
            {
                later[i] += kRunCount;
            }

            HeatCountDelta::Encode (later.data(), earlier.data(), kEntries, bytes);

            isApplied = HeatCountDelta::TryApply (bytes.data(), bytes.size(), false, rebuilt.data(), kEntries);
            Assert::IsTrue (isApplied, L"a delta just made applies");
            Assert::IsTrue (rebuilt == later, L"adding it to the earlier counts gives the later ones exactly");

            isApplied = HeatCountDelta::TryApply (bytes.data(), bytes.size(), true, rebuilt.data(), kEntries);
            Assert::IsTrue (isApplied, L"and takes away again");
            Assert::IsTrue (rebuilt == earlier, L"taking it away gives the earlier counts back");
        }


        TEST_METHOD (NothingChangedIsNoBytes)
        {
            std::vector<int64_t>  counts = MakeCounts (3);
            std::vector<Byte>     bytes  = { 1, 2, 3 };



            HeatCountDelta::Encode (counts.data(), counts.data(), kEntries, bytes);

            Assert::AreEqual ((size_t) 0, bytes.size());
        }


        TEST_METHOD (ARunOfOneCountCostsAFewBytesHoweverLong)
        {
            constexpr size_t      kRunStart  = 0x2000;
            constexpr size_t      kRunLength = 0x2000;
            constexpr size_t      kMostBytes = 8;
            std::vector<int64_t>  earlier    (kEntries, 0);
            std::vector<int64_t>  later      (kEntries, 0);
            std::vector<Byte>     bytes;



            for (size_t i = kRunStart; i < kRunStart + kRunLength; i++)
            {
                later[i] = 10;
            }

            HeatCountDelta::Encode (later.data(), earlier.data(), kEntries, bytes);

            Logger::WriteMessage (std::format ("a run of {} writes ten times each: {} bytes\n", kRunLength, bytes.size()).c_str());
            Assert::IsTrue (bytes.size() <= kMostBytes, std::format (L"{} bytes", bytes.size()).c_str());
        }


        TEST_METHOD (ACountThatFellIsTakenAsUnchanged)
        {
            std::vector<int64_t>  earlier (kEntries, 0);
            std::vector<int64_t>  later   (kEntries, 0);
            std::vector<int64_t>  rebuilt (kEntries, 0);
            std::vector<Byte>     bytes;



            earlier[5] = 9;
            later[6]   = 4;

            HeatCountDelta::Encode (later.data(), earlier.data(), kEntries, bytes);
            Assert::IsTrue (HeatCountDelta::TryApply (bytes.data(), bytes.size(), false, rebuilt.data(), kEntries));

            Assert::AreEqual ((int64_t) 0, rebuilt[5], L"a fall is no access");
            Assert::AreEqual ((int64_t) 4, rebuilt[6]);
        }


        TEST_METHOD (PackedDeltasComeBackExactlyAndSmall)
        {
            constexpr size_t      kStride  = 3;
            constexpr size_t      kCycle   = 7;
            SnapshotCompressor    compressor;
            std::vector<int64_t>  earlier  (kEntries, 0);
            std::vector<int64_t>  later    (kEntries, 0);
            std::vector<Byte>     delta;
            std::vector<Byte>     packed;
            std::vector<Byte>     unpacked;
            std::vector<Byte>     tiny     = { 0, 0 };
            HRESULT               hr       = S_OK;



            //  A loop's counts repeat from place to place, as a program's do.
            for (size_t i = 0; i < kEntries; i += kStride)
            {
                later[i] = (int64_t) (1 + i % kCycle);
            }

            HeatCountDelta::Encode (later.data(), earlier.data(), kEntries, delta);

            hr = HeatCountDelta::Pack (delta, compressor, packed);
            AssertSucceeded (hr, L"Pack");
            Assert::IsTrue (packed.size() < delta.size(), L"a delta of many counts packs smaller");

            hr = HeatCountDelta::Unpack (packed.data(), packed.size(), compressor, unpacked);
            AssertSucceeded (hr, L"Unpack");
            Assert::IsTrue (unpacked == delta, L"and comes back exactly");

            hr = HeatCountDelta::Pack (tiny, compressor, packed);
            AssertSucceeded (hr, L"Pack a delta packing cannot shrink");

            hr = HeatCountDelta::Unpack (packed.data(), packed.size(), compressor, unpacked);
            AssertSucceeded (hr, L"Unpack it");
            Assert::IsTrue (unpacked == tiny, L"kept as it was");

            packed.pop_back();

            hr = HeatCountDelta::Unpack (packed.data(), packed.size(), compressor, unpacked);
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"cut short, it fails");
        }


        TEST_METHOD (BytesThatAreNotADeltaAreRefused)
        {
            std::vector<int64_t>  earlier (kEntries, 0);
            std::vector<int64_t>  later   (kEntries, 0);
            std::vector<int64_t>  totals  (kEntries, 0);
            std::vector<Byte>     bytes;
            std::vector<Byte>     cut;
            std::vector<Byte>     pastTheEnd = { 0xFF, 0xFF, 0x7F, 0x00, 0x01 };



            later[100]          = 300;
            later[kEntries - 1] = 2;

            HeatCountDelta::Encode (later.data(), earlier.data(), kEntries, bytes);

            cut.assign (bytes.begin(), bytes.end() - 1);

            Assert::IsFalse (HeatCountDelta::TryApply (cut.data(), cut.size(), false, totals.data(), kEntries), L"a delta cut short");
            Assert::IsFalse (HeatCountDelta::TryApply (pastTheEnd.data(), pastTheEnd.size(), false, totals.data(), kEntries), L"a segment past the last entry");
            Assert::IsFalse (HeatCountDelta::TryApply (bytes.data(), bytes.size(), false, totals.data(), kEntries - 1), L"a delta for more entries than there are");
        }
    };
}
