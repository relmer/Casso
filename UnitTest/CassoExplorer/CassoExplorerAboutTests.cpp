#include "Pch.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerAboutTests
//
//  The About text explains the name as cask + Casso = CassoExplorer, in icons when
//  it has them and in words when it does not, and credits the photograph
//  with a link; a module without a picture reports it rather than showing
//  garbage.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerAboutTests)
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


    TEST_METHOD (Body_StampsTheVersionAndDescribesTheApp)
    {
        std::vector<DialogTextRun>  runs      = CassoExplorerAbout::GetBody();
        bool                        copyright = false;
        bool                        version   = false;
        bool                        built     = false;
        bool                        described = false;

        for (const DialogTextRun & run : runs)
        {
            copyright = copyright || run.text.find (L"Copyright (C) by Robert Elmer") != std::wstring::npos;
            version   = version   || run.text.find (L"Version ") == 0;
            built     = built     || run.text.find (L"Built ")   == 0;
            described = described || run.text == L"An Apple II disk image explorer.";
        }

        Assert::IsTrue (copyright);
        Assert::IsTrue (version);
        Assert::IsTrue (built);
        Assert::IsTrue (described);
    }


    TEST_METHOD (Body_CarriesTheSameLinksCassoDoes)
    {
        std::vector<DialogTextRun>  runs  = CassoExplorerAbout::GetBody();
        std::vector<std::wstring>   urls;

        for (const DialogTextRun & run : runs)
        {
            if (run.isHyperlink)
            {
                urls.push_back (run.hyperlinkUrl);
            }
        }

        Assert::AreEqual ((size_t) 4, urls.size());
        Assert::AreEqual (std::wstring (CassoExplorerAbout::kRepositoryUrl),  urls[0]);
        Assert::AreEqual (std::wstring (CassoExplorerAbout::kBugReportUrl),   urls[1]);
        Assert::AreEqual (std::wstring (CassoExplorerAbout::kLicenseUrl),     urls[2]);
        Assert::AreEqual (std::wstring (CassoExplorerAbout::kPhotoCreditUrl), urls[3]);
    }


    TEST_METHOD (Picture_MissingFromTheModuleFails)
    {
        DialogImage  image;
        HRESULT      hr = CassoExplorerAbout::LoadPicture (GetModuleHandleW (L"UnitTest.dll"), IDR_CASSO_EXPLORER_CASSOWARY_PNG, CassoExplorerAbout::kHeaderPictureDp, image);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (image.rgba.empty());
    }
};
