#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"

class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile
//
//  An Apple Pascal codefile, Pascal kind 2 or ProDOS type $02. Block 0 is
//  the segment dictionary, sixteen slots in parallel arrays: the block and
//  length of the code, the name, the kind, the block of a unit's interface
//  text, and a word with the segment number, machine type and version. A
//  slot with a block and length of 0 is empty.
//
//  A segment ends with its procedure dictionary: the last word is the
//  segment number and the procedure count, and below it is a pointer for
//  each procedure, each the distance back from the pointer to the
//  procedure's attribute table. A table with a procedure number of 0 is a
//  6502 procedure, and its code ends at the lowest of four relocation
//  tables. The dictionary layout is from CiderPress II's notes, under its
//  Apache license, and the rest from the Apple Pascal Operating System
//  Reference Manual.
//
////////////////////////////////////////////////////////////////////////////////

class PascalCodefile
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    struct Segment
    {
        int          slot      = 0;
        size_t       block     = 0;
        size_t       length    = 0;
        std::string  name;
        Word         kind      = 0;
        Word         textBlock = 0;
        Byte         number    = 0;
        Byte         machine   = 0;
        Byte         version   = 0;
    };

    static constexpr Byte    kPascalCode       = 2;
    static constexpr Byte    kProDosCode       = 0x02;
    static constexpr size_t  kBlockBytes       = 512;
    static constexpr size_t  kDictionaryBytes  = 1024;
    static constexpr int     kSlots            = 16;
    static constexpr size_t  kNameBytes        = 8;
    static constexpr size_t  kNamesAt          = 0x40;
    static constexpr size_t  kKindsAt          = 0xC0;
    static constexpr size_t  kTextBlocksAt     = 0xE0;
    static constexpr size_t  kInfoAt           = 0x100;
    static constexpr size_t  kIntrinsicsAt     = 0x120;
    static constexpr int     kIntrinsicUnits   = 32;
    static constexpr Word    kDataSegment      = 7;
    static constexpr Byte    kMachineUnknown   = 0;
    static constexpr Byte    kMachinePCodeHigh = 1;
    static constexpr Byte    kMachinePCodeLow  = 2;
    static constexpr Byte    kMachine6502      = 7;
    static constexpr size_t  kPCodeTableBytes  = 8;
    static constexpr int     kRelocationTables = 4;
    static constexpr size_t  kHexRowBytes      = 16;

    static size_t             ReadWord        (std::span<const Byte> bytes, size_t at);
    static Segment            ReadSegment     (std::span<const Byte> bytes, int slot);
    static const char *       GetKindName     (Word kind);
    static std::string        GetMachineName  (Byte machine);
    static const Microcode *  GetNmosTable    ();
    static void               AddLine         (DecodedFile & ioFile, const std::string & line);
    static void               ListHeader      (std::span<const Byte> bytes, int segments, DecodedFile & ioFile);
    static void               ListSegment     (std::span<const Byte> bytes, const Segment & segment, DecodedFile & ioFile);
    static bool               ListProcedures  (std::span<const Byte> code, DecodedFile & ioFile);
    static void               ListPCode       (std::span<const Byte> code, int number, size_t attributes, DecodedFile & ioFile);
    static void               ListNative      (std::span<const Byte> code, int number, size_t attributes, DecodedFile & ioFile);
    static void               ListHex         (std::span<const Byte> code, DecodedFile & ioFile);
};
