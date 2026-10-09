#include "Pch.h"

#include "Debugger/CallRecordCopies.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopiesTests
//
//  The copies of the call record kept at history's keyframes: a record packs
//  to bytes that unpack to every field of it, and bytes that are short, run
//  on or hold an unknown kind do not unpack; a copy equal to the one before
//  it holds no bytes of its own; copies are found at or before a position,
//  replaced, kept and dropped; the oldest go over the budget; and laying the
//  arena out again keeps every copy's bytes.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CallRecordCopiesTests)
    {
    public:

        static constexpr size_t  kCopyCount = 100;



        //  A record two calls deep below where history starts, the inner one
        //  an interrupt, guessed, unverified, rewritten and risen, with a
        //  symbol and a note, below a TXS; and a last return past its inline
        //  parameters. The inner call's target varies it.
        static CallRecord MakeRecord (Word target)
        {
            CallRecord                record;
            CallStackFrame            frame;
            CallStackRecorder::Break  each;



            record.isActive   = true;
            record.startCycle = 0x123456789ull;

            frame.callSite   = 0x0810;
            frame.target     = 0x0840;
            frame.stackLevel = 0xFF;
            frame.cycle      = 1234;
            record.frames.push_back (frame);

            frame.callSite    = 0x0842;
            frame.target      = target;
            frame.kind        = CallFrameKind::Irq;
            frame.provenance  = CallProvenance::Guessed;
            frame.stackLevel  = 0xFD;
            frame.cycle       = 0xFFFFFFFFFFull;
            frame.isVerified  = false;
            frame.isRewritten = true;
            frame.hasRisen    = true;
            frame.symbol      = "INNER";
            frame.note        = "return address changed by the store at $0860";
            record.frames.push_back (frame);

            each.info  = CallStackBreak { CallBreakKind::HistoryBegan, 0x0800, 0xA2 };
            each.depth = 0;
            record.breaks.push_back (each);

            each.info  = CallStackBreak { CallBreakKind::Txs, 0x0861, 0x9A };
            each.depth = 2;
            record.breaks.push_back (each);

            frame             = CallStackFrame();
            frame.callSite    = 0x0870;
            frame.target      = 0x0880;
            frame.note        = "returned 3 bytes past its call, over inline parameters";
            record.lastReturn = frame;

            return record;
        }



        static std::vector<Byte> MakePacked (Word target)
        {
            std::vector<Byte>  packed;



            CallRecordCopies::Pack (MakeRecord (target), packed);
            return packed;
        }



        static bool IsSame (std::span<const Byte> a, const std::vector<Byte> & b)
        {
            return std::ranges::equal (a, b);
        }



        static void AssertSameFrame (const CallStackFrame & expected, const CallStackFrame & actual)
        {
            Assert::AreEqual (expected.callSite,         actual.callSite,         L"call site");
            Assert::AreEqual (expected.target,           actual.target,           L"target");
            Assert::AreEqual ((int) expected.kind,       (int) actual.kind,       L"kind");
            Assert::AreEqual ((int) expected.provenance, (int) actual.provenance, L"provenance");
            Assert::AreEqual (expected.stackLevel,       actual.stackLevel,       L"stack level");
            Assert::AreEqual (expected.cycle,            actual.cycle,            L"cycle");
            Assert::AreEqual (expected.isVerified,       actual.isVerified,       L"verified");
            Assert::AreEqual (expected.isRewritten,      actual.isRewritten,      L"rewritten");
            Assert::AreEqual (expected.hasRisen,         actual.hasRisen,         L"risen");
            Assert::AreEqual (expected.symbol,           actual.symbol,           L"symbol");
            Assert::AreEqual (expected.note,             actual.note,             L"note");
        }



        //  Every field of a record comes back from its bytes.
        TEST_METHOD (ARecordPacksToBytesThatUnpackToIt)
        {
            CallRecord         expected = MakeRecord (0x0850);
            CallRecord         actual;
            std::vector<Byte>  packed;
            HRESULT            hr       = S_OK;
            size_t             i        = 0;



            CallRecordCopies::Pack (expected, packed);

            hr = CallRecordCopies::Unpack (packed, actual);
            AssertSucceeded (hr, L"Unpack");

            Assert::AreEqual (expected.isActive,      actual.isActive,      L"on");
            Assert::AreEqual (expected.startCycle,    actual.startCycle,    L"dated from the same cycle");
            Assert::AreEqual (expected.frames.size(), actual.frames.size(), L"as many frames");
            Assert::AreEqual (expected.breaks.size(), actual.breaks.size(), L"as many breaks");

            for (i = 0; i < expected.frames.size(); i++)
            {
                AssertSameFrame (expected.frames[i], actual.frames[i]);
            }

            for (i = 0; i < expected.breaks.size(); i++)
            {
                Assert::AreEqual ((int) expected.breaks[i].info.kind, (int) actual.breaks[i].info.kind, L"break kind");
                Assert::AreEqual (expected.breaks[i].info.pc,         actual.breaks[i].info.pc,         L"break pc");
                Assert::AreEqual (expected.breaks[i].info.opcode,     actual.breaks[i].info.opcode,     L"break opcode");
                Assert::AreEqual (expected.breaks[i].depth,           actual.breaks[i].depth,           L"break depth");
            }

            Assert::IsTrue (actual.lastReturn.has_value(), L"the last return");
            AssertSameFrame (*expected.lastReturn, *actual.lastReturn);
        }


        //  Bytes cut short anywhere, bytes with one more on the end, and a
        //  break of a kind there is none of are not a record.
        TEST_METHOD (BytesThatAreNotARecordDoNotUnpack)
        {
            static constexpr Byte  kNoSuchKind = 0xC8;

            std::vector<Byte>  packed   = MakePacked (0x0850);
            CallRecord         record   = MakeRecord (0x0850);
            CallRecord         unpacked;
            HRESULT            hr       = S_OK;
            size_t             length   = 0;



            for (length = 0; length < packed.size(); length++)
            {
                hr = CallRecordCopies::Unpack (std::span<const Byte> (packed.data(), length), unpacked);
                Assert::IsTrue (FAILED (hr), std::format (L"cut short to {} bytes", length).c_str());
            }

            packed.push_back (0);

            hr = CallRecordCopies::Unpack (packed, unpacked);
            Assert::IsTrue (FAILED (hr), L"a byte more than the record");

            record.breaks[1].info.kind = (CallBreakKind) kNoSuchKind;
            CallRecordCopies::Pack (record, packed);

            hr = CallRecordCopies::Unpack (packed, unpacked);
            Assert::IsTrue (FAILED (hr), L"a break of no known kind");
        }


        //  A copy whose bytes match those of the copy before it uses them
        //  rather than its own, so it costs only its place in the index.
        TEST_METHOD (ACopyEqualToTheOneBeforeHoldsNoBytesOfItsOwn)
        {
            std::vector<Byte>  first  = MakePacked (0x0850);
            std::vector<Byte>  second = MakePacked (0x0860);
            CallRecordCopies   shared;
            CallRecordCopies   distinct;



            Assert::AreEqual (first.size(), second.size(), L"two records of one size");

            shared.Set (10, first);
            shared.Set (20, first);

            distinct.Set (10, first);
            distinct.Set (20, second);

            Assert::IsTrue (distinct.GetUsedByteCount() - shared.GetUsedByteCount() >= first.size(), L"the second copy's bytes are saved");

            Assert::IsTrue (IsSame (shared.GetPacked (20),   first),  L"and it still holds them");
            Assert::IsTrue (IsSame (distinct.GetPacked (20), second), L"a copy that differs holds its own");
        }


        //  The newest copy at or before a position is found; one dropped,
        //  the oldest included, is not.
        TEST_METHOD (CopiesAreFoundAtOrBeforeAPosition)
        {
            std::vector<Byte>  packed   = MakePacked (0x0850);
            CallRecordCopies   copies;
            uint64_t           position = 0;
            bool               isFound  = false;



            copies.Set (10, packed);
            copies.Set (30, packed);
            copies.Set (20, packed);

            isFound = copies.TryFindAtOrBefore (25, position);
            Assert::IsTrue   (isFound && position == 20, L"the newest before");

            isFound = copies.TryFindAtOrBefore (30, position);
            Assert::IsTrue   (isFound && position == 30, L"one at the position itself");

            isFound = copies.TryFindAtOrBefore (5, position);
            Assert::IsFalse  (isFound, L"none before the oldest");

            copies.Drop (20);

            isFound = copies.TryFindAtOrBefore (25, position);
            Assert::IsTrue   (isFound && position == 10, L"a dropped copy is passed over");
            Assert::IsTrue   (copies.GetPacked (20).empty(), L"and holds nothing");

            copies.Drop (10);

            isFound = copies.TryFindAtOrBefore (25, position);
            Assert::IsFalse  (isFound, L"the oldest dropped too");
            Assert::AreEqual ((size_t) 1, copies.GetCount(), L"one left");
            Assert::AreEqual ((uint64_t) 30, copies.GetPosition (0), L"the newest");
            Assert::IsTrue   (IsSame (copies.GetPacked (30), packed), L"with its bytes");
        }


        //  Set replaces a copy kept at a position; TryAdd keeps it.
        TEST_METHOD (SetReplacesACopyAndTryAddKeepsIt)
        {
            std::vector<Byte>  first   = MakePacked (0x0850);
            std::vector<Byte>  second  = MakePacked (0x0860);
            CallRecordCopies   copies;
            bool               isAdded = false;



            copies.Set (10, first);

            isAdded = copies.TryAdd (10, second);

            Assert::IsFalse (isAdded, L"TryAdd keeps the copy there");
            Assert::IsTrue  (IsSame (copies.GetPacked (10), first), L"unchanged");

            copies.Set (10, second);

            Assert::IsTrue   (IsSame (copies.GetPacked (10), second), L"Set replaces it");
            Assert::AreEqual ((size_t) 1, copies.GetCount(), L"still one copy");
        }


        //  Over the budget the oldest copies go and the newest stay, and
        //  what the copies use stays within it.
        TEST_METHOD (TheOldestCopiesGoOverTheBudget)
        {
            static constexpr size_t  kKept = 4;

            std::vector<Byte>  packed  = MakePacked (0x0850);
            CallRecordCopies   copies;
            uint64_t           found   = 0;
            size_t             budget  = 0;
            size_t             i       = 0;
            bool               isFound = false;



            copies.Set (0, packed);
            budget = copies.GetUsedByteCount() * kKept;
            copies.SetBudget (budget);

            for (i = 1; i < kCopyCount; i++)
            {
                copies.Set (i, MakePacked ((Word) i));
            }

            isFound = copies.TryFindAtOrBefore (0, found);

            Assert::IsTrue  (copies.GetCount() <= kKept, L"no more than the budget holds");
            Assert::IsTrue  (copies.GetCount() > 0,      L"and some");
            Assert::IsFalse (isFound,                    L"the oldest went");
            Assert::IsTrue  (IsSame (copies.GetPacked (kCopyCount - 1), MakePacked ((Word) (kCopyCount - 1))), L"the newest stayed");
            Assert::IsTrue  (copies.GetUsedByteCount() <= budget, L"within the budget");
        }


        //  Once the bytes no copy uses are half the arena it is laid out
        //  again in place: every copy left, shared bytes included, keeps its
        //  own, and new copies fill the room the dropped ones left without
        //  the arena growing.
        TEST_METHOD (LayingTheArenaOutAgainKeepsEveryCopy)
        {
            static constexpr size_t  kKeepEvery = 3;
            static constexpr size_t  kNewFrom   = kCopyCount * 2;
            static constexpr size_t  kNewCount  = kCopyCount / 2;

            std::vector<Byte>  shared = MakePacked (0x0900);
            CallRecordCopies   copies;
            size_t             before = 0;
            size_t             i      = 0;



            for (i = 0; i < kCopyCount; i++)
            {
                copies.Set (i, MakePacked ((Word) i));
            }

            copies.Set (kCopyCount,     shared);
            copies.Set (kCopyCount + 1, shared);

            before = copies.GetByteCount();

            for (i = 0; i < kCopyCount; i++)
            {
                if (i % kKeepEvery != 0)
                {
                    copies.Drop (i);
                }
            }

            for (i = 0; i < kNewCount; i++)
            {
                copies.Set (kNewFrom + i, MakePacked ((Word) i));
            }

            Assert::IsTrue (copies.GetByteCount() <= before, L"new copies fill the room the dropped ones left");

            for (i = 0; i < kCopyCount; i += kKeepEvery)
            {
                Assert::IsTrue (IsSame (copies.GetPacked (i), MakePacked ((Word) i)), std::format (L"copy {} kept its bytes", i).c_str());
            }

            for (i = 0; i < kNewCount; i++)
            {
                Assert::IsTrue (IsSame (copies.GetPacked (kNewFrom + i), MakePacked ((Word) i)), std::format (L"new copy {} holds its bytes", i).c_str());
            }

            Assert::IsTrue (IsSame (copies.GetPacked (kCopyCount),     shared), L"a shared block moved once");
            Assert::IsTrue (IsSame (copies.GetPacked (kCopyCount + 1), shared), L"for both its copies");
        }
    };
}
