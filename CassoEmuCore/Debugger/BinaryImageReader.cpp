#include "Pch.h"

#include "Debugger/BinaryImageReader.h"

#include "Core/AppleSingleCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryImageReader::Read (
    std::span<const Byte>         content,
    std::optional<BinaryFormat>   explicitFormat,
    std::optional<Word>           address,
    BinaryImage                 & image,
    std::string                 & error)
{
    HRESULT  hr = S_OK;



    image        = BinaryImage();
    image.format = Detect (content, explicitFormat);

    switch (image.format)
    {
    case BinaryFormat::Dos33Binary: hr = ReadDos33       (content, address, image, error); break;
    case BinaryFormat::AppleSingle: hr = ReadAppleSingle (content, address, image, error); break;
    case BinaryFormat::IntelHex:    hr = ReadIntelHex    (content, address, image, error); break;
    case BinaryFormat::SRecord:     hr = ReadSRecord     (content, address, image, error); break;
    default:                        hr = ReadRaw         (content, address, image, error); break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::TryGetFormatName
//
//  The words BLOAD takes after the file name: DOS, RAW, HEX, SREC, AS.
//
////////////////////////////////////////////////////////////////////////////////

bool BinaryImageReader::TryGetFormatName (const std::string & word, BinaryFormat & format)
{
    std::string  upper (word);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    if      (upper == "DOS")  { format = BinaryFormat::Dos33Binary; }
    else if (upper == "RAW")  { format = BinaryFormat::Raw; }
    else if (upper == "HEX")  { format = BinaryFormat::IntelHex; }
    else if (upper == "SREC") { format = BinaryFormat::SRecord; }
    else if (upper == "AS")   { format = BinaryFormat::AppleSingle; }
    else                      { return false; }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::GetFormatName
//
////////////////////////////////////////////////////////////////////////////////

const char * BinaryImageReader::GetFormatName (BinaryFormat format)
{
    static constexpr const char * kNames[] = { "raw", "DOS 3.3 binary", "Intel HEX", "S-record", "AppleSingle" };



    return kNames[(int) format];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::Detect
//
//  An explicit choice wins. Otherwise the content decides between the three
//  self-describing formats, and raw is what remains.
//
////////////////////////////////////////////////////////////////////////////////

BinaryFormat BinaryImageReader::Detect (std::span<const Byte> content, std::optional<BinaryFormat> explicitFormat)
{
    if (explicitFormat.has_value())
    {
        return *explicitFormat;
    }

    if (AppleSingleCodec::IsAppleSingle (content)) { return BinaryFormat::AppleSingle; }
    if (LooksLikeIntelHex (content))               { return BinaryFormat::IntelHex; }
    if (LooksLikeSRecord (content))                { return BinaryFormat::SRecord; }

    return BinaryFormat::Raw;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::ReadRaw
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryImageReader::ReadRaw (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error)
{
    HRESULT  hr         = S_OK;
    bool     hasAddress = address.has_value();
    bool     hasBytes   = !content.empty();



    CBRFEx (hasAddress, E_INVALIDARG, error = "Raw bytes carry no address; give the address to load them at.");
    CBRFEx (hasBytes,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The file is empty.");

    AddBytes (image, *address, content);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::ReadDos33
//
//  A four-byte header: the address and the length, each low byte first. The
//  length caps what is loaded; a given address replaces the header's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryImageReader::ReadDos33 (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error)
{
    static constexpr int  kByteBits  = 8;
    HRESULT               hr         = S_OK;
    bool                  hasHeader  = content.size() >= kDosHeaderSize;
    Word                  start      = 0;
    size_t                length     = 0;
    bool                  isComplete = false;



    CBRFEx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "A DOS 3.3 binary file starts with a four-byte address and length header.");

    start      = (Word) (content[0] | (content[1] << kByteBits));
    length     = (size_t) (content[2] | (content[3] << kByteBits));
    isComplete = content.size() >= kDosHeaderSize + length;
    CBRFEx (isComplete, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            error = std::format ("The header says {} bytes but the file holds {}.", length, content.size() - kDosHeaderSize));

    AddBytes (image, address.value_or (start), content.subspan (kDosHeaderSize, length));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::ReadAppleSingle
//
//  The data fork, at the ProDOS aux type when the file has one, since that
//  is the load address of a BIN file; otherwise a given address is required.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryImageReader::ReadAppleSingle (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error)
{
    HRESULT          hr         = S_OK;
    AppleSingleFile  file;
    bool             hasAddress = false;



    hr = AppleSingleCodec::Decode (content, file, error);
    CHR (hr);

    hasAddress = address.has_value() || file.hasProDosInfo;
    CBRFEx (hasAddress, E_INVALIDARG, error = "The AppleSingle file has no ProDOS file info to take an address from; give one.");

    AddBytes (image, address.value_or ((Word) file.auxType), file.data);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::ReadIntelHex
//
//  Record types 00 (data), 01 (end), 02 and 04 (segment and linear base),
//  03 and 05 (start address). Every record's checksum must hold.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryImageReader::ReadIntelHex (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error)
{
    static constexpr size_t   kMinRecord    = 11;
    static constexpr int      kByteBits     = 8;
    static constexpr int      kSegmentShift = 4;
    static constexpr int      kLinearShift  = 16;
    HRESULT                   hr            = S_OK;
    std::vector<std::string>  lines;
    uint32_t                  base          = 0;
    bool                      noAddress     = !address.has_value();
    bool                      hasData       = false;



    CBRFEx (noAddress, E_INVALIDARG, error = "An Intel HEX file carries its own addresses; load it without one.");

    SplitLines (content, lines);

    for (const std::string & line : lines)
    {
        std::vector<Byte>  bytes;
        uint8_t            sum       = 0;
        size_t             count     = 0;
        uint32_t           offset    = 0;
        Byte               type      = 0;
        bool               isRecord  = line.size() >= kMinRecord && line[0] == ':';
        bool               isHex     = false;
        bool               isWhole   = false;
        bool               isSummed  = false;
        bool               isInRange = false;



        if (line.empty())
        {
            continue;
        }

        CBRFEx (isRecord, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "A line does not start with the : an Intel HEX record starts with.");

        isHex = TryReadHexBytes (line.substr (1), bytes);
        CBRFEx (isHex, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An Intel HEX record holds something other than hex digits.");

        count   = bytes[0];
        offset  = ((uint32_t) bytes[1] << kByteBits) | bytes[2];
        type    = bytes[3];
        isWhole = bytes.size() == count + 5;
        CBRFEx (isWhole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An Intel HEX record's length does not match its byte count.");

        for (Byte b : bytes)
        {
            sum = (uint8_t) (sum + b);
        }

        isSummed = sum == 0;
        CBRFEx (isSummed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("An Intel HEX record's checksum does not hold at offset ${:04X}.", offset));

        switch (type)
        {
        case 0x00:
            isInRange = base + offset + count <= 0x10000;
            CBRFEx (isInRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An Intel HEX record lies outside the 64 KB address space.");
            AddBytes (image, base + offset, std::span<const Byte> (bytes.data() + 4, count));
            hasData = true;
            break;

        case 0x01:
            break;

        case 0x02: base = (((uint32_t) bytes[4] << kByteBits) | bytes[5]) << kSegmentShift; break;
        case 0x04: base = (((uint32_t) bytes[4] << kByteBits) | bytes[5]) << kLinearShift;  break;

        case 0x03:
        case 0x05:
            image.entry = (Word) (((uint32_t) bytes[count + 2] << kByteBits) | bytes[count + 3]);
            break;

        default:
            CBRFEx (false, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = std::format ("Intel HEX record type {:02X} is not one this reader knows.", type));
        }
    }

    CBRFEx (hasData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The Intel HEX file holds no data records.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::ReadSRecord
//
//  S1, S2 and S3 carry data behind a 16-, 24- or 32-bit address; S7, S8 and
//  S9 carry the start address; S0 and S5 are read past. The checksum is the
//  ones' complement of the sum of the count, address and data bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BinaryImageReader::ReadSRecord (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error)
{
    static constexpr size_t  kMinRecord = 10;
    static constexpr int     kByteBits  = 8;
    HRESULT                  hr         = S_OK;
    std::vector<std::string> lines;
    bool                     noAddress  = !address.has_value();
    bool                     hasData    = false;



    CBRFEx (noAddress, E_INVALIDARG, error = "An S-record file carries its own addresses; load it without one.");

    SplitLines (content, lines);

    for (const std::string & line : lines)
    {
        std::vector<Byte>  bytes;
        uint8_t            sum           = 0;
        size_t             addressSize   = 0;
        uint32_t           recordAddress = 0;
        size_t             dataCount     = 0;
        bool               isRecord      = line.size() >= kMinRecord && (line[0] == 'S' || line[0] == 's') && isdigit ((unsigned char) line[1]);
        bool               isHex         = false;
        bool               isWhole       = false;
        bool               isSummed      = false;
        bool               isInRange     = false;
        char               type          = '\0';



        if (line.empty())
        {
            continue;
        }

        CBRFEx (isRecord, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "A line does not start with the S and digit an S-record starts with.");

        type  = line[1];
        isHex = TryReadHexBytes (line.substr (2), bytes);
        CBRFEx (isHex, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An S-record holds something other than hex digits.");

        isWhole = bytes.size() == (size_t) bytes[0] + 1;
        CBRFEx (isWhole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An S-record's length does not match its byte count.");

        for (size_t i = 0; i + 1 < bytes.size(); ++i)
        {
            sum = (uint8_t) (sum + bytes[i]);
        }

        isSummed = (uint8_t) ~sum == bytes.back();
        CBRFEx (isSummed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An S-record's checksum does not hold.");

        switch (type)
        {
        case '1': case '9': addressSize = 2; break;
        case '2': case '8': addressSize = 3; break;
        case '3': case '7': addressSize = 4; break;
        default:            addressSize = 0; break;
        }

        for (size_t i = 0; i < addressSize; ++i)
        {
            recordAddress = (recordAddress << kByteBits) | bytes[1 + i];
        }

        dataCount = bytes.size() - 2 - addressSize;

        if (type == '1' || type == '2' || type == '3')
        {
            isInRange = recordAddress + dataCount <= 0x10000;
            CBRFEx (isInRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "An S-record lies outside the 64 KB address space.");
            AddBytes (image, recordAddress, std::span<const Byte> (bytes.data() + 1 + addressSize, dataCount));
            hasData = true;
        }
        else if (type == '7' || type == '8' || type == '9')
        {
            image.entry = (Word) recordAddress;
        }
    }

    CBRFEx (hasData, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The S-record file holds no data records.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::LooksLikeIntelHex
//
//  The first line, whole and checksummed, decides.
//
////////////////////////////////////////////////////////////////////////////////

bool BinaryImageReader::LooksLikeIntelHex (std::span<const Byte> content)
{
    std::string        line = FirstLine (content);
    std::vector<Byte>  bytes;
    uint8_t            sum  = 0;



    if (line.size() < 11 || line[0] != ':' || !TryReadHexBytes (line.substr (1), bytes) || bytes.size() != (size_t) bytes[0] + 5)
    {
        return false;
    }

    for (Byte b : bytes)
    {
        sum = (uint8_t) (sum + b);
    }

    return sum == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::LooksLikeSRecord
//
//  The first line, whole and checksummed, decides.
//
////////////////////////////////////////////////////////////////////////////////

bool BinaryImageReader::LooksLikeSRecord (std::span<const Byte> content)
{
    std::string        line = FirstLine (content);
    std::vector<Byte>  bytes;
    uint8_t            sum  = 0;



    if (line.size() < 10 || (line[0] != 'S' && line[0] != 's') || !isdigit ((unsigned char) line[1]) ||
        !TryReadHexBytes (line.substr (2), bytes) || bytes.size() != (size_t) bytes[0] + 1)
    {
        return false;
    }

    for (size_t i = 0; i + 1 < bytes.size(); ++i)
    {
        sum = (uint8_t) (sum + bytes[i]);
    }

    return (uint8_t) ~sum == bytes.back();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::FirstLine
//
////////////////////////////////////////////////////////////////////////////////

std::string BinaryImageReader::FirstLine (std::span<const Byte> content)
{
    std::vector<std::string>  lines;



    SplitLines (content, lines);

    for (const std::string & line : lines)
    {
        if (!line.empty())
        {
            return line;
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::SplitLines
//
//  Lines with their ends trimmed; a byte outside printable ASCII makes the
//  content not a text format, which one unreadable line reports.
//
////////////////////////////////////////////////////////////////////////////////

void BinaryImageReader::SplitLines (std::span<const Byte> content, std::vector<std::string> & lines)
{
    std::string  line;



    for (Byte b : content)
    {
        if (b == '\n')
        {
            lines.push_back (line);
            line.clear();
        }
        else if (b != '\r' && b != ' ' && b != '\t')
        {
            line += (char) b;
        }
    }

    if (!line.empty())
    {
        lines.push_back (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::TryReadHexBytes
//
////////////////////////////////////////////////////////////////////////////////

bool BinaryImageReader::TryReadHexBytes (const std::string & text, std::vector<Byte> & bytes)
{
    static constexpr int  kNibble = 4;



    bytes.clear();

    if (text.size() % 2 != 0)
    {
        return false;
    }

    for (size_t i = 0; i < text.size(); i += 2)
    {
        int  high = isxdigit ((unsigned char) text[i])     ? (int) std::stoul (text.substr (i, 1), nullptr, 16)     : -1;
        int  low  = isxdigit ((unsigned char) text[i + 1]) ? (int) std::stoul (text.substr (i + 1, 1), nullptr, 16) : -1;



        if (high < 0 || low < 0)
        {
            return false;
        }

        bytes.push_back ((Byte) ((high << kNibble) | low));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader::AddBytes
//
//  Bytes that follow on from the last segment join it; any other address
//  starts a new one.
//
////////////////////////////////////////////////////////////////////////////////

void BinaryImageReader::AddBytes (BinaryImage & image, uint32_t address, std::span<const Byte> bytes)
{
    BinarySegment  segment;



    if (bytes.empty())
    {
        return;
    }

    if (!image.segments.empty() && image.segments.back().address + image.segments.back().bytes.size() == address)
    {
        image.segments.back().bytes.insert (image.segments.back().bytes.end(), bytes.begin(), bytes.end());
        return;
    }

    segment.address = (Word) address;
    segment.bytes.assign (bytes.begin(), bytes.end());
    image.segments.push_back (segment);
}
