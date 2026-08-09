#include "Pch.h"
#include "../EhmTestHelper.h"
#include "InMemoryFileSystem.h"
#include "Ui/FileBrowseModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileBrowseModelTests
//
//  Spec 017: the create-dialog's navigation / validation engine against the
//  in-memory IFileSystem — listing order, extension filtering, unique default
//  names, and the ValidateTarget precedence chain (mounted-path refusal
//  outranks Exists; FR-006/007/018). Populated across T008 (v1) and T026
//  (navigation).
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (FileBrowseModelTests)
{
public:

    TEST_METHOD (UnboundModel_ValidatesAsInvalidName)
    {
        FileBrowseModel  model;
        int              drive = 0;



        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"x.woz", drive));
    }
};
