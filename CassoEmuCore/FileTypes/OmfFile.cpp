#include "Pch.h"

#include "FileTypes/OmfFile.h"
#include "FileTypes/AppleWorks.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Reader::ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte OmfFile::Reader::ReadByte()
{
    if (at >= bytes.size())
    {
        failed = true;
        return 0;
    }

    return bytes[at++];
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Reader::ReadNumber
//
//  Low byte first, as the 65816 orders it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t OmfFile::Reader::ReadNumber (size_t width)
{
    uint32_t  value = 0;



    for (size_t i = 0; i < width; i++)
    {
        uint32_t  b = ReadByte();

        if (i < 4)
        {
            value |= b << (8 * i);
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Reader::ReadLabel
//
//  Trailing spaces, which pad a fixed-length label, are dropped.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring OmfFile::Reader::ReadLabel()
{
    size_t        length = (labelLen == 0) ? ReadByte() : labelLen;
    std::wstring  text;



    if (failed || at + length > bytes.size())
    {
        failed = true;
        return std::wstring();
    }

    text = TextCharset::DecodeLine (bytes.subspan (at, length), Charset::MacOsRoman);
    at  += length;

    while (!text.empty() && text.back() == L' ')
    {
        text.pop_back();
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Reader::ReadHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring OmfFile::Reader::ReadHex (size_t count)
{
    std::wstring  text;



    for (size_t i = 0; i < count && !failed; i++)
    {
        text += std::format (L"{}{:02X}", text.empty() ? L"" : L" ", ReadByte());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool OmfFile::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.IsProDos() && facts.type >= kFirstType && facts.type <= kLastType && Check (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Check
//
//  The first segment's header reads.
//
////////////////////////////////////////////////////////////////////////////////

bool OmfFile::Check (std::span<const Byte> bytes)
{
    Header  header;



    return ReadHeader (bytes, header);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::DescribeKind
//
//  The low five bits are the segment's type, the high byte its attributes.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring OmfFile::DescribeKind (Word kind)
{
    static constexpr std::pair<Word, const wchar_t *>  kAttributes[] =
    {
        { 0x4000, L"private" },                 { 0x2000, L"position independent" },
        { 0x1000, L"skip" },                    { 0x0800, L"reload" },
        { 0x0400, L"absolute bank" },           { 0x0200, L"no special memory" },
        { 0x0100, L"bank relative" },
    };



    std::wstring  text;



    switch (kind & 0x1F)
    {
        case 0x00: text = L"code";                       break;
        case 0x01: text = L"data";                       break;
        case 0x02: text = L"jump table";                 break;
        case 0x04: text = L"pathname";                   break;
        case 0x08: text = L"library dictionary";         break;
        case 0x10: text = L"initialization";             break;
        case 0x12: text = L"direct page and stack";      break;
        default:   text = std::format (L"type ${:02X}", kind & 0x1F); break;
    }

    text += (kind & 0x8000) ? L", dynamic" : L", static";

    for (const auto & [bit, name] : kAttributes)
    {
        if (kind & bit)
        {
            text += std::wstring (L", ") + name;
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::ReadHeader
//
//  Version 1's kind byte has its attributes in the top three bits, which
//  map to the high three of version 2's word. Version 1 has no load name.
//
////////////////////////////////////////////////////////////////////////////////

bool OmfFile::ReadHeader (std::span<const Byte> bytes, Header & outHeader)
{
    Reader  reader;



    if (bytes.size() < kMinHeader)
    {
        return false;
    }

    outHeader.labelLen  = bytes[13];
    outHeader.numberLen = bytes[14];
    outHeader.version   = bytes[15];
    outHeader.length    = AppleWorks::ReadLong (bytes, 0);
    outHeader.reserved  = AppleWorks::ReadLong (bytes, 4);
    outHeader.memory    = AppleWorks::ReadLong (bytes, 8);
    outHeader.bankSize  = AppleWorks::ReadLong (bytes, 16);
    outHeader.kind      = AppleWorks::ReadWord (bytes, 20);
    outHeader.org       = AppleWorks::ReadLong (bytes, 24);
    outHeader.align     = AppleWorks::ReadLong (bytes, 28);
    outHeader.number    = AppleWorks::ReadWord (bytes, 34);
    outHeader.entry     = AppleWorks::ReadLong (bytes, 36);
    outHeader.nameAt    = AppleWorks::ReadWord (bytes, 40);
    outHeader.dataAt    = AppleWorks::ReadWord (bytes, 42);

    if (outHeader.version == 1)
    {
        outHeader.length *= kBlockBytes;
        outHeader.kind    = (Word) ((bytes[12] & 0x1F) | ((bytes[12] & 0xE0) << 8));
    }

    if ((outHeader.version != 1 && outHeader.version != 2) || outHeader.numberLen != 4 || bytes[32] != 0)
    {
        return false;
    }

    if (outHeader.nameAt < kMinHeader || outHeader.dataAt <= outHeader.nameAt || outHeader.length < outHeader.dataAt || outHeader.length > bytes.size())
    {
        return false;
    }

    reader.bytes    = bytes.first (outHeader.dataAt);
    reader.at       = outHeader.nameAt;
    reader.labelLen = outHeader.labelLen;

    if (outHeader.version >= 2)
    {
        reader.labelLen    = kLoadName;
        outHeader.loadName = reader.ReadLabel();
        reader.labelLen    = outHeader.labelLen;
    }

    outHeader.name = reader.ReadLabel();

    return !reader.failed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::ReadExpression
//
//  An expression is postfix: operands and operators to a $00.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring OmfFile::ReadExpression (Reader & reader)
{
    static constexpr const wchar_t *  kOperators[] =
    {
        L"+", L"-", L"*", L"/", L"mod", L"neg", L"shift", L"and", L"or", L"eor", L"not",
        L"<=", L">=", L"<>", L"<", L">", L"=", L"&", L"|", L"^", L"~",
    };



    std::wstring  text;



    while (!reader.failed)
    {
        Byte          op = reader.ReadByte();
        std::wstring  term;

        if (op == 0x00)
        {
            break;
        }

        if (op >= 0x01 && op <= 0x15)
        {
            term = kOperators[op - 1];
        }
        else
        {
            switch (op)
            {
                case 0x80: term = L"*";                                                       break;
                case 0x81: term = std::format (L"${:X}", reader.ReadNumber (reader.numberLen)); break;
                case 0x82: term = L"weak(" + reader.ReadLabel() + L")";                      break;
                case 0x83: term = reader.ReadLabel();                                         break;
                case 0x84: term = L"len(" + reader.ReadLabel() + L")";                       break;
                case 0x85: term = L"type(" + reader.ReadLabel() + L")";                      break;
                case 0x86: term = L"count(" + reader.ReadLabel() + L")";                     break;
                case 0x87: term = std::format (L"rel(${:X})", reader.ReadNumber (reader.numberLen)); break;

                default:
                    reader.failed = true;
                    term          = std::format (L"?${:02X}", op);
                    break;
            }
        }

        text += (text.empty() ? L"" : L" ") + term;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::AppendHex
//
////////////////////////////////////////////////////////////////////////////////

void OmfFile::AppendHex (std::span<const Byte> bytes, const std::wstring & indent, std::vector<std::wstring> & outLines)
{
    for (size_t start = 0; start < bytes.size(); start += kHexPerLine)
    {
        std::wstring  line = indent + std::format (L"{:04X}:", start);

        for (size_t i = start; i < std::min (bytes.size(), start + kHexPerLine); i++)
        {
            line += std::format (L" {:02X}", bytes[i]);
        }

        outLines.push_back (std::move (line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::DecodeRecords
//
//  A GLOBAL, LOCAL, EQU or GEQU record's length is a byte in version 1 and
//  a word after. An opcode not in the appendix ends the segment's listing.
//
////////////////////////////////////////////////////////////////////////////////

void OmfFile::DecodeRecords (const Header & header, std::span<const Byte> segment, std::vector<std::wstring> & outLines)
{
    static constexpr const wchar_t *  kIndent = L"    ";



    Reader  reader;
    size_t  numberLen = header.numberLen;
    size_t  lengthLen = (header.version == 1) ? 1 : 2;



    reader.bytes     = segment;
    reader.at        = header.dataAt;
    reader.labelLen  = header.labelLen;
    reader.numberLen = header.numberLen;

    while (!reader.failed && reader.at < segment.size())
    {
        Byte          op = reader.ReadByte();
        std::wstring  line;

        if (op == 0x00)
        {
            outLines.push_back (L"  END");
            return;
        }

        if (op <= 0xDF)
        {
            size_t  count = std::min<size_t> (op, segment.size() - reader.at);

            outLines.push_back (std::format (L"  CONST {} bytes", count));
            AppendHex (segment.subspan (reader.at, count), kIndent, outLines);
            reader.at += count;
            continue;
        }

        switch (op)
        {
            case 0xE0: line = std::format (L"  ALIGN ${:X}", reader.ReadNumber (numberLen)); break;
            case 0xE1: line = std::format (L"  ORG ${:X}", reader.ReadNumber (numberLen));   break;

            case 0xE2:
            {
                Byte      count  = reader.ReadByte();
                Byte      shift  = reader.ReadByte();
                uint32_t  offset = reader.ReadNumber (numberLen);

                line = std::format (L"  RELOC {} bytes, shift {}, at ${:X}, value ${:X}", count, (int8_t) shift, offset, reader.ReadNumber (numberLen));
                break;
            }

            case 0xE3:
            {
                Byte      count   = reader.ReadByte();
                Byte      shift   = reader.ReadByte();
                uint32_t  offset  = reader.ReadNumber (numberLen);
                uint32_t  file    = reader.ReadNumber (2);
                uint32_t  segNum  = reader.ReadNumber (2);

                line = std::format (L"  INTERSEG {} bytes, shift {}, at ${:X}, file {}, segment {}, value ${:X}", count, (int8_t) shift, offset, file, segNum, reader.ReadNumber (numberLen));
                break;
            }

            case 0xE4: line = L"  USING " + reader.ReadLabel();  break;
            case 0xE5: line = L"  STRONG " + reader.ReadLabel(); break;

            case 0xE6:
            case 0xE7:
            case 0xEF:
            case 0xF0:
            {
                const wchar_t *  name    = (op == 0xE6) ? L"GLOBAL" : (op == 0xE7) ? L"GEQU" : (op == 0xEF) ? L"LOCAL" : L"EQU";
                std::wstring     label   = reader.ReadLabel();
                uint32_t         length  = reader.ReadNumber (lengthLen);
                Byte             type    = reader.ReadByte();
                Byte             priv    = reader.ReadByte();

                line = std::format (L"  {} {}, length {}, type '{}'{}", name, label, length, (wchar_t) (type & 0x7F), priv ? L", private" : L"");

                if (op == 0xE7 || op == 0xF0)
                {
                    line += L" = " + ReadExpression (reader);
                }

                break;
            }

            case 0xE8:
            {
                uint32_t  start = reader.ReadNumber (numberLen);

                line = std::format (L"  MEM ${:X} to ${:X}", start, reader.ReadNumber (numberLen));
                break;
            }

            case 0xEB:
            case 0xEC:
            case 0xED:
            case 0xF3:
            {
                const wchar_t *  name  = (op == 0xEB) ? L"EXPR" : (op == 0xEC) ? L"ZEXPR" : (op == 0xED) ? L"BEXPR" : L"LEXPR";
                Byte             count = reader.ReadByte();

                line = std::format (L"  {} {} bytes: {}", name, count, ReadExpression (reader));
                break;
            }

            case 0xEE:
            {
                Byte      count  = reader.ReadByte();
                uint32_t  offset = reader.ReadNumber (numberLen);

                line = std::format (L"  RELEXPR {} bytes, from ${:X}: {}", count, offset, ReadExpression (reader));
                break;
            }

            case 0xF1: line = std::format (L"  DS {} bytes", reader.ReadNumber (numberLen)); break;

            case 0xF2:
            {
                size_t  count = std::min<size_t> (reader.ReadNumber (numberLen), segment.size() - std::min (reader.at, segment.size()));

                outLines.push_back (std::format (L"  LCONST {} bytes", count));
                AppendHex (segment.subspan (reader.at, count), kIndent, outLines);
                reader.at += count;
                continue;
            }

            case 0xF4:
            {
                uint32_t  number = reader.ReadNumber (2);
                uint32_t  offset = reader.ReadNumber (numberLen);

                line = std::format (L"  ENTRY segment {}, at ${:X}, {}", number, offset, reader.ReadLabel());
                break;
            }

            case 0xF5:
            {
                Byte      count  = reader.ReadByte();
                Byte      shift  = reader.ReadByte();
                uint32_t  offset = reader.ReadNumber (2);

                line = std::format (L"  cRELOC {} bytes, shift {}, at ${:X}, value ${:X}", count, (int8_t) shift, offset, reader.ReadNumber (2));
                break;
            }

            case 0xF6:
            {
                Byte      count   = reader.ReadByte();
                Byte      shift   = reader.ReadByte();
                uint32_t  offset  = reader.ReadNumber (2);
                Byte      segNum  = reader.ReadByte();

                line = std::format (L"  cINTERSEG {} bytes, shift {}, at ${:X}, segment {}, value ${:X}", count, (int8_t) shift, offset, segNum, reader.ReadNumber (2));
                break;
            }

            case 0xF7:
            {
                size_t  count = reader.ReadNumber (4);
                Byte    type  = reader.ReadByte();
                size_t  rest  = (count > 0) ? std::min (count - 1, segment.size() - std::min (reader.at, segment.size())) : 0;

                outLines.push_back (std::format (L"  SUPER {}, {} bytes", (type == 0) ? std::wstring (L"RELOC2") : (type == 1) ? std::wstring (L"RELOC3") : std::format (L"INTERSEG{}", type - 1), rest));
                AppendHex (segment.subspan (reader.at, rest), kIndent, outLines);
                reader.at += rest;
                continue;
            }

            default:
                outLines.push_back (std::format (L"  Opcode ${:02X} isn't an OMF record; the rest of the segment isn't listed.", op));
                return;
        }

        outLines.push_back (std::move (line));
    }

    outLines.push_back (L"  The segment ends before its END record.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OmfFile::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr    = S_OK;
    size_t   at    = 0;
    size_t   count = 0;
    bool     read  = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    while (at < bytes.size())
    {
        Header  header;

        read = ReadHeader (bytes.subspan (at), header) && header.length > 0;

        if (!read)
        {
            break;
        }

        count++;
        outFile.lines.push_back (std::format (L"Segment {}: \"{}\", load name \"{}\", {}, version {}",
                                              header.number,
                                              header.name,
                                              header.loadName,
                                              DescribeKind (header.kind),
                                              (header.version == 1) ? L"1" : (header.nameAt >= 0x30) ? L"2.1" : L"2.0"));
        outFile.lines.push_back (std::format (L"  Length ${:X}, reserved ${:X}, bank size ${:X}, alignment ${:X}, origin ${:X}, entry ${:X}",
                                              header.memory, header.reserved, header.bankSize, header.align, header.org, header.entry));

        DecodeRecords (header, bytes.subspan (at, header.length), outFile.lines);
        outFile.lines.push_back (std::wstring());

        at += header.length;
    }

    read = count > 0;
    CBRFEx (read, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file's first segment header doesn't read.");

    if (at < bytes.size())
    {
        outFile.notes.push_back (std::format (L"The {} bytes after segment {} aren't a segment.", bytes.size() - at, count));
    }

Error:
    return hr;
}
