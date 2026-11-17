#include "Pch.h"

#include "FileTypes/PascalCodefile.h"
#include "Core/TextEncoding.h"
#include "Cpu.h"
#include "Disassembler.h"
#include "PCodeDisassembler.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool PascalCodefile::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = (facts.IsPascal() && facts.type == kPascalCode) || facts.HasProDosType (kProDosCode);



    return typed && bytes.size() >= kDictionaryBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

size_t PascalCodefile::ReadWord (std::span<const Byte> bytes, size_t at)
{
    return (size_t) bytes[at] | ((size_t) bytes[at + 1] << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ReadSegment
//
////////////////////////////////////////////////////////////////////////////////

PascalCodefile::Segment PascalCodefile::ReadSegment (std::span<const Byte> bytes, int slot)
{
    Segment  segment;
    size_t   info = ReadWord (bytes, kInfoAt + slot * 2);



    segment.slot      = slot;
    segment.block     = ReadWord (bytes, slot * 4);
    segment.length    = ReadWord (bytes, slot * 4 + 2);
    segment.kind      = (Word) ReadWord (bytes, kKindsAt + slot * 2);
    segment.textBlock = (Word) ReadWord (bytes, kTextBlocksAt + slot * 2);
    segment.number    = (Byte) (info & 0xFF);
    segment.machine   = (Byte) ((info >> 8) & 0x0F);
    segment.version   = (Byte) ((info >> 13) & 0x07);

    for (size_t i = 0; i < kNameBytes; i++)
    {
        char  c = (char) (bytes[kNamesAt + slot * kNameBytes + i] & 0x7F);

        segment.name += (c >= 0x20 && c < 0x7F) ? c : '.';
    }

    while (!segment.name.empty() && segment.name.back() == ' ')
    {
        segment.name.pop_back();
    }

    return segment;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::GetKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * PascalCodefile::GetKindName (Word kind)
{
    static constexpr const char *  kKinds[] =
    {
        "LINKED", "HOSTSEG", "SEGPROC", "UNITSEG", "SEPRTSEG", "UNLINKED_INTRINS", "LINKED_INTRINS", "DATASEG",
    };



    return (kind < std::size (kKinds)) ? kKinds[kind] : "unknown kind";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::GetMachineName
//
////////////////////////////////////////////////////////////////////////////////

std::string PascalCodefile::GetMachineName (Byte machine)
{
    switch (machine)
    {
        case kMachineUnknown:    return "unidentified";
        case kMachinePCodeHigh:  return "p-code, high byte first";
        case kMachinePCodeLow:   return "p-code, low byte first";
        case kMachine6502:       return "6502";
        default:                 break;
    }

    return std::format ("machine code, type {}", machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::GetNmosTable
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * PascalCodefile::GetNmosTable()
{
    static Cpu  cpu;



    return cpu.GetInstructionSet();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::AddLine
//
////////////////////////////////////////////////////////////////////////////////

void PascalCodefile::AddLine (DecodedFile & ioFile, const std::string & line)
{
    ioFile.lines.push_back (TextEncoding::NarrowToWide (line));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ListHeader
//
////////////////////////////////////////////////////////////////////////////////

void PascalCodefile::ListHeader (std::span<const Byte> bytes, int segments, DecodedFile & ioFile)
{
    std::string  units;



    for (int unit = 0; unit < kIntrinsicUnits; unit++)
    {
        if ((bytes[kIntrinsicsAt + unit / 8] >> (unit % 8)) & 1)
        {
            units += ' ';
            units += std::to_string (unit);
        }
    }

    AddLine (ioFile, std::format ("Apple Pascal codefile, segments: {}", segments));
    AddLine (ioFile, "Intrinsic units:" + (units.empty() ? std::string (" none") : units));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ListHex
//
////////////////////////////////////////////////////////////////////////////////

void PascalCodefile::ListHex (std::span<const Byte> code, DecodedFile & ioFile)
{
    for (size_t at = 0; at < code.size(); at += kHexRowBytes)
    {
        std::string  line  = std::format ("  {:04X}:", at);
        std::string  chars;
        size_t       count = (std::min) (kHexRowBytes, code.size() - at);

        for (size_t i = 0; i < kHexRowBytes; i++)
        {
            char  c = (i < count) ? (char) (code[at + i] & 0x7F) : ' ';

            line  += (i < count) ? std::format (" {:02X}", code[at + i]) : std::string ("   ");
            chars += (c >= 0x20 && c < 0x7F) ? c : '.';
        }

        AddLine (ioFile, line + "  " + chars.substr (0, count));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ListPCode
//
//  Below the attribute table are the distances back to the first
//  instruction and to the exit code, the size of the parameters and the
//  size of the data, and below those the jump table. The listing ends with
//  the return of the exit code.
//
////////////////////////////////////////////////////////////////////////////////

void PascalCodefile::ListPCode (std::span<const Byte> code, int number, size_t attributes, DecodedFile & ioFile)
{
    std::vector<PCodeLine>  lines;
    size_t                  enterAt = attributes - 2;
    size_t                  exitAt  = attributes - 4;
    size_t                  end     = attributes - kPCodeTableBytes;
    size_t                  enter   = 0;
    size_t                  exit    = 0;



    if (attributes < kPCodeTableBytes || ReadWord (code, enterAt) > enterAt || ReadWord (code, exitAt) > exitAt)
    {
        AddLine (ioFile, std::format ("  Procedure {}: the attribute table is damaged", number));
        return;
    }

    enter = enterAt - ReadWord (code, enterAt);
    exit  = exitAt  - ReadWord (code, exitAt);

    AddLine (ioFile, std::format ("  Procedure {}: lex level {}, parameters {} bytes, data {} bytes",
                                  number, (int) (int8_t) code[attributes + 1], ReadWord (code, attributes - 6), ReadWord (code, end)));

    PCodeDisassembler::Disassemble (code, enter, end, attributes, lines);

    for (const PCodeLine & line : lines)
    {
        AddLine (ioFile, "    " + PCodeDisassembler::FormatLine (line));

        if (line.offset >= exit && PCodeDisassembler::IsReturn (line))
        {
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ListNative
//
//  Below the attribute table is the distance back to the first instruction,
//  and below that four relocation tables, each a count and that many words
//  below it. The code ends at the lowest table.
//
////////////////////////////////////////////////////////////////////////////////

void PascalCodefile::ListNative (std::span<const Byte> code, int number, size_t attributes, DecodedFile & ioFile)
{
    std::vector<DisassembledLine>  lines;
    size_t                         at      = attributes - 2;
    size_t                         enter   = 0;
    bool                           damaged = attributes < 2;



    if (!damaged)
    {
        damaged = ReadWord (code, at) > at;
        enter   = at - ReadWord (code, at);
    }

    for (int table = 0; table < kRelocationTables && !damaged; table++)
    {
        size_t  words = 0;

        damaged = at < 2;

        if (!damaged)
        {
            at     -= 2;
            words   = ReadWord (code, at);
            damaged = words * 2 > at;
            at     -= damaged ? 0 : words * 2;
        }
    }

    if (damaged || enter > at)
    {
        AddLine (ioFile, std::format ("  Procedure {}: the attribute table is damaged", number));
        return;
    }

    AddLine (ioFile, std::format ("  Procedure {}: 6502", number));

    Disassembler::Disassemble (code.subspan (enter, at - enter), (Word) enter, GetNmosTable(), lines);

    for (const DisassembledLine & line : lines)
    {
        AddLine (ioFile, "    " + Disassembler::FormatLine (line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ListProcedures
//
//  False when the procedure dictionary doesn't fit the segment. A pointer
//  of 0 is a procedure the segment doesn't have.
//
////////////////////////////////////////////////////////////////////////////////

bool PascalCodefile::ListProcedures (std::span<const Byte> code, DecodedFile & ioFile)
{
    size_t  count = (code.size() >= 2) ? code[code.size() - 1] : 0;
    size_t  top   = code.size() - 2;



    if (count == 0 || count * 2 > top)
    {
        return false;
    }

    for (size_t i = 1; i <= count; i++)
    {
        if (ReadWord (code, top - i * 2) > top - i * 2)
        {
            return false;
        }
    }

    for (size_t i = 1; i <= count; i++)
    {
        size_t  pointer    = top - i * 2;
        size_t  attributes = pointer - ReadWord (code, pointer);

        AddLine (ioFile, "");

        if (attributes == pointer)
        {
            AddLine (ioFile, std::format ("  Procedure {}: not in the segment", i));
        }
        else if (code[attributes] == 0)
        {
            ListNative (code, (int) i, attributes, ioFile);
        }
        else
        {
            ListPCode (code, (int) i, attributes, ioFile);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::ListSegment
//
//  A data segment has a length and no bytes in the file. A segment for
//  another machine, or one whose procedure dictionary doesn't fit, is
//  listed in hex.
//
////////////////////////////////////////////////////////////////////////////////

void PascalCodefile::ListSegment (std::span<const Byte> bytes, const Segment & segment, DecodedFile & ioFile)
{
    size_t                 start   = segment.block * kBlockBytes;
    bool                   listed  = segment.machine == kMachineUnknown || segment.machine == kMachinePCodeLow || segment.machine == kMachine6502;
    std::span<const Byte>  code;



    AddLine (ioFile, "");
    AddLine (ioFile, std::format ("Segment {}: {} ({})", segment.slot, segment.name, GetKindName (segment.kind)));
    AddLine (ioFile, std::format ("  Block {}, {} bytes, segment number {}, version {}", segment.block, segment.length, segment.number, segment.version));
    AddLine (ioFile, "  Machine type: " + GetMachineName (segment.machine));

    if (segment.textBlock != 0)
    {
        AddLine (ioFile, std::format ("  Interface text: block {}", segment.textBlock));
    }

    if (segment.block == 0)
    {
        AddLine (ioFile, (segment.kind == kDataSegment) ? "  A data segment has no bytes in the file." : "  The segment has no block.");
        return;
    }

    if (start + segment.length > bytes.size())
    {
        AddLine (ioFile, "  The segment ends past the end of the file.");
        ioFile.notes.push_back (TextEncoding::NarrowToWide (std::format ("Segment {} ends past the end of the file.", segment.name)));
        return;
    }

    code = bytes.subspan (start, segment.length);

    if (listed && ListProcedures (code, ioFile))
    {
        return;
    }

    AddLine (ioFile, "");
    ListHex (code, ioFile);

    if (listed)
    {
        ioFile.notes.push_back (TextEncoding::NarrowToWide (std::format ("Segment {} is listed in hex: its procedure dictionary is damaged.", segment.name)));
    }
    else
    {
        ioFile.notes.push_back (TextEncoding::NarrowToWide (std::format ("Segment {} is listed in hex: it is {}, version {}.", segment.name, GetMachineName (segment.machine), segment.version)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefile::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalCodefile::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    std::vector<Segment>  segments;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    if (bytes.size() < kDictionaryBytes)
    {
        outError = L"The file is shorter than a segment dictionary.";
        return HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    }

    for (int slot = 0; slot < kSlots; slot++)
    {
        Segment  segment = ReadSegment (bytes, slot);

        if (segment.block != 0 || segment.length != 0)
        {
            segments.push_back (std::move (segment));
        }
    }

    ListHeader (bytes, (int) segments.size(), outFile);

    for (const Segment & segment : segments)
    {
        ListSegment (bytes, segment, outFile);
    }

    if (segments.empty())
    {
        outFile.notes.push_back (L"The segment dictionary is empty.");
    }

    return S_OK;
}
