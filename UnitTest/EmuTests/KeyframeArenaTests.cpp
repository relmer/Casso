#include "Pch.h"

#include "Debugger/Reverse/KeyframeArena.h"
#include "Debugger/Reverse/KeyframeLayout.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArenaTests
//
//  The keyframe store's parts apart from the store: the layout math, and an
//  arena small enough to wrap and to drop its oldest group after a few
//  keyframes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeArenaTests)
{
public:

    static constexpr size_t  kSlots      = 8;
    static constexpr size_t  kArenaBytes = 100;
    static constexpr size_t  kStateBytes = 64;


    //  Lists a keyframe at position and places packedBytes of it.
    static void AddKeyframe (KeyframeArena & arena, uint64_t position, bool isWhole, size_t packedBytes)
    {
        KeyframeInfo       info;
        std::vector<Byte>  packed (packedBytes, static_cast<Byte> (position));
        bool               isSideDropped = false;



        info.position   = position;
        info.cycle      = position * 10;
        info.stateBytes = kStateBytes;
        info.isWhole    = isWhole;

        (void) arena.List (info);
        AssertSucceeded (arena.PlaceNext (packed, {}, position, isSideDropped), L"PlaceNext");
    }


    TEST_METHOD (TheLeastArenaFitsTheNewestGroupAtTheWorstCase)
    {
        KeyframeSettings  settings;



        Assert::AreEqual ((size_t) (settings.wholeEvery + KeyframeLayout::kInFlight + 2) * KeyframeLayout::GetPackedBound (kStateBytes),
                          KeyframeLayout::GetLeastArena (settings, kStateBytes));
    }


    TEST_METHOD (ALayoutNeverGivesLessThanTheLeastArena)
    {
        KeyframeSettings  settings;
        size_t            slots = 0;
        size_t            bytes = 0;



        settings.budgetBytes = 1;

        KeyframeLayout::ComputeLayout (settings, kStateBytes, 0, KeyframeArena::GetEntryBytes(), slots, bytes);

        Assert::AreEqual (KeyframeLayout::GetLeastArena (settings, kStateBytes), bytes);
        Assert::AreEqual (2 * KeyframeLayout::GetGroupSlots (settings), slots, L"room for the newest group in half the table");
    }


    TEST_METHOD (AGroupEndsAtWholeEveryOnceItsDifferencesCostAWhole)
    {
        KeyframeSettings  settings;
        size_t            slots = 1000;



        Assert::IsFalse (KeyframeLayout::IsGroupDone (settings, settings.wholeEvery - 1, slots, 100, 10), L"short of wholeEvery");
        Assert::IsFalse (KeyframeLayout::IsGroupDone (settings, settings.wholeEvery,     slots, 5,   10), L"its differences cost less than a whole");
        Assert::IsTrue  (KeyframeLayout::IsGroupDone (settings, settings.wholeEvery,     slots, 10,  10), L"they cost a whole");
        Assert::IsTrue  (KeyframeLayout::IsGroupDone (settings, settings.longestGroup,   slots, 0,   10), L"the longest group");
    }


    TEST_METHOD (APlacedKeyframeIsFoundByPositionAndCycle)
    {
        KeyframeArena  arena;
        size_t         index = 0;



        AssertSucceeded (arena.Allocate (kSlots, kArenaBytes), L"Allocate");

        AddKeyframe (arena, 1, true,  20);
        AddKeyframe (arena, 2, false, 10);
        AddKeyframe (arena, 3, false, 10);

        Assert::AreEqual ((size_t) 3,  arena.GetCount());
        Assert::AreEqual ((size_t) 40, arena.GetStoredBytes());
        Assert::AreEqual ((size_t) 3,  arena.GetGroupLength());
        Assert::AreEqual ((size_t) 20, arena.GetGroupWhole());
        Assert::AreEqual ((size_t) 20, arena.GetGroupDiffs());

        Assert::IsTrue   (arena.TryFindByPosition (2, index));
        Assert::AreEqual ((size_t) 1, index);
        Assert::IsTrue   (arena.TryFindAtOrBefore (25, index), L"cycle 25 is after keyframe 2's cycle 20");
        Assert::AreEqual ((size_t) 1, index);
        Assert::IsFalse  (arena.TryFindByPosition (0, index), L"before every keyframe");
        Assert::AreEqual ((size_t) 0, arena.FindGroupStart (2));
        Assert::AreEqual ((Byte) 3, arena.GetPacked (2).data[0], L"each keyframe's bytes where it was placed");
    }


    TEST_METHOD (AFullArenaDropsItsOldestGroupAndWraps)
    {
        KeyframeArena               arena;
        std::vector<KeyframeDrop>   drops;



        AssertSucceeded (arena.Allocate (kSlots, kArenaBytes), L"Allocate");
        arena.SetDropCallback ([&] (KeyframeDrop drop) { drops.push_back (drop); });

        AddKeyframe (arena, 1, true,  40);
        AddKeyframe (arena, 2, false, 10);
        AddKeyframe (arena, 3, true,  40);

        // 90 of 100 bytes stored: the next 40 drop the first group.
        AddKeyframe (arena, 4, false, 40);

        Assert::AreEqual ((size_t) 2, arena.GetCount(), L"the oldest group went whole");
        Assert::AreEqual ((size_t) 2, drops.size());
        Assert::IsTrue   (drops[0] == KeyframeDrop::Oldest);
        Assert::AreEqual ((uint64_t) 3, arena.GetInfo (0).position);
        Assert::IsTrue   (arena.IsFull(), L"dropped for room");
        Assert::AreEqual ((Byte) 4, arena.GetPacked (1).data[0], L"the newest at the start of the arena");
    }


    TEST_METHOD (TheNewestGroupIsNeverDropped)
    {
        KeyframeArena  arena;



        AssertSucceeded (arena.Allocate (kSlots, kArenaBytes), L"Allocate");

        AddKeyframe (arena, 1, true,  40);
        AddKeyframe (arena, 2, false, 10);

        Assert::IsFalse  (arena.TryDropOldestGroup(), L"no whole keyframe follows it");
        Assert::AreEqual ((size_t) 2, arena.GetCount());
    }


    TEST_METHOD (DroppingTheNewestLeavesRoomAndRecountsTheGroup)
    {
        KeyframeArena  arena;



        AssertSucceeded (arena.Allocate (kSlots, kArenaBytes), L"Allocate");

        AddKeyframe (arena, 1, true,  20);
        AddKeyframe (arena, 2, false, 10);
        AddKeyframe (arena, 3, false, 10);

        Assert::IsTrue   (arena.DropNewerThan (true, 2));
        arena.RecountNewestGroup();

        Assert::AreEqual ((size_t) 2,  arena.GetCount());
        Assert::AreEqual ((size_t) 30, arena.GetStoredBytes());
        Assert::AreEqual ((size_t) 2,  arena.GetGroupLength());
        Assert::AreEqual ((size_t) 10, arena.GetGroupDiffs());
        Assert::IsFalse  (arena.DropNewerThan (true, 2), L"nothing past position 2");
    }


    TEST_METHOD (AnUnplacedKeyframeIsListedAndDroppedWithoutBytes)
    {
        KeyframeArena  arena;
        KeyframeInfo   info;



        AssertSucceeded (arena.Allocate (kSlots, kArenaBytes), L"Allocate");

        AddKeyframe (arena, 1, true, 20);

        info.position   = 2;
        info.stateBytes = kStateBytes;
        (void) arena.List (info);

        Assert::AreEqual ((size_t) 1, arena.GetUnplacedCount());
        Assert::AreEqual ((size_t) 1, arena.GetPlacedCount());

        arena.DropUnplaced();

        Assert::AreEqual ((size_t) 1,  arena.GetCount());
        Assert::AreEqual ((size_t) 20, arena.GetStoredBytes());
    }
};
