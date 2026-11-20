#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOptionRows
//
//  The rows Copy to folder with options has for a selection: one for each
//  kind of file in it that a copy has options for, in a fixed order. Each
//  row's choices are indexes into its list, read from and written to a
//  DecodeOptions.
//
////////////////////////////////////////////////////////////////////////////////

class CopyOptionRows
{
public:
    enum class Row { Charset, Spreadsheet, HiRes, DoubleHiRes, Basic, Code };

    //  The rows a file adds, by its format, its encoding on a copy, and
    //  whether its type marks it as code.
    static void  Add (FileFormat format, DiskOperations::Encoding encoding, bool executable, std::vector<Row> & ioRows);

    static int   GetChoice (Row row, const DecodeOptions & options);
    static void  SetChoice (Row row, int choice, DecodeOptions & ioOptions);
};
