#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CppUnitTest.h"

#include "Ui/Dialogs/AttributionsText.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AttributionsTextTests
//
//  Both applications show this list, and a work that loses its line loses its
//  attribution in the only place a user can read one. So the tests are about
//  what must be there: each work, each author, each license, and a link to
//  the source of each.
//
//  The icons are not an entry of their own. They are drawn from the
//  photograph, so the sentence that says so must sit in the photograph's
//  entry rather than anywhere else in the list.
//
////////////////////////////////////////////////////////////////////////////////

namespace UiTests
{

TEST_CLASS (AttributionsTextTests)
{
public:

    static bool Mentions (const std::vector<DialogTextRun> & runs, const wchar_t * text)
    {
        for (const DialogTextRun & run : runs)
        {
            if (run.text.find (text) != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }

    static bool Links (const std::vector<DialogTextRun> & runs, const wchar_t * url)
    {
        for (const DialogTextRun & run : runs)
        {
            if (run.isHyperlink && run.hyperlinkUrl == url)
            {
                return true;
            }
        }

        return false;
    }

    TEST_METHOD (Body_NamesEveryWorkAndItsAuthor)
    {
        std::vector<DialogTextRun>  runs = AttributionsText::BuildBody();

        Assert::IsTrue (Mentions (runs, L"Cassowary photograph"),          L"the photograph");
        Assert::IsTrue (Mentions (runs, L"Mr. Smiley / BunyipCo"),         L"its author");
        Assert::IsTrue (Mentions (runs, L"ImageWriter II printer sounds"), L"the recordings");
        Assert::IsTrue (Mentions (runs, L"Scott Lawrence"),                L"their author");
        Assert::IsTrue (Mentions (runs, L"Fluent UI System Icons"),        L"the command bar's icons");
        Assert::IsTrue (Mentions (runs, L"Copyright (c) 2020 Microsoft Corporation"), L"the notice MIT asks for");
    }


    TEST_METHOD (Body_LinksEverySourceAndLicense)
    {
        std::vector<DialogTextRun>  runs = AttributionsText::BuildBody();

        Assert::IsTrue (Links (runs, AttributionsText::kPhotoUrl),      L"where the photograph came from");
        Assert::IsTrue (Links (runs, AttributionsText::kPhotoLicense),  L"the license it is under");
        Assert::IsTrue (Links (runs, AttributionsText::kSoundsUrl),     L"where the recordings came from");
        Assert::IsTrue (Links (runs, AttributionsText::kSoundsLicense), L"the license they are under");
        Assert::IsTrue (Links (runs, AttributionsText::kIconsUrl),      L"where the icons came from");
        Assert::IsTrue (Links (runs, AttributionsText::kIconsLicense),  L"their license");

        Assert::IsTrue (Mentions (runs, L"CC BY-NC-SA 3.0"), L"the license is readable, not only clickable");
        Assert::IsTrue (Mentions (runs, L"CC BY 4.0"),       L"the same for the other");
    }


    TEST_METHOD (Body_SaysTheIconsComeFromThePhotograph)
    {
        std::vector<DialogTextRun>  runs  = AttributionsText::BuildBody();
        size_t                      photo = runs.size();
        size_t                      icons = runs.size();

        for (size_t i = 0; i < runs.size(); i++)
        {
            if (runs[i].text.find (L"Cassowary photograph") != std::wstring::npos)
            {
                photo = i;
            }

            if (runs[i].text.find (L"icons are drawn from it") != std::wstring::npos)
            {
                icons = i;
            }
        }

        Assert::IsTrue (photo < runs.size(), L"the photograph has an entry");
        Assert::IsTrue (icons < runs.size(), L"the icons are accounted for");
        Assert::IsTrue (icons > photo,       L"under the photograph, not as a work of their own");
        Assert::IsTrue (Mentions (runs, L"licensed under the same terms"), L"which terms they are under");
    }
};

}   // namespace UiTests
