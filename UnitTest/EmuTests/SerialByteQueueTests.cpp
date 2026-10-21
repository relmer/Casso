#include "Pch.h"

#include "Devices/SerialByteQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace SerialByteQueueTestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialByteQueueTests
    //
    //  The queue between a serial attachment and its 6551. Order and count
    //  are what the chip relies on; the cross-thread case is what a socket or
    //  COM-port reader relies on.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialByteQueueTests)
    {
    public:
        TEST_METHOD (PopsInPushOrder)
        {
            const Byte       bytes[] = { 1, 2, 3 };
            SerialByteQueue  queue;
            Byte             value   = 0;



            queue.Push (bytes, size (bytes));
            Assert::AreEqual<size_t> (3, queue.GetCount());

            for (Byte expected : bytes)
            {
                Assert::IsTrue (queue.TryPop (value));
                Assert::AreEqual (expected, value);
            }

            Assert::IsFalse (queue.TryPop (value), L"an empty queue pops nothing");
            Assert::AreEqual<size_t> (0, queue.GetCount());
        }


        TEST_METHOD (ClearEmptiesTheQueue)
        {
            const Byte       bytes[] = { 9, 8 };
            SerialByteQueue  queue;



            queue.Push (bytes, size (bytes));
            queue.Clear();

            Assert::AreEqual<size_t> (0, queue.GetCount());
        }


        TEST_METHOD (NullOrEmptyPushAddsNothing)
        {
            const Byte       bytes[] = { 1 };
            SerialByteQueue  queue;



            queue.Push (nullptr, 4);
            queue.Push (bytes, 0);

            Assert::AreEqual<size_t> (0, queue.GetCount());
        }


        TEST_METHOD (PushFromAnotherThreadIsSeenInOrder)
        {
            constexpr int    kCount = 10000;
            SerialByteQueue  queue;
            vector<Byte>     popped;
            Byte             value  = 0;



            thread  producer ([&queue] ()
            {
                for (int i = 0; i < kCount; ++i)
                {
                    Byte  b = static_cast<Byte> (i);

                    queue.Push (&b, 1);
                }
            });

            while (popped.size() < static_cast<size_t> (kCount))
            {
                if (queue.TryPop (value))
                {
                    popped.push_back (value);
                }
            }

            producer.join();

            for (int i = 0; i < kCount; ++i)
            {
                Assert::AreEqual (static_cast<Byte> (i), popped[i]);
            }
        }
    };
}
