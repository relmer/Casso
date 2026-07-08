#include "Pch.h"
#include <thread>

#include "Devices/Printer/PrinterByteRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


// A single ring instance carries 64 KiB of slot storage; on the stack that
// trips C6262. The ring is the system under test -- suppress for this file.
#pragma warning (disable: 6262)




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterByteRingTests
//
//  Single-threaded fundamentals, the FreeSpace high-water contract that
//  backs the card's ready bit, and a two-threaded stress test verifying FIFO
//  ordering with no dropped or torn bytes under sustained contention.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterByteRingTests
{
    TEST_CLASS (PrinterByteRingTests)
    {
    public:

        TEST_METHOD (EmptyPopReturnsFalse)
        {
            PrinterByteRing   ring;
            Byte              out  = 0;

            Assert::IsFalse (ring.TryPop (out));
            Assert::AreEqual ((uint32_t) 0, ring.ApproxSize ());
            Assert::AreEqual (PrinterByteRing::kByteRingCapacity, ring.FreeSpace ());
        }


        TEST_METHOD (PushFillsToCapacityThenRejects)
        {
            PrinterByteRing   ring;
            uint32_t          i    = 0;

            for (i = 0; i < PrinterByteRing::kByteRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPush ((Byte) (i & 0xFF)));
            }

            Assert::AreEqual (PrinterByteRing::kByteRingCapacity, ring.ApproxSize ());
            Assert::AreEqual ((uint32_t) 0, ring.FreeSpace ());

            // One-past-capacity push must fail without corrupting state.
            Assert::IsFalse (ring.TryPush (0xAB));
            Assert::AreEqual (PrinterByteRing::kByteRingCapacity, ring.ApproxSize ());
        }


        TEST_METHOD (FreeSpaceTracksInFlight)
        {
            PrinterByteRing   ring;
            Byte              out  = 0;
            uint32_t          i    = 0;

            for (i = 0; i < 100; i++)
            {
                Assert::IsTrue (ring.TryPush ((Byte) i));
            }

            Assert::AreEqual (PrinterByteRing::kByteRingCapacity - 100, ring.FreeSpace ());

            for (i = 0; i < 40; i++)
            {
                Assert::IsTrue (ring.TryPop (out));
            }

            Assert::AreEqual (PrinterByteRing::kByteRingCapacity - 60, ring.FreeSpace ());
        }


        TEST_METHOD (FifoOrderingOnFullCycle)
        {
            PrinterByteRing   ring;
            Byte              out  = 0;
            uint32_t          i    = 0;

            for (i = 0; i < PrinterByteRing::kByteRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPush ((Byte) (i & 0xFF)));
            }

            for (i = 0; i < PrinterByteRing::kByteRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPop (out));
                Assert::AreEqual ((Byte) (i & 0xFF), out);
            }

            Assert::IsFalse (ring.TryPop (out));
        }


        TEST_METHOD (DrainEmptiesRing)
        {
            PrinterByteRing   ring;
            Byte              buffer[32];
            uint32_t          i      = 0;
            uint32_t          pulled = 0;

            for (i = 0; i < 32; i++)
            {
                Assert::IsTrue (ring.TryPush ((Byte) (100 + i)));
            }

            pulled = ring.Drain (buffer, 32);
            Assert::AreEqual ((uint32_t) 32, pulled);
            Assert::AreEqual ((uint32_t) 0, ring.ApproxSize ());

            for (i = 0; i < 32; i++)
            {
                Assert::AreEqual ((Byte) (100 + i), buffer[i]);
            }
        }


        TEST_METHOD (DrainCapsAtMaxCount)
        {
            PrinterByteRing   ring;
            Byte              buffer[10];
            uint32_t          i      = 0;
            uint32_t          pulled = 0;

            for (i = 0; i < 25; i++)
            {
                Assert::IsTrue (ring.TryPush ((Byte) i));
            }

            pulled = ring.Drain (buffer, 10);
            Assert::AreEqual ((uint32_t) 10, pulled);
            Assert::AreEqual ((uint32_t) 15, ring.ApproxSize ());

            for (i = 0; i < 10; i++)
            {
                Assert::AreEqual ((Byte) i, buffer[i]);
            }
        }


        TEST_METHOD (WrapAroundPreservesFifo)
        {
            // Fill, partially drain, refill, fully drain -- exercises the
            // index-wrap behavior. The 32-bit counters increment
            // monotonically; the mask isolates the slot index.
            PrinterByteRing   ring;
            Byte              out  = 0;
            uint32_t          i    = 0;

            for (i = 0; i < PrinterByteRing::kByteRingCapacity; i++)
            {
                Assert::IsTrue (ring.TryPush ((Byte) (i & 0xFF)));
            }

            for (i = 0; i < 2048; i++)
            {
                Assert::IsTrue (ring.TryPop (out));
                Assert::AreEqual ((Byte) (i & 0xFF), out);
            }

            for (i = 0; i < 2048; i++)
            {
                uint32_t  seq = PrinterByteRing::kByteRingCapacity + i;
                Assert::IsTrue (ring.TryPush ((Byte) (seq & 0xFF)));
            }

            Assert::AreEqual (PrinterByteRing::kByteRingCapacity, ring.ApproxSize ());

            for (i = 0; i < PrinterByteRing::kByteRingCapacity; i++)
            {
                uint32_t  seq = 2048 + i;
                Assert::IsTrue (ring.TryPop (out));
                Assert::AreEqual ((Byte) (seq & 0xFF), out);
            }
        }


        TEST_METHOD (TwoThreadStressNoDropsNoReorder)
        {
            // One producer pushes 500,000 bytes whose values step through
            // 0..255 repeatedly; one consumer drains. Every push that
            // returns false is retried, so the consumer must observe every
            // byte exactly once in order -- popped value i must equal
            // (i mod 256).
            PrinterByteRing         ring;
            std::atomic<uint32_t>   failedPushes { 0 };
            constexpr uint32_t      kTotal       = 500000;

            std::thread  producer ([&]()
            {
                uint32_t  i = 0;

                for (i = 0; i < kTotal; i++)
                {
                    while (!ring.TryPush ((Byte) (i & 0xFF)))
                    {
                        failedPushes.fetch_add (1, std::memory_order_relaxed);
                        std::this_thread::yield ();
                    }
                }
            });

            uint32_t   seen = 0;
            Byte       out  = 0;

            while (seen < kTotal)
            {
                if (ring.TryPop (out))
                {
                    Assert::AreEqual ((Byte) (seen & 0xFF), out,
                                      L"bytes must arrive in FIFO order with no drops");
                    seen++;
                }
                else
                {
                    std::this_thread::yield ();
                }
            }

            producer.join ();

            Assert::AreEqual (kTotal, seen);
        }
    };
}
