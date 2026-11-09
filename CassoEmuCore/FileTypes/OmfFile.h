#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OmfFile
//
//  An Object Module Format file, ProDOS types $B1 to $BE, as the OMF
//  appendix of the Apple IIgs GS/OS Reference sets it out: segments one
//  after another, each a header, then opcode bytes and their operands, to
//  an END record. Version 1 counts a segment in 512-byte blocks and has a
//  kind byte; versions 2.0 and 2.1 count it in bytes and have a kind word,
//  and 2.1 adds TEMPORG. A label is a Pascal string when
//  the header's label length is 0, otherwise that many bytes; a number is
//  the header's number length in bytes.
//
////////////////////////////////////////////////////////////////////////////////

class OmfFile
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     Check   (std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static std::wstring  DescribeKind (Word kind);

private:
    static constexpr Byte    kFirstType   = 0xB1;
    static constexpr Byte    kLastType    = 0xBE;
    static constexpr size_t  kMinHeader   = 0x2C;
    static constexpr size_t  kBlockBytes  = 512;
    static constexpr size_t  kHexPerLine  = 16;
    static constexpr size_t  kLoadName    = 10;

    struct Header
    {
        size_t        length     = 0;   // the segment's bytes in the file
        uint32_t      reserved   = 0;
        uint32_t      memory     = 0;
        Byte          labelLen   = 0;
        Byte          numberLen  = 4;
        Byte          version    = 2;
        uint32_t      bankSize   = 0;
        Word          kind       = 0;
        uint32_t      org        = 0;
        uint32_t      align      = 0;
        Word          number     = 0;
        uint32_t      entry      = 0;
        size_t        nameAt     = 0;
        size_t        dataAt     = 0;
        std::wstring  loadName;
        std::wstring  name;
    };

    //  A segment's bytes with a cursor, each read checked against the end.
    struct Reader
    {
        std::span<const Byte>  bytes;
        size_t                 at        = 0;
        Byte                   labelLen  = 0;
        Byte                   numberLen = 4;
        bool                   failed    = false;

        Byte          ReadByte   ();
        uint32_t      ReadNumber (size_t width);
        std::wstring  ReadLabel  ();
        std::wstring  ReadHex    (size_t count);
    };

    static bool          ReadHeader       (std::span<const Byte> bytes, Header & outHeader);
    static std::wstring  ReadExpression   (Reader & reader);
    static void          DecodeRecords    (const Header & header, std::span<const Byte> segment, std::vector<std::wstring> & outLines);
    static void          AppendHex        (std::span<const Byte> bytes, const std::wstring & indent, std::vector<std::wstring> & outLines);
};
