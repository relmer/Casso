#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Reverse/UndoRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kModelSpacing    = 64;
static constexpr uint64_t  s_kModelInterval   = 256;
static constexpr int       s_kModelSteps      = 20000;
static constexpr uint32_t  s_kModelSeed       = 12345;
static constexpr uint32_t  s_kModelPushWeight = 950;
static constexpr uint32_t  s_kModelJumpWeight = 2;
static constexpr uint32_t  s_kModelPerMille   = 1000;
static constexpr uint32_t  s_kModelTruncation = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  UndoRingModelTests
//
//  The record ring against a plain map of what it should hold, over a long
//  random run of pushes, truncations and restarts, so every wrap of the
//  write slot and every truncation across it is checked record by record.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UndoRingModelTests)
{
public:

    TEST_METHOD (TheRingHoldsTheNewestRecordsOfItsRunWhateverTheOrderOfOperations)
    {
        UndoRing                          ring;
        UndoRingSettings                  settings;
        std::map<uint64_t, UndoRecord>    model;
        std::mt19937                      random   (s_kModelSeed);
        uint64_t                          next     = 1000;
        uint32_t                          roll     = 0;
        int                               step     = 0;
        size_t                            capacity = 0;
        size_t                            wraps    = 0;



        settings.checkpointCycles = s_kModelSpacing;
        settings.budgetBytes      = 0;

        ring.Configure (settings, s_kModelInterval);

        capacity = ring.GetByteCount() / sizeof (UndoRecord);
        Assert::IsTrue (capacity > 0 && capacity < s_kModelSteps / 4, L"a ring small enough to wrap many times");

        for (step = 0; step < s_kModelSteps; step++)
        {
            roll = random() % s_kModelPerMille;

            if (roll < s_kModelPushWeight)
            {
                ring.Push (next, MakeRecord (next));
                model[next] = MakeRecord (next);
                next++;
            }
            else if (roll < s_kModelPushWeight + s_kModelJumpWeight)
            {
                next += 1 + random() % s_kModelInterval;

                ring.Push (next, MakeRecord (next));
                model.clear();
                model[next] = MakeRecord (next);
                next++;
            }
            else
            {
                next -= std::min<uint64_t> (next - ring.GetFirstPosition(), random() % s_kModelTruncation);

                ring.TruncateAt (next);
                model.erase (model.lower_bound (next), model.end());
            }

            CheckAgainst (ring, model, next, step);

            wraps += (model.size() == capacity) ? 1 : 0;
        }

        Assert::IsTrue (wraps > 0, L"the run filled the ring, so the write slot wrapped");
    }

private:

    static UndoRecord MakeRecord (uint64_t position)
    {
        UndoRecord  record;



        record.cycle = position * 3;
        record.pc    = static_cast<Word> (position);
        record.sp    = static_cast<Byte> (position >> 3);

        return record;
    }


    //  A ring drops its oldest records only for want of room: it holds the
    //  newest of the model's records that fit, and exactly those.
    static void CheckAgainst (
        const UndoRing                  & ring,
        std::map<uint64_t, UndoRecord>  & model,
        uint64_t                          next,
        int                               step)
    {
        size_t      capacity = ring.GetByteCount() / sizeof (UndoRecord);
        size_t      expected = std::min (model.size(), capacity);
        UndoRecord  record;
        uint64_t    position = 0;
        bool        isHeld   = false;
        bool        isSame   = false;



        model.erase (model.begin(), model.lower_bound (next - expected));

        Assert::AreEqual<uint64_t> (next,            ring.GetEndPosition(),   std::format (L"end at step {}", step).c_str());
        Assert::AreEqual<uint64_t> (next - expected, ring.GetFirstPosition(), std::format (L"first at step {}", step).c_str());

        if (ring.GetFirstPosition() > 0)
        {
            isHeld = ring.TryGetRecord (ring.GetFirstPosition() - 1, record);
            Assert::IsFalse (isHeld, std::format (L"nothing before the first at step {}", step).c_str());
        }

        // Compared by hand and reported only on a mismatch: formatting a
        // message for every record of every step costs most of the run.
        for (position = ring.GetFirstPosition(); position < ring.GetEndPosition(); position++)
        {
            isHeld  = ring.TryGetRecord (position, record);
            isSame  = isHeld && record.cycle == model[position].cycle && record.pc == model[position].pc;

            if (!isSame)
            {
                Assert::Fail (std::format (L"record {} at step {}", position, step).c_str());
            }
        }
    }};
