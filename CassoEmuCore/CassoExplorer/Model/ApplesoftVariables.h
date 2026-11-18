#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables
//
//  A ProDOS VAR file, the Applesoft variables that BASIC.SYSTEM's STORE writes,
//  as rows: each simple variable, then each array's elements.
//
//  The file is a five-byte header and then a copy of memory. The header is the
//  length of the simple variables and arrays together, the length of the
//  simple variables, and the page of HIMEM. The copy is the simple variable
//  table, the array table and the strings, which STORE moves together so they
//  end at HIMEM, followed by the page above HIMEM, which is not part of them.
//  The aux type is the address the copy starts at, so a string's address
//  less the aux type is its offset into the copy.
//
//  Each table entry is Applesoft's: two name bytes whose high bits indicate the
//  type, then the value. An integer is high byte first, a real is Applesoft's
//  five-byte float, and a string is its length and address. An array's
//  dimensions are high byte first and in the reverse of the order DIM lists
//  them, and its first subscript varies fastest.
//
////////////////////////////////////////////////////////////////////////////////

class ApplesoftVariables
{
public:
    static constexpr Byte  kProDosType = 0xFD;

    //  The preview's columns, in order.
    enum class Column { Name, Type, Length, Value };

    //  One simple variable (array -1) or one array's elements, as indexes
    //  into the rows, in display order.
    struct Section
    {
        int                  array = -1;
        std::vector<size_t>  rows;
    };

    //  The file's variables and arrays; ERROR_INVALID_DATA when its lengths or
    //  an array's entry extend past the file.
    static HRESULT  Decode (std::span<const Byte> file, Word auxType, std::vector<VariableRow> & outRows, std::vector<VariableArray> & outArrays);

    //  The rows that contain `filter` in their name or value, or are elements
    //  of an array whose name contains it, sorted on `column`: each simple
    //  variable and each array sorted together, an array on its name, its
    //  elements' type and its element count. An array's elements are in
    //  subscript order when sorted by name.
    static std::vector<Section>  Arrange (const std::vector<VariableRow>   & rows,
                                          const std::vector<VariableArray> & arrays,
                                          const std::wstring               & filter,
                                          Column                             column,
                                          bool                               descending);

    //  A five-byte real as PRINT prints it: nine significant digits, without a
    //  leading zero, and in E notation below .01 or from 1E9.
    static std::wstring  FormatReal (std::span<const Byte> value);

    //  A variable's name with its type's suffix: A, N%, A$ or FN F.
    static std::wstring  FormatName (Byte first, Byte second);

private:
    enum class Type { Real, Integer, String, Function };

    //  One array's elements and the array itself, from its entry at `pos`,
    //  and the entry's length.
    static HRESULT       AddArray     (std::span<const Byte> file, size_t pos, Word origin, std::vector<VariableRow> & ioRows, std::vector<VariableArray> & ioArrays, size_t & outLength);

    static VariableRow      MakeRow       (const std::wstring & name, Type type, std::span<const Byte> value, std::span<const Byte> file, Word origin);
    static Type             GetType       (Byte first, Byte second);
    static const wchar_t  * GetTypeName   (Type type);
    static size_t           GetValueSize  (Type type);
    static std::wstring     FormatValue   (Type type, std::span<const Byte> value, std::span<const Byte> file, Word origin);
    static std::wstring     FormatString  (std::span<const Byte> value, std::span<const Byte> file, Word origin);
    static bool             Contains      (const std::wstring & text, const std::wstring & lowerFilter);
    static int              Compare       (const VariableRow & a, const VariableRow & b, Column column);
    static int              CompareValues (const VariableRow & a, const VariableRow & b);
    static bool             IsNumber      (const VariableRow & row);

    static constexpr size_t  s_kHeaderBytes       = 5;
    static constexpr size_t  s_kTrailingBytes     = 256;
    static constexpr size_t  s_kEntryBytes        = 7;
    static constexpr size_t  s_kArrayHeaderBytes  = 5;
    static constexpr size_t  s_kRealBytes         = 5;
    static constexpr size_t  s_kIntegerBytes      = 2;
    static constexpr size_t  s_kStringBytes       = 3;
    static constexpr size_t  s_kSignificantDigits = 9;
    static constexpr int     s_kExponentBias      = 128;
    static constexpr int     s_kMantissaBits      = 32;

    //  The decimal exponents PRINT prints without E notation.
    static constexpr int     s_kFixedExponentLow  = -2;
    static constexpr int     s_kFixedExponentHigh = 8;

    //  Where Unicode's pictures of the control characters start, for a control
    //  character in a string.
    static constexpr wchar_t  s_kControlPictures = 0x2400;
};
