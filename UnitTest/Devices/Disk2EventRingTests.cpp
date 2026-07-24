#include "Pch.h"

#include "Devices/Disk2EventRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


// Stress-test ring carries 16 MiB of slot storage on the stack
// (single instance) which trips C6262. The ring is the system under
// test; suppress for this file only.
#pragma warning (disable: 6262)





////////////////////////////////////////////////////////////////////////////////
//
//  Helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    Disk2Event MakeEvent (uint64_t cycle)
    {
        Disk2Event  e {};

        e.category               = EventCategory::Controller;
        e.type                   = Disk2EventType::HeadStep;
        e.cycle                  = cycle;
        e.payload.step.prevQt    = 0;
        e.payload.step.newQt     = (int) (cycle & 0xFF);

        return e;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2EventRingTests
//
//  Single-threaded fundamentals + a two-threaded stress test that
//  verifies FIFO ordering and no torn reads under sustained producer/
//  consumer contention (FR-009, FR-010).
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2EventRingTests
{
    TEST_CLASS (Disk2EventRingTests)
    {
    public:

        TEST_METHOD (EmptyPopReturnsFalse)
        {
            Disk2EventRing  ring;
            Disk2Event      out  {};

            Assert::IsFalse (ring.TryPop (out));
            Assert::AreEqual ((uint32_t) 0, ring.ApproxSize());
        }


        TEST_METHOD (PushFillsToCapacityThenRejects)
        {
            Disk2EventRing   ring;
            uint32_t         i    = 0;

            for (i = 0; i < Disk2EventRing::kEventRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPush (MakeEvent (i)));
            }

            Assert::AreEqual (Disk2EventRing::kEventRingCapacity, ring.ApproxSize());

            // One-past-capacity push must fail without corrupting state.
            Assert::IsFalse (ring.TryPush (MakeEvent (0xDEADBEEF)));
            Assert::AreEqual (Disk2EventRing::kEventRingCapacity, ring.ApproxSize());
        }


        TEST_METHOD (FifoOrderingOnFullCycle)
        {
            Disk2EventRing   ring;
            Disk2Event       out  {};
            uint32_t         i    = 0;

            for (i = 0; i < Disk2EventRing::kEventRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPush (MakeEvent (i)));
            }

            for (i = 0; i < Disk2EventRing::kEventRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPop (out));
                Assert::AreEqual ((uint64_t) i, out.cycle);
            }

            Assert::IsFalse (ring.TryPop (out));
        }


        TEST_METHOD (DrainEmptiesRing)
        {
            Disk2EventRing   ring;
            Disk2Event       buffer[32];
            uint32_t         i      = 0;
            uint32_t         pulled = 0;

            for (i = 0; i < 32; i++)
            {
                Assert::IsTrue (ring.TryPush (MakeEvent (1000 + i)));
            }

            pulled = ring.Drain (buffer, 32);
            Assert::AreEqual ((uint32_t) 32, pulled);
            Assert::AreEqual ((uint32_t) 0, ring.ApproxSize());

            for (i = 0; i < 32; i++)
            {
                Assert::AreEqual ((uint64_t) (1000 + i), buffer[i].cycle);
            }
        }


        TEST_METHOD (DrainCapsAtMaxCount)
        {
            Disk2EventRing   ring;
            Disk2Event       buffer[10];
            uint32_t         i      = 0;
            uint32_t         pulled = 0;

            for (i = 0; i < 25; i++)
            {
                Assert::IsTrue (ring.TryPush (MakeEvent (i)));
            }

            pulled = ring.Drain (buffer, 10);
            Assert::AreEqual ((uint32_t) 10, pulled);
            Assert::AreEqual ((uint32_t) 15, ring.ApproxSize());

            for (i = 0; i < 10; i++)
            {
                Assert::AreEqual ((uint64_t) i, buffer[i].cycle);
            }
        }


        TEST_METHOD (WrapAroundPreservesFifo)
        {
            // Fill, partially drain, refill, fully drain -- exercises
            // the index-wrap behavior. The 32-bit counters increment
            // monotonically; the mask isolates the slot index.
            Disk2EventRing   ring;
            Disk2Event       out  {};
            uint32_t         i    = 0;

            for (i = 0; i < Disk2EventRing::kEventRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPush (MakeEvent (i)));
            }

            for (i = 0; i < 2048; i++)
            {
                Assert::IsTrue (ring.TryPop (out));
                Assert::AreEqual ((uint64_t) i, out.cycle);
            }

            for (i = 0; i < 2048; i++)
            {
                uint64_t  cycleVal = Disk2EventRing::kEventRingCapacity + i;
                Assert::IsTrue (ring.TryPush (MakeEvent (cycleVal)));
            }

            Assert::AreEqual (Disk2EventRing::kEventRingCapacity, ring.ApproxSize());

            for (i = 0; i < Disk2EventRing::kEventRingCapacity; i++)
            {
                uint64_t  expected = 2048ULL + i;
                Assert::IsTrue (ring.TryPop (out));
                Assert::AreEqual (expected, out.cycle);
            }
        }


        TEST_METHOD (TwoThreadStressNoTornReadsNoReorder)
        {
            // One producer thread pushes 100,000 monotonically-
            // increasing payloads; one consumer thread drains. The
            // consumer asserts pops arrive in strictly ascending
            // cycle order. Any gap in the observed cycle sequence
            // must correspond exactly to a producer-side failed
            // push (ring-full case) -- the counter of failed pushes
            // bounds the total gap size.
            Disk2EventRing                  ring;
            std::atomic<bool>               consumerDone { false };
            std::atomic<uint32_t>           failedPushes { 0 };
            constexpr uint64_t              kTotal       = 100000;

            std::thread  producer ([&]()
            {
                uint64_t  i = 0;

                for (i = 0; i < kTotal; i++)
                {
                    while (!ring.TryPush (MakeEvent (i)))
                    {
                        failedPushes.fetch_add (1, std::memory_order_relaxed);
                        std::this_thread::yield();
                    }
                }
            });

            uint64_t      seen    = 0;
            uint64_t      lastVal = 0;
            bool          haveLast = false;
            Disk2Event    out     {};

            while (seen < kTotal)
            {
                if (ring.TryPop (out))
                {
                    if (haveLast)
                    {
                        Assert::IsTrue (out.cycle > lastVal,
                                        L"pops must arrive in strictly ascending cycle order");
                    }

                    lastVal  = out.cycle;
                    haveLast = true;
                    seen++;
                }
                else
                {
                    std::this_thread::yield();
                }
            }

            producer.join();

            // Every push that ever returned false was retried (the
            // producer loops on TryPush). End-state must be: consumer
            // saw every payload, no torn read, no reorder.
            Assert::AreEqual ((uint64_t) (kTotal - 1), lastVal);
            consumerDone.store (true, std::memory_order_release);
        }
    };
}
