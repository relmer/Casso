#include "Pch.h"

#include "CassoExplorer/Model/CopyOptionRows.h"
#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOptionRows::Add
//
//  A row is added once, and the rows are sorted in the enum's order.
//
////////////////////////////////////////////////////////////////////////////////

void CopyOptionRows::Add (FileFormat format, DiskOperations::Encoding encoding, bool executable, std::vector<Row> & ioRows)
{
    std::vector<Row>  added;



    if (encoding == DiskOperations::Encoding::Text || FileFormatTable::HasCharsetChoice (format))
    {
        added.push_back (Row::Charset);
    }

    if (format == FileFormat::VisiCalc)
    {
        added.push_back (Row::Spreadsheet);
    }

    if (format == FileFormat::HiResScreen)
    {
        added.push_back (Row::HiRes);
    }

    if (format == FileFormat::DoubleHiResScreen)
    {
        added.push_back (Row::DoubleHiRes);
    }

    if (encoding == DiskOperations::Encoding::Basic)
    {
        added.push_back (Row::Basic);
    }

    if (format == FileFormat::None && executable)
    {
        added.push_back (Row::Code);
    }

    for (Row row : added)
    {
        if (std::find (ioRows.begin(), ioRows.end(), row) == ioRows.end())
        {
            ioRows.push_back (row);
        }
    }

    std::sort (ioRows.begin(), ioRows.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOptionRows::GetChoice
//
//  A BASIC listing is plain (0), plain with its control characters as the
//  bytes (1), or colored (2).
//
////////////////////////////////////////////////////////////////////////////////

int CopyOptionRows::GetChoice (Row row, const DecodeOptions & options)
{
    switch (row)
    {
        case Row::Charset:      return (int) options.charset;
        case Row::Spreadsheet:  return options.formulas ? 1 : 0;
        case Row::HiRes:        return options.hiResMono ? 1 : 0;
        case Row::DoubleHiRes:  return (int) options.doubleHiRes;
        case Row::Basic:        return options.basicColor ? 2 : options.basicRawControls ? 1 : 0;
        case Row::Code:         return (int) options.disassembly;
        default:                return 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOptionRows::SetChoice
//
////////////////////////////////////////////////////////////////////////////////

void CopyOptionRows::SetChoice (Row row, int choice, DecodeOptions & ioOptions)
{
    switch (row)
    {
        case Row::Charset:      ioOptions.charset     = (Charset) choice;           break;
        case Row::Spreadsheet:  ioOptions.formulas    = choice == 1;                break;
        case Row::HiRes:        ioOptions.hiResMono   = choice == 1;                break;
        case Row::DoubleHiRes:  ioOptions.doubleHiRes = (DoubleHiResForm) choice;   break;
        case Row::Code:         ioOptions.disassembly = (DisassemblyCpu) choice;    break;

        case Row::Basic:
            ioOptions.basicRawControls = choice == 1;
            ioOptions.basicColor       = choice == 2;
            break;

        default:
            break;
    }
}
