#include "Pch.h"

#include "Update/ReleaseVersion.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersionTests
//
//  Release tag parsing, ordering, and the minor-line comparison README
//  highlights use.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReleaseVersionTests)
{
public:

    TEST_METHOD (Parse_TagAndBareForms)
    {
        ReleaseVersion  version;
        HRESULT         hr      = S_OK;



        hr = ReleaseVersion::Parse ("v1.30.0", version);
        AssertSucceeded (hr);
        Assert::AreEqual (1,  version.major);
        Assert::AreEqual (30, version.minor);
        Assert::AreEqual (0,  version.patch);

        hr = ReleaseVersion::Parse ("2.4.17", version);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("2.4.17"), version.ToString());
    }



    TEST_METHOD (Parse_RejectsEverythingElse)
    {
        static constexpr const char * kBad[] =
        {
            "", "v", "1", "1.30", "1.30.0.1", "1.30.0-beta", " 1.30.0", "1.30.0 ",
            "1..0", "1.x.0", "vv1.30.0", "Unreleased", "1234567.0.0",
        };

        ReleaseVersion  version;
        HRESULT         hr      = S_OK;



        for (const char * text : kBad)
        {
            hr = ReleaseVersion::Parse (text, version);
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, std::wstring (text, text + strlen (text)).c_str());
            Assert::IsTrue   (version == ReleaseVersion {}, L"a failed parse leaves no partial version");
        }
    }



    TEST_METHOD (Ordering_ComparesNumericallyNotTextually)
    {
        ReleaseVersion  a;
        ReleaseVersion  b;



        Assert::IsTrue (ReleaseVersion::TryParse ("1.9.0", a));
        Assert::IsTrue (ReleaseVersion::TryParse ("1.10.0", b));
        Assert::IsTrue (a < b);

        Assert::IsTrue (ReleaseVersion::TryParse ("1.30.1", a));
        Assert::IsTrue (ReleaseVersion::TryParse ("1.30.0", b));
        Assert::IsTrue (a > b);

        Assert::IsTrue (ReleaseVersion::TryParse ("2.0.0", a));
        Assert::IsTrue (ReleaseVersion::TryParse ("1.99.99", b));
        Assert::IsTrue (a > b);

        Assert::IsTrue (ReleaseVersion::TryParse ("v1.30.0", a));
        Assert::IsTrue (ReleaseVersion::TryParse ("1.30.0", b));
        Assert::IsTrue (a == b);
    }



    TEST_METHOD (CompareMinorLine_IgnoresPatch)
    {
        ReleaseVersion  a { 1, 30, 4 };
        ReleaseVersion  b { 1, 30, 0 };
        ReleaseVersion  c { 1, 31, 0 };
        ReleaseVersion  d { 2, 0, 0 };



        Assert::AreEqual (0,  a.CompareMinorLine (b));
        Assert::AreEqual (-1, a.CompareMinorLine (c));
        Assert::AreEqual (1,  c.CompareMinorLine (a));
        Assert::AreEqual (-1, c.CompareMinorLine (d));
    }
};
