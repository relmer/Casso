#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventCacheTests
//
//  The ring of the newest events: appends in position order, the oldest
//  overwritten when full, where it is complete, a cut back to the machine's
//  position, gap marks, and spans copied out.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebugEventCacheTests)
{
public:

    using Cache = DebugEventCache<int>;


    static void  Fill (Cache & cache, uint64_t from, uint64_t count)
    {
        for (uint64_t position = from; position < from + count; position++)
        {
            cache.Append (position, position * 4, (int) position);
        }
    }


    TEST_METHOD (EntriesComeBackOldestFirst)
    {
        Cache  cache (8);



        cache.Reset (10);
        Fill (cache, 10, 5);

        Assert::AreEqual ((size_t) 5, cache.GetCount());
        Assert::AreEqual ((uint64_t) 10, cache.GetAt (0).position);
        Assert::AreEqual ((uint64_t) 56, cache.GetAt (4).cycle);
        Assert::AreEqual (14,            cache.GetAt (4).event);
        Assert::AreEqual ((uint64_t) 10, cache.GetCoveredFrom(), L"complete from where it was reset");
        Assert::IsFalse  (cache.HasDropped());
    }


    TEST_METHOD (AFullRingOverwritesTheOldestAndIsCompleteAfterIt)
    {
        Cache  cache (4);



        cache.Reset (0);
        Fill (cache, 0, 6);

        Assert::AreEqual ((size_t) 4, cache.GetCount());
        Assert::AreEqual ((uint64_t) 2, cache.GetAt (0).position, L"0 and 1 overwritten");
        Assert::AreEqual ((uint64_t) 5, cache.GetAt (3).position);
        Assert::AreEqual ((uint64_t) 2, cache.GetCoveredFrom(), L"complete after the last one overwritten");
        Assert::IsTrue   (cache.HasDropped());
    }


    //  Entries at the position of one overwritten may remain, so the cache
    //  is complete only after that position.
    TEST_METHOD (AnOverwrittenPositionIsNotComplete)
    {
        Cache  cache (3);



        cache.Reset (0);
        cache.Append (7, 0, 1);
        cache.Append (7, 1, 2);
        cache.Append (7, 2, 3);
        cache.Append (8, 3, 4);

        Assert::AreEqual ((uint64_t) 8, cache.GetCoveredFrom(), L"two events at 7 remain, the first is gone");
    }


    TEST_METHOD (ACutBackDropsTheMachinesFuture)
    {
        Cache  cache (16);



        cache.Reset (0);
        Fill (cache, 0, 10);
        cache.CutBack (6);

        Assert::AreEqual ((size_t) 6, cache.GetCount(), L"events at 6 and after are the machine's future");
        Assert::AreEqual ((uint64_t) 5, cache.GetAt (5).position);
        Assert::AreEqual ((uint64_t) 0, cache.GetCoveredFrom());

        Fill (cache, 6, 2);
        Assert::AreEqual ((size_t) 8, cache.GetCount(), L"running on appends again");
    }


    //  A cut to before where the cache is complete empties it, and it is
    //  complete from there as the machine runs on.
    TEST_METHOD (ACutBeforeTheCoverageMovesItThere)
    {
        Cache  cache (4);



        cache.Reset (0);
        Fill (cache, 0, 10);
        cache.CutBack (3);

        Assert::AreEqual ((size_t) 0, cache.GetCount());
        Assert::AreEqual ((uint64_t) 3, cache.GetCoveredFrom());
    }


    TEST_METHOD (AGapIsAnEntryOfItsOwn)
    {
        Cache  cache (8);



        cache.Reset (0);
        cache.Append  (1, 4, 1);
        cache.MarkGap (9, 36);
        cache.Append  (9, 36, 9);

        Assert::AreEqual ((size_t) 3, cache.GetCount());
        Assert::IsTrue   (cache.GetAt (1).kind == DebugEventEntryKind::Gap);
        Assert::AreEqual ((uint64_t) 9, cache.GetAt (1).position);
        Assert::IsTrue   (cache.GetAt (2).kind == DebugEventEntryKind::Event);
    }


    TEST_METHOD (ASpanIsCopiedFromItsStartUpToItsEnd)
    {
        Cache                                cache (6);
        std::vector<DebugEventEntry<int>>   span;



        cache.Reset (0);
        Fill (cache, 0, 9);
        cache.CopySpan (4, 7, span);

        Assert::AreEqual ((size_t) 3, span.size());
        Assert::AreEqual (4, span.front().event);
        Assert::AreEqual (6, span.back().event, L"the end is not part of the span");
        Assert::AreEqual ((size_t) 0, cache.FindFirstAtOrAfter (2), L"before the oldest, the oldest");
        Assert::AreEqual ((size_t) 6, cache.FindFirstAtOrAfter (20), L"after the newest, the count");
    }
};
