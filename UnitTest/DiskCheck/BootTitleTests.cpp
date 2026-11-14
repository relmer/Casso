#include "Pch.h"

#include "DiskCheck/BootTitle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootTitleTests
//
//  A program's title from the rows of its text screen.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootTitleTests)
{
public:

    static std::vector<std::string>  Screen (std::initializer_list<std::pair<int, const char *>> lines)
    {
        std::vector<std::string>  rows (24, std::string (40, ' '));

        for (const auto & [row, text] : lines)
        {
            std::string  line = text;

            line.resize (40, ' ');
            rows[(size_t) row] = line;
        }

        return rows;
    }

    //  The Witness: its title is ten lines down a page of prose, with its
    //  copyright under it.
    TEST_METHOD (ALineOverACopyrightLine_IsTheTitle)
    {
        std::vector<std::string>  rows = Screen ({ { 2,  "SOMEWHERE NEAR LOS ANGELES. A COLD" },
                                                   { 3,  "FRIDAY EVENING IN FEBRUARY 1938." },
                                                   { 10, "THE WITNESS: AN INTERLOGIC MYSTERY" },
                                                   { 11, "COPYRIGHT (C) 1983 INFOCOM, INC." },
                                                   { 12, "ALL RIGHTS RESERVED." } });

        Assert::AreEqual (std::string ("THE WITNESS: AN INTERLOGIC MYSTERY"), BootTitle::Find (rows, true));
    }

    TEST_METHOD (ABlankLineBetweenThem_IsPassedOver)
    {
        std::vector<std::string>  rows = Screen ({ { 8, "    *** Wizardry ***" }, { 10, "(C) 1981 SIR-TECH SOFTWARE" } });

        Assert::AreEqual (std::string ("Wizardry"), BootTitle::Find (rows, true), L"The punctuation at its ends is dropped");
    }

    TEST_METHOD (TheOneLineOfTheFirstFour_IsTheTitle)
    {
        std::vector<std::string>  rows = Screen ({ { 1, "DOS 3.3 SYSTEM MASTER" }, { 6, "JANUARY 1, 1983" } });

        Assert::AreEqual (std::string ("DOS 3.3 SYSTEM MASTER"), BootTitle::Find (rows, true));
    }

    TEST_METHOD (AScreenWithNeither_HasNoTitle)
    {
        Assert::IsTrue (BootTitle::Find (Screen ({ { 0, "IT WAS A DARK AND" }, { 1, "STORMY NIGHT." }, { 5, "MORE PROSE" } }), true).empty(), L"A page of prose");
        Assert::IsTrue (BootTitle::Find (Screen ({ { 0, "]" } }), true).empty(), L"A prompt");
        Assert::IsTrue (BootTitle::Find (Screen ({}), true).empty(), L"A blank screen");
        Assert::IsTrue (BootTitle::Find (Screen ({ { 1, "COPYRIGHT 1984 SOMEONE" } }), true).empty(), L"A copyright line alone");
    }

    TEST_METHOD (ATitleAsAName_IsUpToItsColon_AndInTitleCase)
    {
        Assert::AreEqual (std::string ("The Witness"), BootTitle::ToName ("THE WITNESS: AN INTERLOGIC MYSTERY"));
        Assert::AreEqual (std::string ("Wizardry"), BootTitle::ToName ("Wizardry"), L"A title with small letters is as drawn");
        Assert::AreEqual (std::string ("Ultima III"), BootTitle::ToName ("ULTIMA III"));
        Assert::AreEqual (std::string ("Lord of the Rings"), BootTitle::ToName ("LORD OF THE RINGS"));
        Assert::AreEqual (std::string ("DOS 3.3 System Master"), BootTitle::ToName ("DOS 3.3 SYSTEM MASTER"));
        Assert::AreEqual (std::string ("One-On-One"), BootTitle::ToName ("ONE-ON-ONE"));
    }

    TEST_METHOD (AScreenInAGraphicsMode_HasNoTitle)
    {
        Assert::IsTrue (BootTitle::Find (Screen ({ { 1, "DOS 3.3 SYSTEM MASTER" } }), false).empty());
    }
};
