#include "Pch.h"

#include "Debugger/Reverse/ArenaExtents.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ArenaExtentsTests
//
//  The arena's free space: the first extent that fits is taken from its
//  start, freed extents merge with free neighbors, a hole freed in the middle
//  is reused, a full arena has no room, and the list does not grow past the
//  room reserved for it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ArenaExtentsTests)
{
public:

    TEST_METHOD (AllocationsAreTakenFromTheFirstExtentThatFits)
    {
        ArenaExtents  arena;
        size_t        a = 0;
        size_t        b = 0;



        arena.Reset (1000, 8);

        Assert::IsTrue   (arena.TryAllocate (100, a), L"the first");
        Assert::IsTrue   (arena.TryAllocate (200, b), L"the second");
        Assert::AreEqual<size_t> (0,   a, L"at the start");
        Assert::AreEqual<size_t> (100, b, L"right after it");
        Assert::AreEqual<size_t> (700, arena.GetFreeBytes(), L"the rest free");
    }


    TEST_METHOD (AHoleFreedInTheMiddleIsReused)
    {
        ArenaExtents  arena;
        size_t        a = 0;
        size_t        b = 0;
        size_t        c = 0;
        size_t        d = 0;



        arena.Reset (1000, 8);

        (void) arena.TryAllocate (100, a);
        (void) arena.TryAllocate (100, b);
        (void) arena.TryAllocate (100, c);

        arena.Free (b, 100);

        Assert::AreEqual<size_t> (2, arena.GetExtentCount(), L"the hole and the tail");
        Assert::IsTrue   (arena.TryAllocate (60, d), L"a smaller one fits");
        Assert::AreEqual (b, d, L"in the hole");
        Assert::IsTrue   (arena.TryAllocate (60, d), L"one too large for what is left of it");
        Assert::AreEqual<size_t> (300, d, L"goes to the tail");
    }


    TEST_METHOD (FreedNeighborsMerge)
    {
        ArenaExtents  arena;
        size_t        a = 0;
        size_t        b = 0;
        size_t        c = 0;
        size_t        d = 0;



        arena.Reset (300, 8);

        (void) arena.TryAllocate (100, a);
        (void) arena.TryAllocate (100, b);
        (void) arena.TryAllocate (100, c);

        arena.Free (a, 100);
        arena.Free (c, 100);

        Assert::AreEqual<size_t> (2, arena.GetExtentCount(), L"two holes");

        arena.Free (b, 100);

        Assert::AreEqual<size_t> (1, arena.GetExtentCount(), L"one extent once the middle is freed");
        Assert::IsTrue   (arena.TryAllocate (300, d), L"the whole arena fits again");
        Assert::AreEqual<size_t> (0, d, L"from the start");
    }


    TEST_METHOD (AFullArenaHasNoRoom)
    {
        ArenaExtents  arena;
        size_t        a = 0;



        arena.Reset (200, 8);

        Assert::IsTrue  (arena.TryAllocate (200, a), L"the whole arena");
        Assert::IsFalse (arena.TryAllocate (1, a),   L"then no more");
        Assert::AreEqual<size_t> (0, arena.GetExtentCount(), L"no free extent");

        arena.Clear();

        Assert::AreEqual<size_t> (200, arena.GetFreeBytes(), L"all free after a clear");
    }


    TEST_METHOD (TheListStaysWithinItsReservedRoom)
    {
        constexpr size_t  kCount = 64;

        ArenaExtents         arena;
        std::vector<size_t>  offsets (kCount);



        arena.Reset (kCount * 10, kCount);

        for (size_t i = 0; i < kCount; i++)
        {
            (void) arena.TryAllocate (10, offsets[i]);
        }

        //  Freeing every other one makes as many holes as can ever be free.
        for (size_t i = 0; i < kCount; i += 2)
        {
            arena.Free (offsets[i], 10);
        }

        Assert::AreEqual<size_t> (kCount / 2, arena.GetExtentCount(), L"a hole for each one freed");
        Assert::IsTrue (arena.GetExtentCount() <= kCount + 1, L"never more than one past the extents allocated");
    }
};
