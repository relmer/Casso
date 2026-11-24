#include "Pch.h"

#include "../EhmTestHelper.h"
#include "BrowserPartsFixture.h"
#include "FakeRecycleBin.h"
#include "CassoExplorer/Browser/BrowserArchives.h"
#include "CassoExplorer/Browser/BrowserSelection.h"
#include "CassoExplorer/Browser/IBrowserSelectionSource.h"
#include "CassoExplorer/Model/ArchiveProbe.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchivesTests
//
//  Which paths are inside an archive the shell lists, why an archive with
//  nothing in it is empty, and the archive Extract all applies to.
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

    //  One file listed with a size of the test's choosing, whatever its content is.
    class ReportedSizeFileSystem : public InMemoryFileSystem
    {
    public:
        HRESULT EnumerateAllEntries (const std::wstring & directory, std::vector<FileSystemEntry> & outEntries) override
        {
            HRESULT  hr = InMemoryFileSystem::EnumerateAllEntries (directory, outEntries);

            for (FileSystemEntry & entry : outEntries)
            {
                if (entry.name == reportedName)
                {
                    entry.sizeBytes = reportedSize;
                }
            }

            return hr;
        }

        std::wstring  reportedName;
        uint64_t      reportedSize = 0;
    };


    struct SizedRig
    {
        BrowserPartsFixture     fixture;
        ReportedSizeFileSystem  fs;
        BrowserParts            parts     { fs, fixture.io, fixture.imageClasses, fixture.catalog, fixture.tree, fixture.hostFolders, fixture.model,
                                            fixture.operations, fixture.shellListings, fixture.list, fixture.sources };
        FakeSelectionSource     source    { fixture.model };
        BrowserSelection        selection { fixture.model, fixture.list.rows, fixture.list.selectedRows, source };
        BrowserArchives         archives  { parts, selection };

        //  A zip with no entries: its end record alone.
        void SeedEmptyZip (const wchar_t * path)
        {
            std::string  zip ("PK\x05\x06", 4);

            zip.append (18, '\0');
            AssertSucceeded (fs.WriteAllText (path, zip));
        }
    };


    TEST_METHOD (DescribeEmptyArchive_ForAZipWithNothingInIt_GivesNoReason)
    {
        SizedRig  rig;

        rig.SeedEmptyZip (L"C:\\Disks\\empty.zip");

        Assert::IsTrue (rig.archives.DescribeEmptyArchive (L"C:\\Disks\\empty.zip").empty());
    }


    TEST_METHOD (DescribeEmptyArchive_ForAFileThatIsNotAZip_GivesTheNotZipReason)
    {
        SizedRig  rig;

        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Disks\\notes.zip", "not a zip at all"));

        Assert::AreEqual (ArchiveProbe::GetReason (L"notes.zip", ArchiveProbe::Verdict::NotZip),
                          rig.archives.DescribeEmptyArchive (L"C:\\Disks\\notes.zip"));
    }


    TEST_METHOD (DescribeEmptyArchive_ForAFileThatCannotBeRead_GivesTheNotZipReason)
    {
        SizedRig  rig;

        Assert::AreEqual (ArchiveProbe::GetReason (L"gone.zip", ArchiveProbe::Verdict::NotZip),
                          rig.archives.DescribeEmptyArchive (L"C:\\Disks\\gone.zip"));
    }


    //  The size is the file system's, which is how a file too big to read whole
    //  is distinguished from one the file system lists with a size to read.
    TEST_METHOD (DescribeEmptyArchive_PastTheSizeLimit_IsNotRead)
    {
        SizedRig  rig;

        rig.SeedEmptyZip (L"C:\\Disks\\huge.zip");
        rig.fs.reportedName = L"huge.zip";
        rig.fs.reportedSize = BrowserArchives::kArchiveProbeMaxBytes + 1;

        Assert::AreEqual (ArchiveProbe::GetReason (L"huge.zip", ArchiveProbe::Verdict::NotZip),
                          rig.archives.DescribeEmptyArchive (L"C:\\Disks\\huge.zip"),
                          L"A zip with nothing in it would have given no reason had it been read");
    }


    TEST_METHOD (DescribeEmptyArchive_AtTheSizeLimit_IsRead)
    {
        SizedRig  rig;

        rig.SeedEmptyZip (L"C:\\Disks\\large.zip");
        rig.fs.reportedName = L"large.zip";
        rig.fs.reportedSize = BrowserArchives::kArchiveProbeMaxBytes;

        Assert::IsTrue (rig.archives.DescribeEmptyArchive (L"C:\\Disks\\large.zip").empty());
    }
};