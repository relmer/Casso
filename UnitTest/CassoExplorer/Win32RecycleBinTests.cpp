#include "Pch.h"

#include "Seams/ShellVerbs/Win32RecycleBin.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBinTests
//
//  The flags of a deletion to the Recycle Bin: a user's Delete gets Windows'
//  confirmation, and an undo of a copy deletes with no prompt.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Win32RecycleBinTests)
{
public:

    TEST_METHOD (ADeleteAsksFirst)
    {
        DWORD  flags = Win32RecycleBin::GetRecycleFlags (true);



        Assert::IsTrue ((flags & FOF_NOCONFIRMATION) == 0, L"a user's Delete gets Windows' confirmation");
        Assert::IsTrue ((flags & FOFX_RECYCLEONDELETE) != 0, L"the items go to the Recycle Bin");
    }

    TEST_METHOD (AnUndoOfACopyDeletesWithNoPrompt)
    {
        DWORD  flags = Win32RecycleBin::GetRecycleFlags (false);



        Assert::IsTrue ((flags & FOF_NOCONFIRMATION) != 0, L"an undo of a copy deletes with no prompt");
        Assert::IsTrue ((flags & FOFX_RECYCLEONDELETE) != 0, L"the copies go to the Recycle Bin where there is one");
        Assert::IsTrue ((flags & FOFX_ADDUNDORECORD) != 0, L"the deletion can be undone");
    }
};