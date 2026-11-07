#include "Pch.h"

#include "CassoExplorer/Model/ApplesoftVariables.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::Decode
//
//  The simple variables, in the order the program first used them, then each
//  array: its DIM, then its elements up to s_kElementsShown and a count of the
//  rest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftVariables::Decode (std::span<const Byte> file, std::vector<std::pair<std::wstring, std::wstring>> & outRows)
{
    HRESULT  hr        = S_OK;
    size_t   himem     = 0;
    Word     origin    = 0;
    size_t   total     = 0;
    size_t   simple    = 0;
    size_t   simpleEnd = 0;
    size_t   end       = 0;
    size_t   pos       = s_kHeaderBytes;
    size_t   length    = 0;
    size_t   fileBytes = file.size();



    outRows.clear();

    CBREx (fileBytes >= s_kHeaderBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    total     = (size_t) file[0] | ((size_t) file[1] << 8);
    simple    = (size_t) file[2] | ((size_t) file[3] << 8);
    himem     = (size_t) file[4] << 8;
    simpleEnd = s_kHeaderBytes + simple;
    end       = s_kHeaderBytes + total;

    CBREx (simple <= total && end <= fileBytes && simple % s_kEntryBytes == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (fileBytes - s_kHeaderBytes <= himem,                              HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    //  Where the copy started in memory, which its strings' addresses are in.
    origin = (Word) (himem - (fileBytes - s_kHeaderBytes));

    for (; pos < simpleEnd; pos += s_kEntryBytes)
    {
        outRows.push_back ({ FormatName (file[pos], file[pos + 1]),
                             FormatValue (GetType (file[pos], file[pos + 1]), file.subspan (pos + 2, s_kRealBytes), file, origin) });
    }

    for (; pos < end; pos += length)
    {
        hr = AddArrayRows (file.first (end), pos, origin, outRows, length);
        CHR (hr);
    }

    if (outRows.empty())
    {
        outRows.push_back ({ L"Variables", L"None" });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::AddArrayRows
//
//  One array's entry at `pos`: its name, the length of the entry, the count of
//  dimensions, each dimension's element count, then the elements.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftVariables::AddArrayRows (
    std::span<const Byte>                                 file,
    size_t                                                pos,
    Word                                                  origin,
    std::vector<std::pair<std::wstring, std::wstring>>  & ioRows,
    size_t                                              & outLength)
{
    HRESULT              hr          = S_OK;
    size_t               fileBytes   = file.size();
    Type                 type        = Type::Real;
    size_t               dimensions  = 0;
    size_t               headerBytes = 0;
    size_t               valueBytes  = 0;
    size_t               count       = 1;
    size_t               elements    = 0;
    size_t               shown       = 0;
    size_t               rest        = 0;
    std::wstring         name;
    std::wstring         subscripts;
    std::vector<size_t>  counts;



    outLength = 0;

    CBREx (pos + s_kArrayHeaderBytes <= fileBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    type        = GetType (file[pos], file[pos + 1]);
    name        = FormatName (file[pos], file[pos + 1]);
    outLength   = (size_t) file[pos + 2] | ((size_t) file[pos + 3] << 8);
    dimensions  = file[pos + 4];
    headerBytes = s_kArrayHeaderBytes + 2 * dimensions;
    valueBytes  = GetValueSize (type);

    CBREx (type != Type::Function && dimensions > 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (outLength >= headerBytes && pos + outLength <= fileBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    //  The dimensions are in the reverse of DIM's order.
    counts.resize (dimensions);

    for (size_t i = 0; i < dimensions; i++)
    {
        elements = ((size_t) file[pos + s_kArrayHeaderBytes + 2 * i] << 8) | file[pos + s_kArrayHeaderBytes + 2 * i + 1];
        CBREx (elements > 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        counts[dimensions - 1 - i]  = elements;
        count                      *= elements;

        //  No element is smaller than a byte, so the product cannot
        //  overflow.
        CBREx (count <= outLength, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    CBREx (headerBytes + count * valueBytes <= outLength, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (size_t i = 0; i < dimensions; i++)
    {
        subscripts += std::format (L"{}{}", (i == 0) ? L"" : L",", counts[i] - 1);
    }

    ioRows.push_back ({ std::format (L"DIM {}({})", name, subscripts), std::format (L"{} {}", count, (count == 1) ? L"element" : L"elements") });

    shown = (std::min) (count, s_kElementsShown);

    //  The first subscript varies fastest.
    for (size_t element = 0; element < shown; element++)
    {
        subscripts.clear();
        rest = element;

        for (size_t i = 0; i < dimensions; i++)
        {
            subscripts += std::format (L"{}{}", (i == 0) ? L"" : L",", rest % counts[i]);
            rest       /= counts[i];
        }

        ioRows.push_back ({ std::format (L"{}({})", name, subscripts),
                            FormatValue (type, file.subspan (pos + headerBytes + element * valueBytes, valueBytes), file, origin) });
    }

    if (count > shown)
    {
        ioRows.push_back ({ name + L"(...)", std::format (L"{} more elements", count - shown) });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::FormatReal
//
//  The first byte is the exponent, biased by 128, and zero for zero. The four
//  bytes after it are the mantissa, high byte first, with its leading one bit
//  replaced by the sign.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ApplesoftVariables::FormatReal (std::span<const Byte> value)
{
    HRESULT       hr         = S_OK;
    size_t        valueBytes = value.size();
    uint32_t      mantissa   = 0;
    double        number     = 0;
    int           exponent   = 0;
    bool          isZero     = false;
    std::wstring  scientific;
    std::wstring  digits;
    std::wstring  text       = L"0";



    CBRAEx (valueBytes >= s_kRealBytes, E_INVALIDARG);

    isZero = value[0] == 0;
    BAIL_OUT_IF (isZero, S_OK);

    mantissa   = ((uint32_t) (value[1] | 0x80) << 24) | ((uint32_t) value[2] << 16) | ((uint32_t) value[3] << 8) | value[4];
    number     = std::ldexp ((double) mantissa, (int) value[0] - s_kExponentBias - s_kMantissaBits);
    scientific = std::format (L"{:.{}e}", number, s_kSignificantDigits - 1);
    digits     = scientific.substr (0, 1) + scientific.substr (2, s_kSignificantDigits - 1);
    exponent   = std::stoi (scientific.substr (scientific.find (L'e') + 1));

    while (digits.size() > 1 && digits.back() == L'0')
    {
        digits.pop_back();
    }

    if (exponent < s_kFixedExponentLow || exponent > s_kFixedExponentHigh)
    {
        text = digits.substr (0, 1) + ((digits.size() > 1) ? L"." + digits.substr (1) : std::wstring())
             + std::format (L"E{}{:02}", (exponent < 0) ? L'-' : L'+', std::abs (exponent));
    }
    else if (exponent < 0)
    {
        text = L"." + std::wstring ((size_t) (-exponent - 1), L'0') + digits;
    }
    else
    {
        digits.resize ((std::max) (digits.size(), (size_t) exponent + 1), L'0');
        text = digits.substr (0, (size_t) exponent + 1);

        if (digits.size() > (size_t) exponent + 1)
        {
            text += L"." + digits.substr ((size_t) exponent + 1);
        }
    }

    if (value[1] & 0x80)
    {
        text = L"-" + text;
    }

Error:
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::FormatName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ApplesoftVariables::FormatName (Byte first, Byte second)
{
    std::wstring  name (1, (wchar_t) (first & 0x7F));



    if ((second & 0x7F) != 0)
    {
        name += (wchar_t) (second & 0x7F);
    }

    switch (GetType (first, second))
    {
        case Type::Integer:   name += L"%";           break;
        case Type::String:    name += L"$";           break;
        case Type::Function:  name  = L"FN " + name;  break;
        case Type::Real:
        default:                                      break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::GetType
//
//  Applesoft sets the high bit of both name bytes for an integer, of the second
//  for a string, and of the first for a function DEF FN defines.
//
////////////////////////////////////////////////////////////////////////////////

ApplesoftVariables::Type ApplesoftVariables::GetType (Byte first, Byte second)
{
    bool  firstHigh  = (first  & 0x80) != 0;
    bool  secondHigh = (second & 0x80) != 0;



    return (firstHigh && secondHigh) ? Type::Integer
         : secondHigh                ? Type::String
         : firstHigh                 ? Type::Function
                                     : Type::Real;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::GetValueSize
//
//  The size of an array element. A simple variable's value takes five bytes
//  whatever its type.
//
////////////////////////////////////////////////////////////////////////////////

size_t ApplesoftVariables::GetValueSize (Type type)
{
    return (type == Type::Integer) ? s_kIntegerBytes
         : (type == Type::String)  ? s_kStringBytes
                                   : s_kRealBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::FormatValue
//
//  A function's value is where its definition is in the program and its
//  argument's address, neither of which is in the file.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ApplesoftVariables::FormatValue (Type type, std::span<const Byte> value, std::span<const Byte> file, Word origin)
{
    std::wstring  text;



    switch (type)
    {
        case Type::Integer:   text = std::to_wstring ((int16_t) ((value[0] << 8) | value[1]));  break;
        case Type::String:    text = FormatString (value, file, origin);                         break;
        case Type::Function:  text = L"Function";                                                break;
        case Type::Real:
        default:              text = FormatReal (value);                                         break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftVariables::FormatString
//
//  The length, then the address, low byte first, in the memory the file was
//  copied from. A control character is drawn as its picture.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ApplesoftVariables::FormatString (std::span<const Byte> value, std::span<const Byte> file, Word origin)
{
    HRESULT       hr        = S_OK;
    size_t        length    = value[0];
    size_t        address   = (size_t) value[1] | ((size_t) value[2] << 8);
    size_t        offset    = s_kHeaderBytes + address - origin;
    size_t        fileBytes = file.size();
    bool          isEmpty   = length == 0;
    std::wstring  text      = L"\"\"";



    BAIL_OUT_IF (isEmpty, S_OK);

    text = L"Not in the file";
    BAIL_OUT_IF (address < origin || offset + length > fileBytes, S_OK);

    text = L"\"";

    for (Byte c : file.subspan (offset, length))
    {
        c    &= 0x7F;
        text += (c < 0x20) ? (wchar_t) (s_kControlPictures + c) : (wchar_t) c;
    }

    text += L"\"";

Error:
    return text;
}