#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/PreservedCopy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PreservedCopyTests
//
//  What a version of a disk is called when it is not the one that stays
//  mounted.
//
//  THE NAME IS THE WHOLE PRODUCT HERE. A preserved copy the user cannot find,
//  cannot tell from an ordinary disk, or that quietly replaced the previous one
//  is a copy that was not preserved -- so the naming is asserted directly
//  rather than through the paths that happen to call it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PreservedCopyTests)
{
public:

    static constexpr const char *  kStamp = "20260831-004512";



    TEST_METHOD (ACopySitsBesideTheOriginalAndCarriesItsTimestamp)
    {
        std::string  made = PreservedCopy::MakePath ("C:\\work\\Loader.dsk", kStamp, 0);



        //  Beside the original, so the pairing is obvious in a folder listing
        //  and the copy is where the user is already looking.
        Assert::IsTrue (made.find ("C:\\work\\") == 0, L"beside the original");

        //  Carrying the moment it was made, which is what tells two of them
        //  apart and what says which is which.
        Assert::IsTrue (made.find (kStamp) != std::string::npos, L"timestamped");

        //  Still a mountable image: a disk the user cannot put back in a drive
        //  is not a preserved version of anything.
        Assert::IsTrue (made.size() > 4 && made.substr (made.size() - 4) == ".dsk");

        //  And recognizably the same disk.
        Assert::IsTrue (made.find ("Loader") != std::string::npos);
    }



    TEST_METHOD (EveryCopyIsNumberedIncludingTheFirst)
    {
        std::string  first  = PreservedCopy::MakePath ("Work.dsk", kStamp, 0);
        std::string  second = PreservedCopy::MakePath ("Work.dsk", kStamp, 1);
        std::string  tenth  = PreservedCopy::MakePath ("Work.dsk", kStamp, 9);



        Assert::AreEqual (std::string ("Work.20260831-004512-01.dsk"), first);
        Assert::AreEqual (std::string ("Work.20260831-004512-02.dsk"), second);
        Assert::AreEqual (std::string ("Work.20260831-004512-10.dsk"), tenth);
    }



    TEST_METHOD (CopiesMadeInOneSecondSortInTheOrderTheyHappened)
    {
        std::vector<std::string>  made;
        int                       attempt = 0;



        for (attempt = 0; attempt < 12; attempt++)
        {
            made.push_back (PreservedCopy::MakePath ("Work.dsk", kStamp, attempt));
        }

        //  THE PROMISE IS THAT REPEATED CONFLICTS ACCUMULATE READABLY. Leaving
        //  the first copy unnumbered would sort it AFTER its own successors --
        //  the comparison reaches `.dsk` where they have `-02` -- and an
        //  unpadded counter would put `-10` before `-2`.
        for (size_t i = 1; i < made.size(); i++)
        {
            Assert::IsTrue (made[i - 1] < made[i],
                            (L"out of order at " + std::to_wstring (i)).c_str());
        }
    }



    TEST_METHOD (ACopyNeverTakesTheNameOfAnother)
    {
        std::vector<std::string>  made;
        int                       attempt = 0;



        for (attempt = 0; attempt < PreservedCopy::kMaxAttempts; attempt++)
        {
            made.push_back (PreservedCopy::MakePath ("Work.dsk", kStamp, attempt));
        }

        std::sort (made.begin(), made.end());

        Assert::IsTrue (std::adjacent_find (made.begin(), made.end()) == made.end(),
                        L"two attempts produced one name");
    }



    TEST_METHOD (AnImageWithNoExtensionStillProducesAMountableName)
    {
        std::string  made = PreservedCopy::MakePath ("C:\\work\\Loader", kStamp, 0);



        //  A container Casso can read, rather than a name nothing will open.
        Assert::IsTrue (made.substr (made.size() - 4) == ".dsk");
    }



    TEST_METHOD (AStampIsSortableAsText)
    {
        //  Two moments a second apart, which is the resolution the counter
        //  exists to disambiguate below.
        std::string  earlier = PreservedCopy::MakeStamp (1756500000);
        std::string  later   = PreservedCopy::MakeStamp (1756500001);



        Assert::IsTrue (earlier < later,
                        L"a directory listing must read in the order things happened");

        //  `YYYYMMDD-HHMMSS`, which is what makes that true without anything
        //  having to parse it.
        Assert::AreEqual ((size_t) 15, earlier.size());
        Assert::AreEqual ('-', earlier[8]);
    }
};
