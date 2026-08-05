#include "Pch.h"
#include "Core/Prng.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrngTests
//
//  Constitution §II compliant: pure unit tests, no host filesystem, no
//  registry, no network, no system APIs. SplitMix64 is fully deterministic
//  per seed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PrngTests)
{
public:

    TEST_METHOD (SplitMix64MatchesReference)
    {
        // Canonical Vigna SplitMix64 with seed = 0 produces these first
        // three 64-bit outputs. Any deviation means our implementation
        // diverged from the reference.
        Prng        prng (0);

        Assert::AreEqual (uint64_t (0xE220A8397B1DCDAFULL), prng.Next64());
        Assert::AreEqual (uint64_t (0x6E789E6AA1B965F4ULL), prng.Next64());
        Assert::AreEqual (uint64_t (0x06C45D188009454FULL), prng.Next64());
    }

    TEST_METHOD (SeedDeterminism)
    {
        constexpr uint64_t  kPinnedSeed = 0xCA550001ULL;
        size_t              i;
        Prng                a (kPinnedSeed);
        Prng                b (kPinnedSeed);

        for (i = 0; i < 1024; i++)
        {
            Assert::AreEqual (a.Next64(), b.Next64());
        }
    }

    TEST_METHOD (ZeroSeedProducesNonZeroSequence)
    {
        size_t      i;
        uint64_t    sample;



        Prng        prng (0);

        for (i = 0; i < 16; i++)
        {
            sample = prng.Next64();
            Assert::AreNotEqual (uint64_t (0), sample);
        }
    }
};
