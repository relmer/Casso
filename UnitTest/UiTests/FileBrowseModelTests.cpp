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

    //  A small staged tree: two disk images, a stray text file, and a
    //  subfolder (folders exist in InMemoryFileSystem by containing a file).
    static void StageTree (InMemoryFileSystem & fs)
    {
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\zeta.woz",          "z"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\alpha.woz",         "a"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\notes.txt",         "n"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Saves\\game.woz",   "g"));
    }

    static FileBrowseModel MakeModel (InMemoryFileSystem & fs)
    {
        FileBrowseModel  model;

        model.Bind (&fs);
        model.SetExtensionFilter (L".woz");
        AssertSucceeded (model.SetFolder (L"C:\\Disks"));

        return model;
    }

    TEST_METHOD (Listing_FoldersFirstThenFilteredFilesSorted)
    {
        InMemoryFileSystem  fs;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);
        const auto &     rows  = model.Entries();

        Assert::AreEqual ((size_t) 3, rows.size());   // Saves, alpha, zeta -- txt filtered out
        Assert::AreEqual (std::wstring (L"Saves"),     rows[0].name);
        Assert::IsTrue   (rows[0].isFolder);
        Assert::AreEqual (std::wstring (L"alpha.woz"), rows[1].name);
        Assert::AreEqual (std::wstring (L"zeta.woz"),  rows[2].name);
    }

    TEST_METHOD (Filter_RefiltersCachedListingWithoutRefresh)
    {
        InMemoryFileSystem  fs;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);

        model.SetExtensionFilter (L".txt");

        Assert::AreEqual ((size_t) 2, model.Entries().size());   // Saves + notes.txt
        Assert::AreEqual (std::wstring (L"notes.txt"), model.Entries()[1].name);
    }

    TEST_METHOD (UniqueDefaultName_SkipsExistingNames)
    {
        InMemoryFileSystem  fs;



        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Blank Disk.woz",     "1"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Blank Disk (2).woz", "2"));

        FileBrowseModel  model = MakeModel (fs);

        Assert::AreEqual (std::wstring (L"Blank Disk (3).woz"),
                          model.UniqueDefaultName (L"Blank Disk"));
    }

    TEST_METHOD (UniqueDefaultName_FirstNameWhenFolderClean)
    {
        InMemoryFileSystem  fs;



        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\other.woz", "x"));

        FileBrowseModel  model = MakeModel (fs);

        Assert::AreEqual (std::wstring (L"Blank Disk.woz"),
                          model.UniqueDefaultName (L"Blank Disk"));
    }

    TEST_METHOD (ComposeTargetPath_AppendsExtensionWhenMissing)
    {
        InMemoryFileSystem  fs;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);

        Assert::AreEqual (std::wstring (L"C:\\Disks\\new.woz"),
                          model.ComposeTargetPath (L"new"));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\new.woz"),
                          model.ComposeTargetPath (L"new.woz"));

        // Already carries the extension in different case: nothing appended.
        Assert::AreEqual (std::wstring (L"C:\\Disks\\new.WOZ"),
                          model.ComposeTargetPath (L"new.WOZ"));
    }

    TEST_METHOD (ValidateTarget_OkOnFreshName)
    {
        InMemoryFileSystem  fs;
        int                 drive = -1;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);

        Assert::IsTrue (TargetVerdict::Ok == model.ValidateTarget (L"fresh.woz", drive));
    }

    TEST_METHOD (ValidateTarget_InvalidNames)
    {
        InMemoryFileSystem  fs;
        int                 drive = -1;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);

        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"",          drive));
        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"a<b.woz",   drive));
        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"a\\b.woz",  drive));
        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"ends.",     drive));
        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"CON",       drive));
        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"con.woz",   drive));
    }

    TEST_METHOD (ValidateTarget_ExistsRequiresConfirm)
    {
        InMemoryFileSystem  fs;
        int                 drive = -1;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);

        Assert::IsTrue (TargetVerdict::Exists == model.ValidateTarget (L"alpha.woz", drive));
        Assert::IsTrue (TargetVerdict::Exists == model.ValidateTarget (L"ALPHA.WOZ", drive));
    }

    TEST_METHOD (ValidateTarget_MountedRefusalOutranksExists)
    {
        InMemoryFileSystem  fs;
        int                 drive = -1;



        StageTree (fs);

        FileBrowseModel  model = MakeModel (fs);

        // alpha.woz exists AND is mounted in drive 2: the refusal must win so
        // the overwrite-confirm path is unreachable for a live mount (FR-018).
        model.SetMountedPaths ({ L"c:/disks/ALPHA.woz" }, { 1 });

        Assert::IsTrue (TargetVerdict::MountedInDrive == model.ValidateTarget (L"alpha.woz", drive));
        Assert::AreEqual (1, drive);
    }

    TEST_METHOD (UnboundModel_ValidatesAsInvalidName)
    {
        FileBrowseModel  model;
        int              drive = 0;



        Assert::IsTrue (TargetVerdict::InvalidName == model.ValidateTarget (L"x.woz", drive));
    }
};
