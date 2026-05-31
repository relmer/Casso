#include "Pch.h"
#include "Devices/InputEvent.h"
#include "Devices/InputEventRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


// The ring carries kEventRingCapacity InputEvent slots inline, so a
// single stack-allocated instance trips C6262. The ring is the system
// under test; suppress for this file only.
#pragma warning (disable: 6262)




namespace
{
    InputEvent MakeGuestRead (Word address, Byte value, uint64_t cycle)
    {
        InputEvent  e = {};

        e.category        = InputEventCategory::Guest;
        e.type            = InputEventType::KbdDataRead;
        e.cycle           = cycle;
        e.payload.io.address = address;
        e.payload.io.value   = value;
        e.payload.io.flags   = (value & 0x80) != 0 ? InputEvent::kFlagStrobe : 0;

        return e;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  InputEventRingTests
//
//  Headless unit tests for the single-producer / single-consumer ring
//  carrying InputEvent values from the CPU thread to the UI thread.
//  Verifies FIFO ordering, drain semantics, full-ring overflow, and the
//  counter-wrap correctness inherited from the Vyukov layout.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InputEventRingTests)
{
public:

    TEST_METHOD (PushThenPop_RoundTripsOneEvent)
    {
        InputEventRing  ring;
        InputEvent      out = {};

        Assert::IsTrue (ring.TryPush (MakeGuestRead (0xC000, 0xC1, 42)),
            L"First push into an empty ring must succeed");

        Assert::IsTrue (ring.TryPop (out),
            L"Pop must succeed when one event is in flight");

        Assert::AreEqual (static_cast<uint16_t> (0xC000), out.payload.io.address,
            L"Popped address must match the pushed address");
        Assert::AreEqual (static_cast<uint8_t> (0xC1), out.payload.io.value,
            L"Popped value must match the pushed value");
        Assert::AreEqual (static_cast<uint64_t> (42), out.cycle,
            L"Popped cycle stamp must match the pushed cycle");
    }

    TEST_METHOD (Pop_FailsOnEmptyRing)
    {
        InputEventRing  ring;
        InputEvent      out = {};

        Assert::IsFalse (ring.TryPop (out),
            L"Pop on an empty ring must return false");
    }

    TEST_METHOD (PushPop_PreservesFifoOrder)
    {
        InputEventRing  ring;
        InputEvent      out = {};

        for (uint8_t i = 0; i < 8; i++)
        {
            Assert::IsTrue (ring.TryPush (MakeGuestRead (0xC000, i, i)),
                L"Sequential pushes must all succeed");
        }

        for (uint8_t i = 0; i < 8; i++)
        {
            Assert::IsTrue (ring.TryPop (out),
                L"Sequential pops must all succeed");
            Assert::AreEqual (i, out.payload.io.value,
                L"Pops must observe values in push (FIFO) order");
        }
    }

    TEST_METHOD (Drain_ReturnsAllPendingInOrder)
    {
        InputEventRing  ring;
        InputEvent      batch[16] = {};
        uint32_t        drained   = 0;

        for (uint8_t i = 0; i < 10; i++)
        {
            ring.TryPush (MakeGuestRead (0xC010, i, i));
        }

        drained = ring.Drain (batch, 16);

        Assert::AreEqual (static_cast<uint32_t> (10), drained,
            L"Drain must report exactly the number of in-flight events");

        for (uint8_t i = 0; i < 10; i++)
        {
            Assert::AreEqual (i, batch[i].payload.io.value,
                L"Drain must preserve FIFO order");
        }
    }

    TEST_METHOD (Drain_RespectsMaxCount)
    {
        InputEventRing  ring;
        InputEvent      batch[4] = {};
        uint32_t        drained  = 0;

        for (uint8_t i = 0; i < 10; i++)
        {
            ring.TryPush (MakeGuestRead (0xC000, i, i));
        }

        drained = ring.Drain (batch, 4);

        Assert::AreEqual (static_cast<uint32_t> (4), drained,
            L"Drain must not write more than maxCount entries");
        Assert::AreEqual (static_cast<uint8_t> (0), batch[0].payload.io.value,
            L"Partial drain must start at the oldest event");
        Assert::AreEqual (static_cast<uint32_t> (6), ring.ApproxSize (),
            L"Six events must remain after a partial drain of four");
    }

    TEST_METHOD (Push_FailsWhenRingIsFull)
    {
        InputEventRing  ring;
        uint32_t        accepted = 0;

        for (uint32_t i = 0; i < InputEventRing::kEventRingCapacity; i++)
        {
            if (ring.TryPush (MakeGuestRead (0xC000, 0, i)))
            {
                accepted++;
            }
        }

        Assert::AreEqual (InputEventRing::kEventRingCapacity, accepted,
            L"A fresh ring must accept exactly kEventRingCapacity events");

        Assert::IsFalse (ring.TryPush (MakeGuestRead (0xC000, 0, 9999)),
            L"The push past capacity must fail so the producer can drop it");
    }

    TEST_METHOD (DrainAfterFull_RefillsWithoutWrapCorruption)
    {
        InputEventRing  ring;
        InputEvent      batch[InputEventRing::kEventRingCapacity] = {};
        uint32_t        drained = 0;

        // Fill, drain half, refill: exercises the head/tail counters past
        // a slot-index wrap so a masking or wrap bug would surface as a
        // mis-ordered or dropped event.
        for (uint32_t i = 0; i < InputEventRing::kEventRingCapacity; i++)
        {
            ring.TryPush (MakeGuestRead (0xC000, static_cast<Byte> (i & 0xFF), i));
        }

        drained = ring.Drain (batch, InputEventRing::kEventRingCapacity / 2);

        Assert::AreEqual (InputEventRing::kEventRingCapacity / 2, drained,
            L"Half drain must return half the capacity");

        for (uint32_t i = 0; i < InputEventRing::kEventRingCapacity / 2; i++)
        {
            Assert::IsTrue (ring.TryPush (MakeGuestRead (0xC010, 0, i)),
                L"After freeing half the ring, half a capacity of pushes must succeed");
        }

        Assert::AreEqual (InputEventRing::kEventRingCapacity, ring.ApproxSize (),
            L"The ring must again be full after the refill");
    }
};
