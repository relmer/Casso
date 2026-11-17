#pragma once

#include "Pch.h"

#include "BasicProgram.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCrossReference
//
//  The variables, the line references and the CALL, PEEK and POKE addresses
//  of a tokenized Applesoft or Integer BASIC program, each with the lines it
//  is written on.
//
//  An Applesoft variable is listed as written, although only its first two
//  characters are significant to Applesoft.
//
////////////////////////////////////////////////////////////////////////////////

class BasicCrossReference
{
public:
    struct Entry
    {
        std::string            label;
        std::vector<uint32_t>  lines;
        bool                   isMissing = false;   // a reference to a line the program does not have
    };

    struct Report
    {
        BasicProgram::Dialect  dialect = BasicProgram::Dialect::Applesoft;
        std::vector<Entry>     variables;
        std::vector<Entry>     lineReferences;
        std::vector<Entry>     addresses;
    };

    static HRESULT      Build  (const std::vector<Byte> & programBytes, Report & outReport, std::string & outReason);
    static std::string  Format (const Report & report);

private:
    using LineSet = std::set<uint32_t>;

    static void  AppendSection (const char * title, const std::vector<Entry> & entries, std::string & ioText);
    static void  AppendEntry   (const std::string & label, const LineSet & lines, bool isMissing, std::vector<Entry> & ioEntries);
};
