#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables
//
//  A ProDOS VAR file, the Applesoft variables that BASIC.SYSTEM's STORE writes,
//  as one name and value for each variable and array element.
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

    //  The file's variables as name and value; ERROR_INVALID_DATA when its
    //  lengths or an array's entry extend past the file.
    static HRESULT       Decode     (std::span<const Byte> file, Word auxType, std::vector<std::pair<std::wstring, std::wstring>> & outRows);

    //  A five-byte real as PRINT prints it: nine significant digits, without a
    //  leading zero, and in E notation below .01 or from 1E9.
    static std::wstring  FormatReal (std::span<const Byte> value);

    //  A variable's name with its type's suffix: A, N%, A$ or FN F.
    static std::wstring  FormatName (Byte first, Byte second);

private:
    enum class Type { Real, Integer, String, Function };

    //  One array's rows, from its entry at `pos`, and the entry's length.
    static HRESULT       AddArrayRows (std::span<const Byte> file, size_t pos, Word origin, std::vector<std::pair<std::wstring, std::wstring>> & ioRows, size_t & outLength);

    static Type          GetType      (Byte first, Byte second);
    static size_t        GetValueSize (Type type);
    static std::wstring  FormatValue  (Type type, std::span<const Byte> value, std::span<const Byte> file, Word origin);
    static std::wstring  FormatString (std::span<const Byte> value, std::span<const Byte> file, Word origin);

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

    //  The elements of one array listed before the rest are counted.
    static constexpr size_t  s_kElementsShown = 100;

    //  The decimal exponents PRINT prints without E notation.
    static constexpr int     s_kFixedExponentLow  = -2;
    static constexpr int     s_kFixedExponentHigh = 8;

    //  Where Unicode's pictures of the control characters start, for a control
    //  character in a string.
    static constexpr wchar_t  s_kControlPictures = 0x2400;
};
