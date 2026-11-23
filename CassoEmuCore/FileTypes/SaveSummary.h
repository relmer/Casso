#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummary
//
//  A disk's saved game as the rows of an Apple text screen, for a thumbnail
//  of a disk that doesn't boot: a line for each position of an Infocom save
//  disk, as the save viewer lists it, or a line for each character of a
//  Wizardry scenario's roster, from the volume's SCENARIO.DATA. A row is at
//  most kColumns characters of ASCII, and a longer line wraps at a space.
//
////////////////////////////////////////////////////////////////////////////////

class SaveSummary
{
public:
    static constexpr size_t  kColumns = 40;
    static constexpr size_t  kRows    = 24;

    //  The rows of a disk in DOS sector order; false when it has no save.
    static bool  Find     (const std::vector<Byte> & sectors, std::vector<std::string> & outRows);

    //  A heading, a blank row and the lines, each wrapped, to kRows rows.
    static void  MakeRows (const std::string & heading, const std::vector<std::string> & lines, std::vector<std::string> & outRows);

private:
    static constexpr size_t  kNameColumn = 12;      // a Wizardry character's name, padded

    static bool  FindInfocom  (const std::vector<Byte> & sectors, std::vector<std::string> & outLines);
    static bool  FindWizardry (const std::vector<Byte> & sectors, std::vector<std::string> & outLines);
    static void  Wrap         (const std::string & line, std::vector<std::string> & ioRows);
};
