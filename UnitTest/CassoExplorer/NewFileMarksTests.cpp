#include "Pch.h"

#include "CassoExplorer/Model/NewFileMarks.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NewFileMarksTests
//
//  The files marked new in a receiving folder's list.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NewFileMarksTests)
{
public:

    //  On the first listing, the files written after the folder was last
    //  listed are new, and the others are not.
    TEST_METHOD (FirstListing_MarksTheFilesWrittenSinceTheLastListing)
    {
        NewFileMarks  marks;



        marks.AddNew (L"C:\\Box", 100, { { L"old.woz", 90 }, { L"New.woz", 110 } });

        Assert::IsTrue  (marks.IsNew (L"C:\\Box", L"new.woz"));
        Assert::IsFalse (marks.IsNew (L"C:\\Box", L"old.woz"));
        Assert::IsFalse (marks.IsNew (L"C:\\Other", L"new.woz"));
    }


    //  A file that arrives while the folder is listed is added, and the
    //  files already marked stay marked.
    TEST_METHOD (ArrivalWhileListed_AddsTheFile)
    {
        NewFileMarks  marks;



        marks.AddNew (L"C:\\Box", 100, { { L"a.woz", 110 } });
        marks.AddNew (L"c:\\box", 120, { { L"a.woz", 110 }, { L"b.woz", 130 } });

        Assert::IsTrue (marks.IsNew (L"C:\\Box", L"a.woz"));
        Assert::IsTrue (marks.IsNew (L"C:\\Box", L"b.woz"));
    }


    //  Listing another location, or another receiving folder, clears the set.
    TEST_METHOD (NavigatingAway_ClearsTheMarks)
    {
        NewFileMarks  marks;



        marks.AddNew (L"C:\\Box", 100, { { L"a.woz", 110 } });
        marks.Clear();

        Assert::IsFalse (marks.IsNew (L"C:\\Box", L"a.woz"));

        marks.AddNew (L"C:\\Box", 100, { { L"a.woz", 110 } });
        marks.AddNew (L"C:\\Other", 100, {});

        Assert::IsFalse (marks.IsNew (L"C:\\Box", L"a.woz"));
    }
};
