#include "Pch.h"

#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStreamTests
//
//  The pair the link's tests run on: what one end writes, the other reads,
//  in order, and a close applies to both ends; bytes sent before it are readable.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InProcessByteStreamTests)
{
public:

    TEST_METHOD (WritesArriveAtTheOtherEndInOrder)
    {
        std::shared_ptr<InProcessByteStream>  first;
        std::shared_ptr<InProcessByteStream>  second;
        const Byte                            sent[]      = { 1, 2, 3, 0xFF };
        Byte                                  received[8] = {};
        size_t                                got         = 0;

        InProcessByteStream::CreatePair (first, second);

        Assert::AreEqual (S_OK, first->Write (sent, std::size (sent)));
        Assert::AreEqual<size_t> (std::size (sent), second->GetReadableCount());
        Assert::AreEqual<size_t> (0, first->GetReadableCount());

        Assert::AreEqual (S_OK, second->Read (received, std::size (received), 0, got));
        Assert::AreEqual<size_t> (std::size (sent), got);
        Assert::IsTrue (std::equal (sent, sent + got, received));
    }


    TEST_METHOD (ReadTakesNoMoreThanItsCapacity)
    {
        std::shared_ptr<InProcessByteStream>  first;
        std::shared_ptr<InProcessByteStream>  second;
        const Byte                            sent[]      = { 10, 20, 30 };
        Byte                                  received[2] = {};
        size_t                                got         = 0;

        InProcessByteStream::CreatePair (first, second);
        Assert::AreEqual (S_OK, first->Write (sent, std::size (sent)));

        Assert::AreEqual (S_OK, second->Read (received, std::size (received), 0, got));
        Assert::AreEqual<size_t> (2, got);
        Assert::AreEqual<size_t> (1, second->GetReadableCount());
    }


    TEST_METHOD (ReadWithNothingWaitingReturnsNothingAfterItsTimeout)
    {
        std::shared_ptr<InProcessByteStream>  first;
        std::shared_ptr<InProcessByteStream>  second;
        Byte                                  received[4] = {};
        size_t                                got         = 1;

        InProcessByteStream::CreatePair (first, second);

        Assert::AreEqual (S_OK, second->Read (received, std::size (received), 0, got));
        Assert::AreEqual<size_t> (0, got);
    }


    TEST_METHOD (ReadWakesWhenTheOtherEndWritesFromAnotherThread)
    {
        std::shared_ptr<InProcessByteStream>  first;
        std::shared_ptr<InProcessByteStream>  second;
        Byte                                  received[4] = {};
        size_t                                got         = 0;
        const Byte                            value       = 0x42;
        std::thread                           writer;

        InProcessByteStream::CreatePair (first, second);

        writer = std::thread ([first, value]()
        {
            HRESULT  hr = first->Write (&value, 1);

            IGNORE_RETURN_VALUE (hr, S_OK);
        });

        Assert::AreEqual (S_OK, second->Read (received, std::size (received), 5000, got));
        writer.join();

        Assert::AreEqual<size_t> (1, got);
        Assert::AreEqual<Byte> (value, received[0]);
    }


    TEST_METHOD (CloseAppliesToBothEndsAndSentBytesRemainReadable)
    {
        std::shared_ptr<InProcessByteStream>  first;
        std::shared_ptr<InProcessByteStream>  second;
        const Byte                            value       = 7;
        Byte                                  received[4] = {};
        size_t                                got         = 0;
        HRESULT                               hr          = S_OK;

        InProcessByteStream::CreatePair (first, second);
        Assert::AreEqual (S_OK, first->Write (&value, 1));

        first->Close();

        Assert::IsTrue (first->IsClosed());
        Assert::IsTrue (second->IsClosed());
        hr = second->Write (&value, 1);
        Assert::IsTrue (FAILED (hr));

        Assert::AreEqual (S_OK, second->Read (received, std::size (received), 0, got));
        Assert::AreEqual<size_t> (1, got);
    }
};
