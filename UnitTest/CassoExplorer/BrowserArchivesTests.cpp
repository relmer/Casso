#include "Pch.h"

#include "../EhmTestHelper.h"
#include "BrowserPartsFixture.h"
#include "FakeRecycleBin.h"
#include "CassoExplorer/Browser/BrowserArchives.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Browser/IBrowserSelectionSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchivesTests
//
//  Which paths are inside an archive the shell lists, and the archive Extract
//  all applies to.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserArchivesTests)
{
public:

    //  The tab's location for the selection, and none of the rest.
    class FakeSelectionSource : public IBrowserSelectionSource
    {
    public:
        explicit FakeSelectionSource (BrowserModel & model) : m_model (model) {}

        Location               GetLocation                  () const override  { return m_model.GetActiveTab().location; }
        bool                   IsHomeLocation               () const override  { return false; }
        bool                   IsImageLocation              () const override  { return false; }
        bool                   IsVolumeLocation             () const override  { return false; }
        const VolumeListing  & GetListing                   () const override  { return m_listing; }
        bool                   TryGetSelectedCassoLinkPaths (std::vector<std::wstring> &) const override  { return false; }
        void                   UpdatePreviewAndStatus       () override        {}

    private:
        BrowserModel  & m_model;
        VolumeListing   m_listing;
    };


    struct Rig
    {
        BrowserPartsFixture  fixture;
        FakeRecycleBin       shellVerbs;
        FakeSelectionSource  source    { fixture.model };
        BrowserSelection     selection { fixture.model, fixture.list.rows, fixture.list.selectedRows, source };
        BrowserArchives      archives  { fixture.parts, selection };

        void SetLocation (const Location & location)
        {
            fixture.model.GetActiveTab().location = location;
        }

        void SelectRows (const std::vector<CatalogRow> & rows, const std::vector<int> & selected)
        {
            fixture.list.rows         = rows;
            fixture.list.selectedRows = selected;
        }
    };


    static CatalogRow MakeRow (const wchar_t * name, bool isArchive)
    {
        CatalogRow  row;

        row.name      = name;
        row.isArchive = isArchive;

        return row;
    }


    TEST_METHOD (IsLocalArchivePath_NeedsTheShell)
    {
        Rig  rig;

        Assert::IsFalse (rig.archives.IsLocalArchivePath (L"C:\\Disks\\notes.zip"), L"With no shell, no path is one the shell lists");

        rig.fixture.sources.shellVerbs = &rig.shellVerbs;
        Assert::IsTrue  (rig.archives.IsLocalArchivePath (L"C:\\Disks\\notes.zip"));
    }


    TEST_METHOD (IsLocalArchivePath_NeedsADriveLetterPathBelowTheRoot)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shellVerbs;

        Assert::IsFalse (rig.archives.IsLocalArchivePath (L"C:\\"));
        Assert::IsFalse (rig.archives.IsLocalArchivePath (L"\\\\server\\share\\notes.zip"), L"A network path has no drive letter");
        Assert::IsFalse (rig.archives.IsLocalArchivePath (L"notes.zip"));
    }


    TEST_METHOD (IsLocalArchivePath_IsNotAFolderOnTheDisk)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shellVerbs;
        rig.fixture.SeedHostFile (L"C:\\Disks\\games\\readme.txt", "hello");

        Assert::IsFalse (rig.archives.IsLocalArchivePath (L"C:\\Disks\\games"), L"The file system lists a folder on the disk");
        Assert::IsTrue  (rig.archives.IsLocalArchivePath (L"C:\\Disks\\games\\notes.zip"));
    }


    TEST_METHOD (GetExtractTarget_IsTheArchiveSelectedAloneInAHostFolder)
    {
        Rig  rig;

        rig.SetLocation (Location::MakeHostFolder (L"C:\\Disks"));
        rig.SelectRows ({ MakeRow (L"notes.zip", true), MakeRow (L"readme.txt", false) }, { 0 });

        Assert::AreEqual (std::wstring (L"C:\\Disks\\notes.zip"), rig.archives.GetExtractTarget());
    }


    TEST_METHOD (GetExtractTarget_IsEmptyForAPlainFileOrSeveralRows)
    {
        Rig  rig;

        rig.SetLocation (Location::MakeHostFolder (L"C:\\Disks"));
        rig.SelectRows ({ MakeRow (L"notes.zip", true), MakeRow (L"readme.txt", false) }, { 1 });
        Assert::IsTrue (rig.archives.GetExtractTarget().empty(), L"A plain file has no archive");

        rig.SelectRows ({ MakeRow (L"notes.zip", true), MakeRow (L"readme.txt", false) }, { 0, 1 });
        Assert::IsTrue (rig.archives.GetExtractTarget().empty(), L"nor does a selection of several");
    }


    TEST_METHOD (GetExtractTarget_InsideAnArchive_IsTheFirstPartOfThePathThatIsAFile)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shellVerbs;
        rig.fixture.SeedHostFile (L"C:\\Disks\\notes.zip", "zip");
        rig.SetLocation (Location::MakeShellFolder (L"C:\\Disks\\notes.zip\\inner", L"inner"));

        Assert::AreEqual (std::wstring (L"C:\\Disks\\notes.zip"), rig.archives.GetExtractTarget());
    }


    TEST_METHOD (GetExtractTarget_InAShellFolderWithNoArchiveInItsPath_IsEmpty)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shellVerbs;
        rig.SetLocation (Location::MakeShellFolder (L"C:\\Disks\\missing.zip\\inner", L"inner"));

        Assert::IsTrue (rig.archives.GetExtractTarget().empty());
    }


    TEST_METHOD (GetExtractTarget_ForAnotherKindOfLocation_IsEmpty)
    {
        Rig  rig;

        rig.SetLocation (Location::MakeDiskImage (L"C:\\Disks\\a.dsk"));

        Assert::IsTrue (rig.archives.GetExtractTarget().empty());
    }
};
