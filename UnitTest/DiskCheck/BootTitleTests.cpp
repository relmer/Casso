#include "Pch.h"

#include "Core/TextEncoding.h"
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

    //  The DOS 3.3 System Master's screen, as it boots: its date is between
    //  the title and the copyright line.
    TEST_METHOD (ADateOverTheCopyrightLine_IsPassedOver)
    {
        std::vector<std::string>  rows = Screen ({ { 1,  "               APPLE II" },
                                                   { 3,  "     DOS VERSION 3.3  SYSTEM MASTER" },
                                                   { 6,  "            JANUARY 1, 1983" },
                                                   { 9,  "COPYRIGHT APPLE COMPUTER,INC. 1980,1982" },
                                                   { 11, "        BE SURE CAPS LOCK IS DOWN" },
                                                   { 14, "]" } });

        Assert::AreEqual (std::string ("DOS VERSION 3.3  SYSTEM MASTER"), BootTitle::Find (rows, true));
        Assert::AreEqual (std::string ("DOS Version 3.3 System Master"), BootTitle::ToName (BootTitle::Find (rows, true)));
    }

    TEST_METHOD (AScreenWithNeither_HasNoTitle)
    {
        Assert::IsTrue (BootTitle::Find (Screen ({ { 0, "IT WAS A DARK AND" }, { 1, "STORMY NIGHT." }, { 5, "MORE PROSE" } }), true).empty(), L"A page of prose");
        Assert::IsTrue (BootTitle::Find (Screen ({ { 0, "]" } }), true).empty(), L"A prompt");
        Assert::IsTrue (BootTitle::Find (Screen ({}), true).empty(), L"A blank screen");
        Assert::IsTrue (BootTitle::Find (Screen ({ { 1, "COPYRIGHT 1984 SOMEONE" } }), true).empty(), L"A copyright line alone");
    }

    TEST_METHOD (ATitleAsAName_HasAHyphenForEachColon_AndIsInTitleCase)
    {
        Assert::AreEqual (std::string ("The Witness - An Interlogic Mystery"), BootTitle::ToName ("THE WITNESS: AN INTERLOGIC MYSTERY"));
        Assert::AreEqual (std::string ("Ultima IV - Quest of the Avatar"), BootTitle::ToName ("ULTIMA IV :  QUEST   OF THE AVATAR"), L"One space on each side of the hyphen");
        Assert::AreEqual (std::string ("Archon"), BootTitle::ToName ("ARCHON:"));
        Assert::AreEqual (std::string ("Wizardry"), BootTitle::ToName ("Wizardry"), L"A title with small letters is as drawn");
        Assert::AreEqual (std::string ("Ultima III"), BootTitle::ToName ("ULTIMA III"));
        Assert::AreEqual (std::string ("Lord of the Rings"), BootTitle::ToName ("LORD OF THE RINGS"));
        Assert::AreEqual (std::string ("DOS 3.3 System Master"), BootTitle::ToName ("DOS 3.3 SYSTEM MASTER"));
        Assert::AreEqual (std::string ("One-On-One"), BootTitle::ToName ("ONE-ON-ONE"));
    }

    //  Apple Pascal 1.3's welcome: the lines over its copyright are a block
    //  of sentences, none set off, so it has no title. A centered line with
    //  text above it is a title.
    TEST_METHOD (ASentenceOrALineInABlock_IsNotATitle)
    {
        std::vector<std::string>  welcome  = Screen ({ { 0, "Command: F(ile, E(dit, R(un, C(omp, L(ink" },
                                                       { 1, "Welcome APPLE1, to Apple II Pascal 1.3" },
                                                       { 2, "Based on UCSD Pascal II.1" },
                                                       { 3, "Current date is 10-Apr-90" },
                                                       { 4, "Pascal system size is 64K" },
                                                       { 5, "Copyright Apple Computer 1979-1985" } });
        std::vector<std::string>  centered = Screen ({ { 3, "PRESENTED BY BRODERBUND" },
                                                       { 4, "                KARATEKA" },
                                                       { 5, "COPYRIGHT 1984 JORDAN MECHNER" } });

        Assert::IsTrue (BootTitle::Find (welcome, true).empty(), TextEncoding::NarrowToWide (BootTitle::Find (welcome, true)).c_str());
        Assert::AreEqual (std::string ("KARATEKA"), BootTitle::Find (centered, true));
        Assert::IsTrue (BootTitle::Find (Screen ({ { 1, "INSERT DISK 2 AND PRESS RETURN." } }), true).empty(), L"A sentence alone in the first four");
    }

    TEST_METHOD (AScreenInAGraphicsMode_HasNoTitle)
    {
        Assert::IsTrue (BootTitle::Find (Screen ({ { 1, "DOS 3.3 SYSTEM MASTER" } }), false).empty());
    }
};
