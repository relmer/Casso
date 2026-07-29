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
    ////////////////////////////////////////////////////////////////////////

    class ExpectedEhmAssert
    {
    public:
        ExpectedEhmAssert  ();
        ~ExpectedEhmAssert ();

        ExpectedEhmAssert  (const ExpectedEhmAssert &) = delete;
        ExpectedEhmAssert & operator= (const ExpectedEhmAssert &) = delete;

        // Live count, readable while the scope is still open -- so a test can
        // assert "all six rejections asserted", not merely "at least one did".
        int  Count () const;

    private:
        int   m_count = 0;
    };
}
