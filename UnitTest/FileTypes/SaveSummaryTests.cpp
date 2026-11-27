#include "Pch.h"

#include "FileTypes/SaveSummary.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummaryTests
//
//  A save's rows for a thumbnail. InfocomSaveTests and WizardryScenarioTests
//  cover reading the saves.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SaveSummaryTests)
{
public:

    //  The heading, a blank row, then each line, a long one wrapped at a
    //  space to rows of at most 40 characters.
    TEST_METHOD (MakeRows_WrapsLongLinesAtASpace)
    {
        std::vector<std::string>  rows;



        SaveSummary::MakeRows ("Infocom saved games", { "1. Zork I, release 88: West of House, score 0 of 350, 1 move", "2. short" }, rows);

        Assert::AreEqual ((size_t) 5, rows.size());
        Assert::AreEqual (std::string ("Infocom saved games"), rows[0]);
        Assert::IsTrue   (rows[1].empty());
        Assert::AreEqual (std::string ("1. Zork I, release 88: West of House,"), rows[2]);
        Assert::AreEqual (std::string ("score 0 of 350, 1 move"), rows[3]);
        Assert::AreEqual (std::string ("2. short"), rows[4]);

        for (const std::string & row : rows)
        {
            Assert::IsTrue (row.size() <= SaveSummary::kColumns);
        }
    }

    //  At most a screen of rows, and no rows for a disk with no save.
    TEST_METHOD (MakeRows_FitsAScreen_AndADiskWithNoSaveHasNoRows)
    {
        std::vector<std::string>  lines (40, "line");
        std::vector<std::string>  rows;
        std::vector<Byte>         blank (143360, 0);



        SaveSummary::MakeRows ("Wizardry saved game", lines, rows);
        Assert::AreEqual (SaveSummary::kRows, rows.size());

        Assert::IsFalse (SaveSummary::Find (blank, rows));
        Assert::IsTrue  (rows.empty());
    }
};
