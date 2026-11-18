#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/PreviewSaveChoices.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSaveChoicesTests
//
//  Save as's file types for each kind of view, the type read back from the
//  name picked, and the text of each kind of view.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PreviewSaveChoicesTests)
{
public:

    using Kind = PreviewSaveChoices::Kind;

    static std::vector<Kind> GetKinds (const std::vector<PreviewSaveChoices::Choice> & choices)
    {
        std::vector<Kind>  kinds;



        for (const PreviewSaveChoices::Choice & choice : choices)
        {
            kinds.push_back (choice.kind);
        }

        return kinds;
    }


    TEST_METHOD (Picture_PngFirstThenGetsConversionAndTheBytes)
    {
        PreviewContent                           preview;
        std::vector<PreviewSaveChoices::Choice>  choices;



        preview.kind = PreviewContent::Kind::Picture;
        preview.bgra.assign (4, 0);

        PreviewSaveChoices::Build (preview, L"PICTURE.png", choices);

        Assert::IsTrue (GetKinds (choices) == std::vector<Kind> { Kind::ViewPicture, Kind::Original, Kind::AppleSingle },
                        L"Get's PNG is the view's PNG, listed once");
        Assert::AreEqual (std::wstring (L"png"), choices[0].extension);
    }


    TEST_METHOD (Document_TextAsDisplayedThenGetsRtf)
    {
        PreviewContent                           preview;
        std::vector<PreviewSaveChoices::Choice>  choices;



        preview.kind = PreviewContent::Kind::Document;

        PreviewSaveChoices::Build (preview, L"LETTER.rtf", choices);

        Assert::IsTrue (GetKinds (choices) == std::vector<Kind> { Kind::ViewText, Kind::Converted, Kind::Original, Kind::AppleSingle });
        Assert::AreEqual (std::wstring (L"rtf"), choices[1].extension);
    }


    TEST_METHOD (Match_ReadsTheTypeFromTheExtension)
    {
        PreviewContent                           preview;
        std::vector<PreviewSaveChoices::Choice>  choices;



        preview.kind = PreviewContent::Kind::Document;
        PreviewSaveChoices::Build (preview, L"LETTER.RTF", choices);

        Assert::IsTrue (PreviewSaveChoices::Match (choices, L"C:\\Out\\letter.TXT") == Kind::ViewText);
        Assert::IsTrue (PreviewSaveChoices::Match (choices, L"C:\\Out\\letter.rtf") == Kind::Converted);
        Assert::IsTrue (PreviewSaveChoices::Match (choices, L"C:\\Out\\letter.as")  == Kind::AppleSingle);
        Assert::IsTrue (PreviewSaveChoices::Match (choices, L"C:\\Out\\letter.dat") == Kind::Original, L"Any other extension is the original bytes");
    }


    TEST_METHOD (GetViewText_OfEachKind)
    {
        PreviewContent  preview;
        Paragraph       paragraph;



        preview.kind  = PreviewContent::Kind::Listing;
        preview.lines = { L"10 PRINT", L"20 END" };
        Assert::AreEqual (std::wstring (L"10 PRINT\r\n20 END\r\n"), PreviewSaveChoices::GetViewText (preview));

        preview       = PreviewContent();
        preview.kind  = PreviewContent::Kind::Grid;
        preview.grid  = Grid { { L"A", L"B" }, { { L"1", L"2" } } };
        Assert::AreEqual (std::wstring (L"A\tB\r\n1\t2\r\n"), PreviewSaveChoices::GetViewText (preview));

        preview        = PreviewContent();
        preview.kind   = PreviewContent::Kind::Hex;
        preview.bytes  = { 0xC1, 0x42, 0x00 };
        preview.origin = 0x0800;
        Assert::AreEqual (std::wstring (L"0800  C1 42 00" + std::wstring (13 * 3, L' ') + L"  AB.\r\n"), PreviewSaveChoices::GetViewText (preview));

        preview          = PreviewContent();
        preview.kind     = PreviewContent::Kind::Document;
        paragraph.runs   = { Run { L"Dear " }, Run { L"Sir", true } };
        preview.paragraphs = { paragraph };
        Assert::AreEqual (std::wstring (L"Dear Sir\r\n"), PreviewSaveChoices::GetViewText (preview));
    }
};
