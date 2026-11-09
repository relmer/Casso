#include "Pch.h"

#include "FileTypes/CsvWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CsvWriter::Write
//
//  A grid with no headings starts at its first row.
//
////////////////////////////////////////////////////////////////////////////////

void CsvWriter::Write (const Grid & grid, std::vector<Byte> & outBytes)
{
    std::string  csv = "\xEF\xBB\xBF";



    if (!grid.columns.empty())
    {
        AppendRow (grid.columns, csv);
    }

    for (const std::vector<std::wstring> & row : grid.rows)
    {
        AppendRow (row, csv);
    }

    outBytes.assign (csv.begin(), csv.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CsvWriter::AppendRow
//
////////////////////////////////////////////////////////////////////////////////

void CsvWriter::AppendRow (const std::vector<std::wstring> & fields, std::string & inOutCsv)
{
    for (size_t at = 0; at < fields.size(); at++)
    {
        std::string  field  = TextEncoding::WideToUtf8 (fields[at]);
        bool         quoted = field.find_first_of (",\"\r\n") != std::string::npos;

        inOutCsv += (at > 0) ? "," : "";

        if (!quoted)
        {
            inOutCsv += field;
            continue;
        }

        inOutCsv += '"';

        for (char c : field)
        {
            inOutCsv += (c == '"') ? "\"\"" : std::string (1, c);
        }

        inOutCsv += '"';
    }

    inOutCsv += "\r\n";
}
