#include "Pch.h"

#include "Devices/Disk/Store/DiskFileCommit.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommitTests
//
//  The temporary a commit writes through, from the image path alone.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskFileCommitTests)
{
public:

    static constexpr const char *  kImagePath = "C:\\work\\Loader.dsk";


    TEST_METHOD (TwoWritersDeriveDifferentTemporariesFromOneImage)
    {
        std::string  mine   = DiskFileCommit::GetCommitTemporaryPath (kImagePath, 0);
        std::string  again  = DiskFileCommit::GetCommitTemporaryPath (kImagePath, 0);
        std::string  second = DiskFileCommit::GetCommitTemporaryPath (kImagePath, 1);



        //  Within one process the name is stable, which is what lets a caller
        //  ask where its own commit went.
        Assert::AreEqual (mine, again);

        //  And stepping the attempt gives a different one, which is what walks
        //  past a temporary an earlier writer abandoned.
        Assert::IsTrue (mine != second);

        //  THE NAME IS NOT DERIVED FROM THE IMAGE PATH ALONE, which is the
        //  whole defect: it used to be `<image>.casso-tmp` in every process, so
        //  two emulators holding one image wrote into each other's temporary
        //  and one renamed the other's bytes over the target as its own. The
        //  invocation tag is what makes two processes disagree.
        Assert::IsTrue (mine != std::string (kImagePath) + ".casso-tmp",
                        L"a name a second process would also have chosen");
        Assert::IsTrue (mine.find (kImagePath) == 0,
                        L"and it still sits beside the image, so a leftover is "
                        L"findable by someone reading the folder");
    }

    TEST_METHOD (TemporaryPath_SitsInTheImageFolder)
    {
        fs::path  temp = DiskFileCommit::GetCommitTemporaryPath (kImagePath, 3);



        //  A rename across volumes is a copy, so the temporary has to be in the
        //  same folder as the target for the swap to be one operation.
        Assert::AreEqual (fs::path (kImagePath).parent_path().wstring(), temp.parent_path().wstring());
        Assert::IsTrue (temp.wstring() != fs::path (kImagePath).wstring());
    }
};
