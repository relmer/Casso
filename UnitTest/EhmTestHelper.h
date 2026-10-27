#pragma once




namespace UnitTestHelpers
{
    // Redirect EHM assertions to CppUnitTestFramework Assert::Fail()
    // and suppress CRT assert dialogs in Debug builds.
    void SetupForUnitTests ();


    ////////////////////////////////////////////////////////////////////////
    //
    //  ExpectedEhmAssert
    //
    //  Scope guard for a test that deliberately drives an asserting EHM
    //  macro -- typically argument validation, where CBRAEx is right in
    //  production (a caller passing garbage has a bug to fix) but the test
    //  exists precisely to prove the rejection is graceful.
    //
    //  Without this, the two are mutually exclusive: SetupForUnitTests
    //  routes every EHM assert to Assert::Fail, so any path a test
    //  exercises on purpose cannot use an asserting macro at all.
    //
    //  Within the scope an assertion is recorded rather than failing the
    //  test, and the destructor fails if none arrived -- so the guard also
    //  proves the validation is still there and cannot silently rot.
    //
    //      {
    //          ExpectedEhmAssert  expect;
    //          hr = store.MountFromBytes (-1, 0, ...);   // bad slot
    //      }
    //      Assert::IsTrue (FAILED (hr));
    //
    //  Only Debug builds can be held to that, because Release compiles
    //  EHM_BREAKPOINT down to ((void) 0) -- no handler runs, so nothing is
    //  counted and "none arrived" carries no information. The scope still does
    //  its job there: the guarded call runs and the test's own assertions on
    //  the returned HRESULT still hold.
    //
    ////////////////////////////////////////////////////////////////////////

    class ExpectedEhmAssert
    {
    public:
        ExpectedEhmAssert  ();
        ~ExpectedEhmAssert ();

        ExpectedEhmAssert  (const ExpectedEhmAssert &) = delete;
        ExpectedEhmAssert & operator= (const ExpectedEhmAssert &) = delete;

        // Live count, readable while the scope is still open. Always 0 where
        // asserts are compiled out, so compare it with RequireCount rather
        // than asserting on it directly.
        int   Count () const;

        // Asserts the guard fired exactly `expected` times -- "all six
        // rejections asserted", not merely "at least one did". Does nothing
        // where asserts are compiled out, which keeps that knowledge here
        // instead of in every test that counts.
        void  RequireCount (int expected) const;

    private:
        int   m_count = 0;
    };


    ////////////////////////////////////////////////////////////////////////
    //
    //  WorkerThreadEhmCapture
    //
    //  An EHM assertion on a thread the test started cannot fail the test
    //  there. Assert::Fail raises SEH exception 0xE3530001, which nothing on
    //  that thread catches, so the test host ends and no test is reported
    //  as the failure.
    //
    //  While one of these is open on a thread, an assertion on that thread is
    //  appended to the capture instead of failing, and the thread goes on as
    //  a Release build would. The test thread reads the text with
    //  TakeFailures and fails the test with it. Open it on the worker thread
    //  itself; scopes on one thread do not nest.
    //
    ////////////////////////////////////////////////////////////////////////

    class WorkerThreadEhmCapture
    {
    public:
        WorkerThreadEhmCapture  ();
        ~WorkerThreadEhmCapture ();

        WorkerThreadEhmCapture  (const WorkerThreadEhmCapture &) = delete;
        WorkerThreadEhmCapture & operator= (const WorkerThreadEhmCapture &) = delete;

        // Every assertion since the last call, one per line, and empties the
        // capture. Callable from any thread.
        std::wstring  TakeFailures ();

        // Called by the EHM breakpoint handler on the capturing thread.
        void          Add          (const wchar_t * message);

    private:
        std::mutex    m_lock;
        std::wstring  m_failures;
    };
}
