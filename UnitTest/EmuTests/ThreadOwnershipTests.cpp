#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Core/ThreadOwnership.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace UnitTestHelpers;


static constexpr std::chrono::seconds  s_kHoldDeadline { 5 };





////////////////////////////////////////////////////////////////////////////////
//
//  HoldingThread
//
//  A second thread that claims a token and parks with it until Finish, so the
//  test thread can call Claim, Release or the check while another thread holds
//  it. Every assertion stays on the test thread: an EHM assert on this thread
//  would call Assert::Fail off the test thread and end the test host.
//
////////////////////////////////////////////////////////////////////////////////

class HoldingThread
{
public:
    explicit HoldingThread (ThreadOwnership & ownership) :
        m_ownership (ownership),
        m_thread    ([this] { Run(); })
    {
    }

    ~HoldingThread()
    {
        Finish();
    }

    HoldingThread             (const HoldingThread &) = delete;
    HoldingThread & operator= (const HoldingThread &) = delete;

    //  Whether the thread took the token before the deadline.
    bool WaitUntilHeld()
    {
        std::future_status  status = m_held.get_future().wait_for (s_kHoldDeadline);



        return status == std::future_status::ready && m_isHeldAtStart;
    }

    //  Lets the thread release the token and end, then joins it.
    void Finish()
    {
        if (m_thread.joinable())
        {
            m_finish.set_value();
            m_thread.join();
        }
    }

    //  Whether the thread still held the token when Finish let it go.
    bool WasHeldAtFinish() const
    {
        return m_isHeldAtFinish;
    }

private:
    void Run()
    {
        std::future<void>  finish = m_finish.get_future();



        m_ownership.Claim();
        m_isHeldAtStart = m_ownership.IsHeldByCurrentThread();
        m_held.set_value();

        finish.wait_for (s_kHoldDeadline);

        m_isHeldAtFinish = m_ownership.IsHeldByCurrentThread();
        m_ownership.Release();
    }

    ThreadOwnership     & m_ownership;
    std::promise<void>    m_held;
    std::promise<void>    m_finish;
    bool                  m_isHeldAtStart  = false;
    bool                  m_isHeldAtFinish = false;
    std::thread           m_thread;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadOwnershipTests
//
//  One thread at a time holds a token. The constructing thread holds it first;
//  it moves only by Release on the holder and then Claim on the new thread. A
//  claim or release by a thread that does not hold a held token asserts and
//  leaves the holder as it was.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ThreadOwnershipTests)
{
public:

    TEST_METHOD (AnOwnershipIsHeldByTheThreadThatMadeIt)
    {
        ThreadOwnership  ownership;
        bool             isHeldElsewhere = true;
        std::thread      other ([&] { isHeldElsewhere = ownership.IsHeldByCurrentThread(); });



        other.join();

        Assert::IsTrue  (ownership.IsHeldByCurrentThread(), L"the constructing thread holds the token");
        Assert::IsFalse (isHeldElsewhere, L"no other thread does");
    }


    TEST_METHOD (ReleaseThenClaimOnAnotherThreadMovesIt)
    {
        ThreadOwnership  ownership;
        bool             isHeld = false;



        ownership.Release();

        {
            HoldingThread  holder (ownership);

            isHeld = holder.WaitUntilHeld();

            Assert::IsTrue  (isHeld, L"the other thread claimed the released token");
            Assert::IsFalse (ownership.IsHeldByCurrentThread(), L"and the test thread no longer holds it");
        }

        ownership.Claim();

        Assert::IsTrue (ownership.IsHeldByCurrentThread(), L"released again, it can come back to the test thread");
    }


    TEST_METHOD (ClaimingWhileAnotherThreadHoldsItAssertsAndChangesNothing)
    {
        ThreadOwnership  ownership;
        bool             isHeld    = false;
        bool             isKept    = false;



        ownership.Release();

        HoldingThread  holder (ownership);

        isHeld = holder.WaitUntilHeld();
        Assert::IsTrue (isHeld, L"the other thread holds the token");

        {
            ExpectedEhmAssert  expect;

            ownership.Claim();
            expect.RequireCount (1);
        }

        Assert::IsFalse (ownership.IsHeldByCurrentThread(), L"the claim took nothing");

        holder.Finish();
        isKept = holder.WasHeldAtFinish();

        Assert::IsTrue (isKept, L"the holder kept the token");
    }


    TEST_METHOD (ReleasingFromAThreadThatDoesNotHoldItAssertsAndChangesNothing)
    {
        ThreadOwnership  ownership;
        bool             isHeld    = false;
        bool             isKept    = false;



        ownership.Release();

        HoldingThread  holder (ownership);

        isHeld = holder.WaitUntilHeld();
        Assert::IsTrue (isHeld, L"the other thread holds the token");

        {
            ExpectedEhmAssert  expect;

            ownership.Release();
            expect.RequireCount (1);
        }

        holder.Finish();
        isKept = holder.WasHeldAtFinish();

        Assert::IsTrue (isKept, L"the release by a thread that does not hold it left the holder alone");
    }


    TEST_METHOD (ClaimingTwiceAndReleasingTwiceAreHarmless)
    {
        ThreadOwnership  ownership;



        ownership.Claim();
        ownership.Claim();

        Assert::IsTrue (ownership.IsHeldByCurrentThread(), L"a claim by the holder changes nothing");

        ownership.Release();
        ownership.Release();

        Assert::IsFalse (ownership.IsHeldByCurrentThread(), L"a release of an unowned token changes nothing");

        ownership.Claim();

        Assert::IsTrue (ownership.IsHeldByCurrentThread(), L"and the token can still be claimed");
    }


    TEST_METHOD (TheCheckAssertsOffTheHolder)
    {
        ThreadOwnership  ownership;
        bool             isHeld    = false;



        ASSERT_THREAD_OWNERSHIP (ownership);

        ownership.Release();

        HoldingThread  holder (ownership);

        isHeld = holder.WaitUntilHeld();
        Assert::IsTrue (isHeld, L"the other thread holds the token");

        {
            ExpectedEhmAssert  expect;

            ASSERT_THREAD_OWNERSHIP (ownership);
            expect.RequireCount (1);
        }
    }
};
