#include "Pch.h"
#include "Cassque/CassqueAbout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAboutTests
//
//  The About text explains the name and credits the photograph with a link;
//  a module without the picture reports it rather than showing garbage.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueAboutTests)
{
public:

    TEST_METHOD (Body_ExplainsTheNameAndLinksTheCredit)
    {
        std::vector<DialogTextRun>  runs      = CassqueAbout::GetBody();
        bool                        explained = false;
        bool                        credited  = false;

        for (const DialogTextRun & run : runs)
        {
            explained = explained || run.text.find (L"casque") != std::wstring::npos;
            credited  = credited  || (run.isHyperlink && run.hyperlinkUrl == CassqueAbout::kPhotoCreditUrl
                                                      && run.text.find (L"CC BY-NC-SA 3.0") != std::wstring::npos);
        }

        Assert::IsTrue (explained);
        Assert::IsTrue (credited);
    }


    TEST_METHOD (Picture_MissingFromTheModuleFails)
    {
        DialogImage  image;
        HRESULT      hr = CassqueAbout::LoadPicture (GetModuleHandleW (L"UnitTest.dll"), image);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (image.rgba.empty());
    }
};
