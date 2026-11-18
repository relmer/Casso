#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CsvWriter
//
//  A grid as CSV in UTF-8 with a byte order mark: the headings, then each
//  row, each line ending in CR LF. A field with a comma, a quote or a line
//  break is quoted, a quote inside it doubled, as RFC 4180 specifies.
//
////////////////////////////////////////////////////////////////////////////////

class CsvWriter
{
public:
    static void  Write (const Grid & grid, std::vector<Byte> & outBytes);

private:
    static void  AppendRow (const std::vector<std::wstring> & fields, std::string & inOutCsv);
};
