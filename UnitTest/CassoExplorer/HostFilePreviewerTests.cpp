#include "Pch.h"

#include "../EhmTestHelper.h"
#include "BrowserPartsFixture.h"
#include "FakeRecycleBin.h"
#include "CassoExplorer/Browser/Preview/HostFilePreviewer.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/AppleSingleCodec.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewerTests
//
//  The preview of a file in a host folder: an AppleSingle file, a file with
//  its Apple type in its name, assembler source and exported Apple text, a
//  file Windows previews, and any other file as its bytes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostFilePreviewerTests)
{
public:

    static constexpr const wchar_t *  kFolder = L"C:\\Disks";


    struct Rig
    {
        BrowserPartsFixture  fixture;
        FakeRecycleBin       shell;
        HostFilePreviewer    previewer { fixture.parts, fixture.previewParts };

        //  The file written into the folder, then previewed as a row of it.
        const PreviewContent & Preview (const wchar_t * name, const std::string & content)
        {
            CatalogRow  row;

            fixture.SeedHostFile ((std::wstring (kFolder) + L"\\" + name).c_str(), content);

            row.name      = name;
            row.sizeBytes = content.size();

            return Preview (row);
        }

        const PreviewContent & Preview (const CatalogRow & row)
        {
            fixture.preview      = PreviewContent();
            fixture.preview.kind = PreviewContent::Kind::Text;

            previewer.PreviewHostFile (kFolder, row);

            return fixture.preview;
        }
    };


    TEST_METHOD (AnAppleSingleFile_PreviewsAsTheFileItHolds)
    {
        Rig                rig;
        AppleSingleFile    file;
        std::vector<Byte>  bytes;

        file.data          = { 0x60 };
        file.realName      = "GAME";
        file.hasProDosInfo = true;
        file.fileType      = 0x06;
        file.auxType       = 0x6000;
        AppleSingleCodec::Encode (file, bytes);

        const PreviewContent &  preview = rig.Preview (L"GAME.as", std::string (bytes.begin(), bytes.end()));

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Hex, L"a BIN file previews as it does in a disk");
        Assert::IsTrue   (preview.executable, L"its type marks it as code");
        Assert::IsTrue   (preview.hostPath.empty(), L"decoded from the file it holds, not read as the container's bytes");
        Assert::IsTrue   (preview.detailsAbove && !preview.details.empty(), L"the AppleSingle file's details head the preview");
    }


    TEST_METHOD (AnAppleSingleFileWithNoType_PreviewsAsItsDetails)
    {
        Rig                rig;
        AppleSingleFile    file;
        std::vector<Byte>  bytes;
        bool               hasName = false;
        bool               hasDate = false;

        file.data       = { 0x60 };
        file.realName   = "GAME";
        file.modifyDate = 86400;
        AppleSingleCodec::Encode (file, bytes);

        const PreviewContent &  preview = rig.Preview (L"GAME.as", std::string (bytes.begin(), bytes.end()));

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Details);

        for (const auto & [label, value] : preview.details)
        {
            hasName |= label == L"Name"     && value == L"GAME";
            hasDate |= label == L"Modified" && !value.empty() && value != L"Not valid";
        }

        Assert::IsTrue (hasName, L"The real name");
        Assert::IsTrue (hasDate, L"The date, counted from 2000");
    }


    TEST_METHOD (AnyOtherFile_PreviewsAsItsBytesReadFromTheFile)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.Preview (L"readme.txt", "hello");

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Hex, L"ASCII text draws in the hex view");
        Assert::AreEqual (std::wstring (L"C:\\Disks\\readme.txt"), preview.hostPath);
        Assert::IsTrue   (preview.lines.empty());
    }


    TEST_METHOD (Folder_PreviewsNothing)
    {
        Rig         rig;
        CatalogRow  row;

        row.name        = L"games";
        row.isDirectory = true;

        Assert::IsTrue (rig.Preview (row).hostPath.empty());
        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Text, L"The preview is as it was");
    }


    //  An assembler's source previews as text with its line numbers, in
    //  whatever character set it is; UTF-8 text of any kind previews as text,
    //  which the hex view cannot draw.
    TEST_METHOD (SourceFile_PreviewsAsTextWithItsLineNumbers)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.Preview (L"demo.a65", "\xEF\xBB\xBF; caf\xC3\xA9\tx\r\n\tLDA #1\r\n");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Text);
        Assert::IsTrue (preview.source);
        Assert::IsTrue (preview.lines == std::vector<std::wstring> ({ L"; caf\u00E9  x", L"        LDA #1" }),
                        L"The mark dropped, the letters decoded and the tabs expanded");
    }


    TEST_METHOD (Utf8File_PreviewsAsText)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.Preview (L"notes.txt", "a \xE2\x80\x94 b\n");

        Assert::IsTrue  (preview.kind == PreviewContent::Kind::Text);
        Assert::IsFalse (preview.source);
        Assert::IsTrue  (preview.lines == std::vector<std::wstring> ({ L"a \u2014 b" }));
    }


    TEST_METHOD (Utf16File_PreviewsAsText_InBothByteOrders)
    {
        Rig  rig;

        Assert::IsTrue (rig.Preview (L"wide.txt", std::string ("\xFF\xFEh\0i\0\r\0\n\0", 10)).lines == std::vector<std::wstring> ({ L"hi" }),
                        L"UTF-16 with its mark, little-endian");
        Assert::IsTrue (rig.Preview (L"big.txt", std::string ("\xFE\xFF\0h\0i", 6)).lines == std::vector<std::wstring> ({ L"hi" }),
                        L"and big-endian");
    }


    //  A listing or text Casso Explorer exported previews as lines of text,
    //  by its name, where other ASCII text opens in the hex view.
    TEST_METHOD (ExportedAppleText_PreviewsAsLines)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.Preview (L"HELLO.Applesoft BASIC.txt", "10  PRINT \"HELLO\"\r\n");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Text, L"ASCII, read as lines");
        Assert::IsTrue (preview.lines == std::vector<std::wstring> ({ L"10  PRINT \"HELLO\"" }));
    }


    TEST_METHOD (FullSendManifest_PreviewsAsWhatItIs)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.Preview (TextEncoding::NarrowToWide (FullSendManifest::kFileName).c_str(), "{}");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Error);
        Assert::IsTrue (preview.message.starts_with (L"Casso's record of the disks a Full Send received"));
    }


    TEST_METHOD (NameWithAProDosType_PreviewsAsItsType)
    {
        Rig  rig;

        const PreviewContent &  preview = rig.Preview (L"NOTES#040000", "hello\r");

        Assert::IsTrue  (preview.kind != PreviewContent::Kind::Hex, L"A text file previews as the text it is");
        Assert::IsTrue  (preview.hostPath.empty());
        Assert::IsFalse (preview.viewChoices.empty(), L"with the other views of its bytes");
    }


    TEST_METHOD (FileOverTheTextSizeLimit_StaysTheHexView)
    {
        Rig         rig;
        CatalogRow  row;

        rig.fixture.SeedHostFile (L"C:\\Disks\\big.a65", "        LDA #1\r\n");

        row.name      = L"big.a65";
        row.sizeBytes = HostFilePreviewer::kTextPreviewMaxBytes + 1;

        const PreviewContent &  preview = rig.Preview (row);

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Hex);
        Assert::IsTrue (preview.lines.empty(), L"A file this large is not read whole");
    }


    TEST_METHOD (ShellPreview_IsUsedOnlyForAFileThatIsNotSourceOrAnExport)
    {
        Rig  rig;

        rig.fixture.sources.shellVerbs = &rig.shell;
        rig.shell.previewExtension     = L".pdf";

        Assert::IsTrue (rig.Preview (L"manual.pdf", "%PDF-1.4").kind == PreviewContent::Kind::Shell, L"A file Windows previews");

        rig.shell.previewExtension = L".a65";
        Assert::IsTrue (rig.Preview (L"demo.a65", "        LDA #1\r\n").kind == PreviewContent::Kind::Text, L"Assembler source is Casso's to preview");

        rig.shell.previewExtension = L".txt";
        Assert::IsTrue (rig.Preview (L"HELLO.Applesoft BASIC.txt", "10  PRINT\r\n").kind == PreviewContent::Kind::Text, L"as is an exported Apple file");
        Assert::IsTrue (rig.Preview (L"readme.txt", "hello").kind == PreviewContent::Kind::Shell, L"but other text takes the Windows preview");
    }
};
