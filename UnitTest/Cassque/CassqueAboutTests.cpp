#include "Pch.h"
#include "Cassque/CassqueAbout.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAboutTests
//
//  The About text explains the name as cask + Casso = Cassque, in icons when
//  it has them and in words when it does not, and credits the photograph
//  with a link; a module without a picture reports it rather than showing
//  garbage.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueAboutTests)
{
public:

    static DialogImage MakePixel()
    {
        DialogImage  image;

        image.width  = 1;
        image.height = 1;
        image.rgba   = { 0x10, 0x20, 0x30, 0xFF };

        return image;
    }


    static const DialogTextRun * FindEquation (const std::vector<DialogTextRun> & runs)
    {
        for (const DialogTextRun & run : runs)
        {
            if (!run.strip.empty())
            {
                return &run;
            }
        }

        return nullptr;
    }


    TEST_METHOD (Body_DescribesTheAppAndLinksTheCredit)
    {
        std::vector<DialogTextRun>  runs      = CassqueAbout::GetBody ({});
        bool                        described = false;
        bool                        asked     = false;
        bool                        homophone = false;
        bool                        credited  = false;

        for (const DialogTextRun & run : runs)
        {
            described = described || run.text == L"Apple II disk image explorer";
            asked     = asked     || run.text == L"What's with the name?";
            homophone = homophone || run.text.find (L"homophone of casque") != std::wstring::npos;
            credited  = credited  || (run.isHyperlink && run.hyperlinkUrl == CassqueAbout::kPhotoCreditUrl
                                                      && run.text.find (L"CC BY-NC-SA 3.0") != std::wstring::npos);
        }

        Assert::IsTrue (described);
        Assert::IsTrue (asked);
        Assert::IsTrue (homophone);
        Assert::IsTrue (credited);
    }


    TEST_METHOD (Body_WithoutPicturesWritesTheEquationInWords)
    {
        std::vector<DialogTextRun>  runs     = CassqueAbout::GetBody ({});
        const DialogTextRun       * equation = FindEquation (runs);

        Assert::IsNotNull (equation);
        Assert::AreEqual ((size_t) 5, equation->strip.size());
        Assert::AreEqual (std::wstring (L"Cask"),    equation->strip[0].text);
        Assert::AreEqual (std::wstring (L"+"),       equation->strip[1].text);
        Assert::AreEqual (std::wstring (L"Casso"),   equation->strip[2].text);
        Assert::AreEqual (std::wstring (L"="),       equation->strip[3].text);
        Assert::AreEqual (std::wstring (L"Cassque"), equation->strip[4].text);

        for (const DialogInlinePiece & piece : equation->strip)
        {
            Assert::IsTrue (piece.image.rgba.empty());
        }

        for (const DialogTextRun & run : runs)
        {
            Assert::IsFalse (run.leadingImage.has_value());
        }
    }


    TEST_METHOD (Body_PicturesJoinTheEquationAndLeadTheirRows)
    {
        CassqueAbout::Pictures      pictures = { MakePixel(), MakePixel(), MakePixel() };
        std::vector<DialogTextRun>  runs     = CassqueAbout::GetBody (pictures);
        const DialogTextRun       * equation = FindEquation (runs);
        int                         leading  = 0;

        Assert::IsNotNull (equation);
        Assert::AreEqual (CassqueAbout::kEquationPictureDp, equation->strip[0].image.displayDp);
        Assert::AreEqual (CassqueAbout::kEquationPictureDp, equation->strip[4].image.displayDp);
        Assert::IsTrue   (equation->strip[1].image.rgba.empty());

        for (const DialogTextRun & run : runs)
        {
            if (run.leadingImage.has_value())
            {
                Assert::AreEqual (CassqueAbout::kRowPictureDp, run.leadingImage->displayDp);
                leading++;
            }
        }

        Assert::AreEqual (3, leading);
    }


    TEST_METHOD (Picture_MissingFromTheModuleFails)
    {
        DialogImage  image;
        HRESULT      hr = CassqueAbout::LoadPicture (GetModuleHandleW (L"UnitTest.dll"), IDR_CASSQUE_PICTURE_PNG, CassqueAbout::kPictureDp, image);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (image.rgba.empty());
    }
};
