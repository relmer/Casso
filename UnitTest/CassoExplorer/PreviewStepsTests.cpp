#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/Model/PreviewSteps.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewStepsTests
//
//  The expanded preview's previous and next: files in row order, folders
//  skipped, the ends, a step set, and a folder changed between steps.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PreviewStepsTests)
{
public:

    //  A row for each of `names`; a name ending in '/' is a folder.
    static std::vector<CatalogRow> MakeRows (std::initializer_list<const wchar_t *> names)
    {
        std::vector<CatalogRow>  rows;



        for (const wchar_t * name : names)
        {
            CatalogRow    row;
            std::wstring  text = name;

            row.isDirectory = text.ends_with (L'/');
            row.name        = row.isDirectory ? text.substr (0, text.size() - 1) : text;
            rows.push_back (row);
        }

        return rows;
    }


    static std::vector<std::wstring> MakeKeys (const std::vector<CatalogRow> & rows)
    {
        std::vector<std::wstring>  keys;



        for (const CatalogRow & row : rows)
        {
            keys.push_back (L"0:" + row.name);
        }

        return keys;
    }


    TEST_METHOD (Next_SkipsFoldersAndStopsAtTheEnd)
    {
        std::vector<CatalogRow>    rows = MakeRows ({ L"GAMES/", L"HELLO", L"UTILS/", L"PIC", L"NOTES" });
        std::vector<std::wstring>  keys = MakeKeys (rows);



        Assert::AreEqual (1,  PreviewSteps::GetTarget (rows, keys, {}, -1, true));
        Assert::AreEqual (3,  PreviewSteps::GetTarget (rows, keys, {}, 1, true));
        Assert::AreEqual (4,  PreviewSteps::GetTarget (rows, keys, {}, 3, true));
        Assert::AreEqual (-1, PreviewSteps::GetTarget (rows, keys, {}, 4, true));
    }


    TEST_METHOD (Previous_SkipsFoldersAndStopsAtTheStart)
    {
        std::vector<CatalogRow>    rows = MakeRows ({ L"GAMES/", L"HELLO", L"UTILS/", L"PIC", L"NOTES" });
        std::vector<std::wstring>  keys = MakeKeys (rows);



        Assert::AreEqual (4,  PreviewSteps::GetTarget (rows, keys, {}, -1, false));
        Assert::AreEqual (1,  PreviewSteps::GetTarget (rows, keys, {}, 3, false));
        Assert::AreEqual (-1, PreviewSteps::GetTarget (rows, keys, {}, 1, false));
    }


    TEST_METHOD (StepSet_LimitsTheStepsToItsFiles)
    {
        std::vector<CatalogRow>    rows = MakeRows ({ L"A", L"B", L"C", L"D", L"E" });
        std::vector<std::wstring>  keys = MakeKeys (rows);
        std::vector<std::wstring>  set  = { L"0:B", L"0:D", L"0:E" };



        Assert::AreEqual (1,  PreviewSteps::GetTarget (rows, keys, set, -1, true));
        Assert::AreEqual (3,  PreviewSteps::GetTarget (rows, keys, set, 1, true));
        Assert::AreEqual (4,  PreviewSteps::GetTarget (rows, keys, set, 3, true));
        Assert::AreEqual (-1, PreviewSteps::GetTarget (rows, keys, set, 4, true));
        Assert::AreEqual (1,  PreviewSteps::GetTarget (rows, keys, set, 3, false));
    }


    //  A file added after the current one is the next step, and a file
    //  removed is stepped past, since the rows are read at each step.
    TEST_METHOD (FolderChangedBetweenSteps_StepsByTheRowsAsTheyAreNow)
    {
        std::vector<CatalogRow>    before = MakeRows ({ L"A", L"C", L"D" });
        std::vector<CatalogRow>    after  = MakeRows ({ L"A", L"B", L"D" });
        std::vector<std::wstring>  keys   = MakeKeys (before);



        Assert::AreEqual (1, PreviewSteps::GetTarget (before, keys, {}, 0, true));

        keys = MakeKeys (after);

        Assert::AreEqual (1, PreviewSteps::GetTarget (after, keys, {}, 0, true));
        Assert::AreEqual (2, PreviewSteps::GetTarget (after, keys, {}, 1, true));
    }


    TEST_METHOD (NoFiles_HasNoTarget)
    {
        std::vector<CatalogRow>    rows = MakeRows ({ L"GAMES/", L"UTILS/" });
        std::vector<std::wstring>  keys = MakeKeys (rows);



        Assert::AreEqual (-1, PreviewSteps::GetTarget (rows, keys, {}, -1, true));
        Assert::AreEqual (-1, PreviewSteps::GetTarget (rows, keys, {}, 0, false));
        Assert::AreEqual (-1, PreviewSteps::GetTarget ({}, {}, {}, -1, true));
    }
};
