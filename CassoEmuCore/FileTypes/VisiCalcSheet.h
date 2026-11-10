#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet
//
//  A VisiCalc file, as the storage chapter of the 1982 VisiCalc User's
//  Guide sets it out: text, a line for each entry, `>` then its coordinate,
//  a colon, an optional format command such as `/FR`, and its contents: a
//  label after a quote, a repeating label after `/-`, or a value or formula
//  as typed. Lines starting with `/` are the sheet's global settings: the
//  column width `/GC`, the format `/GF`, the recalculation order `/GO` and
//  others.
//
////////////////////////////////////////////////////////////////////////////////

class VisiCalcSheet
{
public:
    struct Cell
    {
        size_t       column = 0;
        size_t       row    = 0;
        char         format = 0;        // the letter after /F, 0 for none
        bool         label  = false;
        bool         repeat = false;
        std::string  contents;          // a label's text, or the formula as typed
    };

    struct Value
    {
        enum class Kind { Blank, Label, Number, Boolean, NotAvailable, Error };

        Kind    kind   = Kind::Blank;
        double  number = 0;             // 1 or 0 for a truth value

        bool  operator== (const Value &) const = default;
    };

    struct Sheet
    {
        std::vector<Cell>  cells;
        size_t             columnWidth = 9;
        char               format      = 'G';
        bool               byColumns   = true;
        size_t             columns     = 0;
        size_t             rows        = 0;
    };

    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static bool     ParseCoordinate (std::string_view text, size_t & outColumn, size_t & outRow, size_t & outLength);
    static HRESULT  Parse           (std::span<const Byte> bytes, Sheet & outSheet, std::wstring & outError);

    //  A cell as VisiCalc draws it without recalculating: a label, a
    //  repeating label across the column, or a formula as typed.
    static std::wstring  GetEntered (const Sheet & sheet, const Cell & cell);

    //  A cell as VisiCalc draws it after a recalculation, the column's width
    //  in characters: the value in the cell's format, or the sheet's if
    //  the cell lacks one.
    static std::wstring  Draw         (const Sheet & sheet, const Cell & cell, const Value & value);
    static std::string   FormatNumber (double number, char format, size_t limit);

private:
    static constexpr int     kDigits     = 11;

    static std::string  FormatFixed      (double number, int decimals);
    static std::string  FormatGeneral    (double number, size_t limit);
    static std::string  FormatScientific (double number, size_t limit);

    static constexpr Byte    kProDosText = 0x04;
    static constexpr Byte    kDosText    = 0x00;
    static constexpr size_t  kMaxColumns = 63;
    static constexpr size_t  kMaxRows    = 254;

    static void  ApplyGlobal (std::string_view line, Sheet & ioSheet);
};
