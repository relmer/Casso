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


        TEST_METHOD (EachEntryHasItsErrorBits)
        {
            const Byte       bytes[] = { 7 };
            SerialByteQueue  queue;
            Byte             value   = 0;
            Byte             errors  = 0;



            queue.Push (bytes, size (bytes));
            queue.PushEntry (0x41, 0x02);
            queue.PushEntry (0x00, 0x03);

            Assert::IsTrue (queue.TryPop (value, errors));
            Assert::AreEqual<Byte> (7, value);
            Assert::AreEqual<Byte> (0, errors, L"bytes pushed as a run have no errors");

            Assert::IsTrue (queue.TryPop (value, errors));
            Assert::AreEqual<Byte> (0x41, value);
            Assert::AreEqual<Byte> (0x02, errors);

            Assert::IsTrue (queue.TryPop (value));
            Assert::AreEqual<Byte> (0x00, value, L"the plain pop returns the value alone");
        }


        TEST_METHOD (CopyToListsValuesAndErrorsOldestFirst)
        {
            SerialByteQueue                       queue;
            std::vector<SerialByteQueue::Entry>   copied;



            queue.PushEntry (1, 0);
            queue.PushEntry (2, 4);

            queue.CopyTo (copied);

            Assert::AreEqual<size_t> (2, copied.size());
            Assert::AreEqual<Byte> (1, copied[0].value);
            Assert::AreEqual<Byte> (2, copied[1].value);
            Assert::AreEqual<Byte> (4, copied[1].errors);
            Assert::AreEqual<size_t> (2, queue.GetCount(), L"a copy does not empty the queue");
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
