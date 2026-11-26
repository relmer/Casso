#include "Pch.h"

#include "../EhmTestHelper.h"
#include "BrowserPartsFixture.h"
#include "FakeRecycleBin.h"
#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/Browser/Preview/CassoLinkPreviewer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPreviewerTests
//
//  The preview of the items CassoLink adds under a connection, and that a row
//  that is not a linked Apple's drive is not previewed as one. A drive and a
//  file read over the link depend on a connection, which has no fake.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkPreviewerTests)
{
public:

    struct Rig
    {
        BrowserPartsFixture  fixture;
        FakeRecycleBin       shell;
        CassoLinkPreviewer   previewer { fixture.parts, fixture.previewParts };

        //  The preview of a CassoLink item, made from a hex preview.
        const PreviewContent & PreviewItem (CatalogRow::CassoLinkItem item, const std::wstring & hostPath = std::wstring())
        {
            CatalogRow  row;

            row.cassoLinkItem = item;
            row.hostPath      = hostPath;

            fixture.preview      = PreviewContent();
            fixture.preview.kind = PreviewContent::Kind::Hex;

            previewer.PreviewCassoLinkItem (row);

            return fixture.preview;
        }
    };


    //  The text before its first line break.
    static std::wstring GetFirstLine (const wchar_t * text)
    {
        std::wstring  all   (text);
        size_t        until = all.find (L"\r\n");

        return all.substr (0, until);
    }


    TEST_METHOD (RowThatIsNotALinkedDrive_IsNotPreviewedAsOne)
    {
        Rig         rig;
        CatalogRow  row;

        row.hostPath = L"C:\\Disks\\a.dsk";

        Assert::IsFalse (rig.previewer.PreviewAppleDrive (row));
        Assert::IsTrue  (rig.fixture.preview.kind == PreviewContent().kind, L"The preview is as it was");
    }


    TEST_METHOD (RowWithNoConnectionsToLinkedDrives_IsNotPreviewedAsADrive)
    {
        Rig         rig;
        CatalogRow  row;

        row.hostPath = L"link:serial:COM3:1:2";

        Assert::IsNull  (rig.fixture.sources.links);
        Assert::IsFalse (rig.previewer.PreviewAppleDrive (row));
    }


    TEST_METHOD (HelpItem_PreviewsAsLinesOfText)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::Help);

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Text);
        Assert::IsFalse  (preview.lines.empty());
        Assert::AreEqual (GetFirstLine (BrowserLoader::GetHelpText (CatalogRow::CassoLinkItem::Help)), preview.lines[0]);
    }


    TEST_METHOD (AdtProHelpItem_PreviewsTheAdtProText)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::AdtProHelp);

        Assert::IsFalse  (preview.lines.empty());
        Assert::AreEqual (GetFirstLine (BrowserLoader::GetHelpText (CatalogRow::CassoLinkItem::AdtProHelp)), preview.lines[0]);
    }


    TEST_METHOD (AddMachineItem_PreviewsHowToAddAConnection)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::AddMachine);

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Text);
        Assert::AreEqual (std::wstring (L"Open it to add a connection to an Apple: a serial cable, a Wi-Fi modem, or a Casso on TCP."), preview.lines[0]);
    }


    TEST_METHOD (BootDiskItem_PreviewsWhatBootingItDoes)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::BootDisk);

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Text);
        Assert::AreEqual (std::wstring (L"The CassoLink boot disk."), preview.lines[0]);
    }


    //  The help is written out where the shell's Markdown preview can read it.
    TEST_METHOD (HelpItem_WithAShellPreview_PreviewsThroughTheShell)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shell;
        rig.shell.previewExtension     = L".md";

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::Help, L"C:\\Cache\\help.md");

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Shell);
        Assert::AreEqual (std::wstring (L"C:\\Cache\\help.md"), preview.hostPath);
        Assert::AreEqual ((size_t) 1, rig.fixture.io.writtenPaths.size(), L"The help was written for the shell to read");
        Assert::AreEqual (std::string ("C:\\Cache\\help.md"), rig.fixture.io.writtenPaths[0]);
        Assert::IsTrue   (rig.fixture.io.files.contains ("C:\\Cache\\help.md"));
    }


    TEST_METHOD (HelpItem_WhenTheShellHasNoPreview_PreviewsAsText)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shell;
        rig.shell.previewExtension     = L".pdf";

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::Help, L"C:\\Cache\\help.md");

        Assert::IsTrue  (preview.kind == PreviewContent::Kind::Text);
        Assert::IsFalse (preview.lines.empty());
    }


    TEST_METHOD (HelpItem_WithNoShell_IsNotWritten)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.PreviewItem (CatalogRow::CassoLinkItem::Help, L"C:\\Cache\\help.md");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Text);
        Assert::IsTrue (rig.fixture.io.writtenPaths.empty());
    }
};
