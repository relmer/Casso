#include "Pch.h"

#include "HResultAssert.h"
#include "InlineWorkQueue.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kUnpackStateBytes = 2048;
static constexpr uint64_t  s_kUnpackInterval   = 1000;
static constexpr uint64_t  s_kUnpackStride     = 100;
static constexpr uint32_t  s_kUnpackGroup      = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeUnpackerTests
//
//  A keyframe copied out of the store still packed and unpacked elsewhere
//  is the state the store restores, whole or difference, in the newest
//  group or an older one; the group's whole snapshot is copied only when the
//  unpacker does not hold it; and one recorded again at the same position
//  is never taken for the one it replaced.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeUnpackerTests)
{
public:

    TEST_METHOD (ACopiedKeyframeUnpacksToWhatTheStoreRestores)
    {
        constexpr size_t  kKeyframes = s_kUnpackGroup * 3;

        KeyframeStore      store;
        KeyframeUnpacker   unpacker;
        PackedKeyframe     packed;
        KeyframeCopy       copy     = KeyframeCopy::Gone;
        std::vector<Byte>  expected;
        std::vector<Byte>  actual;
        size_t             i        = 0;
        size_t             compared = 0;
        HRESULT            hr       = S_OK;



        Fill (store, kKeyframes, 1);

        //  Newest first, so older groups are met after the newest.
        for (i = store.GetCount(); i-- > 0; )
        {
            hr = store.Restore (i, expected);
            AssertSucceeded (hr, L"Restore");

            hr = store.CopyPacked (store.GetInfo (i).position, unpacker, packed, copy);
            AssertSucceeded (hr, L"CopyPacked");
            Assert::IsTrue (copy == KeyframeCopy::Copied, L"copied");

            hr = unpacker.Unpack (packed, actual);
            AssertSucceeded (hr, L"Unpack");

            Assert::IsTrue (actual == expected, L"the same state, byte for byte");
            compared++;
        }

        Assert::AreEqual<size_t> (kKeyframes, compared, L"every keyframe compared");
    }


    TEST_METHOD (TheWholeSnapshotIsCopiedOnlyWhenTheUnpackerLacksIt)
    {
        KeyframeStore      store;
        KeyframeUnpacker   unpacker;
        PackedKeyframe     packed;
        KeyframeCopy       copy  = KeyframeCopy::Gone;
        std::vector<Byte>  state;
        HRESULT            hr    = S_OK;



        Fill (store, s_kUnpackGroup * 2, 1);

        hr = store.CopyPacked (store.GetInfo (1).position, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked");
        Assert::IsFalse (packed.whole.empty(),      L"the unpacker holds nothing yet: the whole snapshot comes along");
        Assert::IsFalse (packed.difference.empty(), L"with the difference");

        hr = unpacker.Unpack (packed, state);
        AssertSucceeded (hr, L"Unpack");

        hr = store.CopyPacked (store.GetInfo (2).position, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked a neighbor");
        Assert::IsTrue  (packed.whole.empty(),      L"the same group: only the difference is copied");
        Assert::IsFalse (packed.difference.empty(), L"the difference");

        hr = unpacker.Unpack (packed, state);
        AssertSucceeded (hr, L"Unpack the neighbor");

        hr = store.CopyPacked (store.GetInfo (s_kUnpackGroup + 1).position, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked from the next group");
        Assert::IsFalse (packed.whole.empty(), L"another group: its whole snapshot comes along");
    }


    //  History past a keyframe dropped and recorded again differently puts a
    //  new whole snapshot at a position an old one had; its checksum tells
    //  them apart.
    TEST_METHOD (ASnapshotRecordedAgainAtTheSamePositionIsNotMistaken)
    {
        KeyframeStore      store;
        KeyframeUnpacker   unpacker;
        PackedKeyframe     packed;
        KeyframeCopy       copy     = KeyframeCopy::Gone;
        std::vector<Byte>  expected;
        std::vector<Byte>  actual;
        uint64_t           position = s_kUnpackGroup * s_kUnpackStride;
        HRESULT            hr       = S_OK;



        Fill (store, s_kUnpackGroup + 2, 1);

        hr = store.CopyPacked (position + s_kUnpackStride, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked");

        hr = unpacker.Unpack (packed, actual);
        AssertSucceeded (hr, L"Unpack, holding the second group's whole snapshot");

        hr = store.DropAfterPosition (position - s_kUnpackStride);
        AssertSucceeded (hr, L"DropAfterPosition");

        Append (store, s_kUnpackGroup, 2, 2);

        hr = store.CopyPacked (position + s_kUnpackStride, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked after recording again");
        Assert::IsFalse (packed.whole.empty(), L"the new whole snapshot comes along, though at the old one's position");

        hr = unpacker.Unpack (packed, actual);
        AssertSucceeded (hr, L"Unpack");

        hr = store.Restore (s_kUnpackGroup + 1, expected);
        AssertSucceeded (hr, L"Restore");

        Assert::IsTrue (actual == expected, L"the new history's state");
    }


    TEST_METHOD (AKeyframeBeingPackedIsPendingAndOneDroppedIsGone)
    {
        KeyframeStore      store;
        KeyframeUnpacker   unpacker;
        KeyframeSettings   settings;
        InlineWorkQueue    queue;
        PackedKeyframe     packed;
        KeyframeCopy       copy  = KeyframeCopy::Copied;
        std::vector<Byte>  state (s_kUnpackStateBytes, 0);
        HRESULT            hr    = S_OK;



        settings.intervalCycles = s_kUnpackInterval;
        settings.wholeEvery     = s_kUnpackGroup;
        settings.longestGroup   = s_kUnpackGroup;

        store.Configure    (settings);
        store.SetWorkQueue (&queue);

        hr = store.Add (0, 0, state);
        AssertSucceeded (hr, L"Add");

        hr = store.CopyPacked (0, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked");
        Assert::IsTrue (copy == KeyframeCopy::Pending, L"still being packed");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"and not waited for");

        Assert::IsTrue (queue.TryRunNext(), L"packed");

        hr = store.CopyPacked (0, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked once packed");
        Assert::IsTrue (copy == KeyframeCopy::Copied, L"copied");

        hr = store.CopyPacked (s_kUnpackStride, unpacker, packed, copy);
        AssertSucceeded (hr, L"CopyPacked where there is none");
        Assert::IsTrue (copy == KeyframeCopy::Gone, L"none there");
    }

private:

    static void Fill (KeyframeStore & store, size_t count, Byte salt)
    {
        KeyframeSettings  settings;



        settings.intervalCycles = s_kUnpackInterval;
        settings.wholeEvery     = s_kUnpackGroup;
        settings.longestGroup   = s_kUnpackGroup;

        store.Configure (settings);

        Append (store, 0, count, salt);
    }


    //  States that change a little each time, as a machine's do, so the
    //  differences are neither empty nor whole.
    static void Append (KeyframeStore & store, size_t first, size_t count, Byte salt)
    {
        constexpr size_t  kChangedEvery = 7;

        std::vector<Byte>  state (s_kUnpackStateBytes, 0);
        HRESULT            hr = S_OK;
        size_t             i  = 0;
        size_t             j  = 0;



        for (i = first; i < first + count; i++)
        {
            for (j = 0; j < state.size(); j += kChangedEvery)
            {
                state[j] = static_cast<Byte> (i * salt + j);
            }

            state[0] = static_cast<Byte> (i + salt);

            hr = store.Add (i * s_kUnpackStride, i * s_kUnpackInterval, state);
            AssertSucceeded (hr, L"Add");
        }
    }
};
