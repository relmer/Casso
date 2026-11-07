#include "Pch.h"

#include "Core/AppleSingleCodec.h"
#include "CassoExplorer/Model/ApplesoftVariables.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "AppleTextCodec.h"
#include "ApplesoftTokenizer.h"
#include "Cpu.h"
#include "Disassembler.h"
#include "IntegerBasicDetokenizer.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NativeListing.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Debugger/RomSymbols.h"
#include "Ui/Debugger/SourceSyntax.h"



//  ProDOS's Integer BASIC type, which the volume layer has no constant for
//  because nothing places one.
static constexpr Byte  s_kProDosTypeInteger = 0xFA;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsApplesoftType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsApplesoftType (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeApplesoft)
                                       : (kind == VolumeKind::ProDos && type == ProDosVolume::kTypeBasic);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsIntegerType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsIntegerType (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeInteger)
                                       : (kind == VolumeKind::ProDos && type == s_kProDosTypeInteger);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsTextType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsTextType (Byte type, VolumeKind kind)
{
    if (kind == VolumeKind::Pascal)
    {
        return type == PascalVolume::kKindText;
    }

    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeText)
                                       : (kind == VolumeKind::ProDos && type == ProDosVolume::kTypeText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsBinaryType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsBinaryType (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeBinary)
                                       : (kind == VolumeKind::ProDos && type == ProDosVolume::kTypeBinary);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsExecutableType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsExecutableType (Byte type, VolumeKind kind)
{
    return IsBinaryType (type, kind) || IsAlwaysCodeType (type, kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsAlwaysCodeType
//
//  ProDOS runs a SYS file, BASIC.SYSTEM runs a CMD file, and a REL file is
//  relocated code.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsAlwaysCodeType (Byte type, VolumeKind kind)
{
    return kind == VolumeKind::ProDos && (type == ProDosVolume::kTypeSystem  ||
                                          type == ProDosVolume::kTypeCommand ||
                                          type == ProDosVolume::kTypeRelocatable);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsCodeAddress (Word origin)
{
    return origin >= s_kFirstCodeAddress && (origin < s_kIoSpaceStart || origin >= s_kIoSpaceEnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::LooksLikeCode
//
//  Whether a binary's first instructions are 6502 code: few enough of them
//  are opcodes the 6502 doesn't document. Measured on MousePaint's files,
//  its programs have none to four in their first 64 instructions, and its
//  fonts, icons and tables six or more.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::LooksLikeCode (std::span<const Byte> bytes, Word origin)
{
    std::vector<DisassembledLine>  lines;
    size_t                         sampled      = 0;
    size_t                         undocumented = 0;



    Disassembler::Disassemble (bytes.first ((std::min) (bytes.size(), s_kCodeSampleBytes)), origin, GetNmosTable(), lines);

    for (const DisassembledLine & line : lines)
    {
        if (sampled == s_kCodeSampleInstructions)
        {
            break;
        }

        sampled++;
        undocumented += SourceSyntax::IsMnemonic (TextEncoding::NarrowToWide (line.mnemonic)) ? 0 : 1;
    }

    return sampled > 0 && undocumented * 100 <= sampled * s_kCodeUndocumentedPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::GetNmosTable
//
//  One CPU for the process, so the table's register pointers stay valid; the
//  disassembler never dereferences them, but a dangling table is a dangling
//  table.
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * PreviewDecoder::GetNmosTable()
{
    static Cpu  cpu;



    return cpu.GetInstructionSet();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::GetSymbols
//
//  The names of known addresses in a disassembly: the Monitor, the //e's
//  switches and Applesoft everywhere, and the entry points of the disk's
//  operating system. Where two tables have one address, the first table's
//  name is used, so $C000 is KBD rather than 80STOREOFF.
//
////////////////////////////////////////////////////////////////////////////////

const std::map<Word, std::string> & PreviewDecoder::GetSymbols (VolumeKind kind)
{
    static const std::map<Word, std::string>  s_common = ParseSymbols ({ RomSymbols::GetMain (s_kpszSymbolMachine), RomSymbols::GetBasic() });
    static const std::map<Word, std::string>  s_dos33  = ParseSymbols ({ RomSymbols::GetMain (s_kpszSymbolMachine), RomSymbols::GetBasic(), RomSymbols::GetDos33() });
    static const std::map<Word, std::string>  s_proDos = ParseSymbols ({ RomSymbols::GetMain (s_kpszSymbolMachine), RomSymbols::GetBasic(), RomSymbols::GetProDos() });



    return (kind == VolumeKind::Dos33)  ? s_dos33
         : (kind == VolumeKind::ProDos) ? s_proDos
                                        : s_common;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::ParseSymbols
//
//  Debug file text: a NAME=$ADDR line a symbol, and ; a comment.
//
////////////////////////////////////////////////////////////////////////////////

std::map<Word, std::string> PreviewDecoder::ParseSymbols (std::initializer_list<const char *> tables)
{
    std::map<Word, std::string>  symbols;



    for (const char * table : tables)
    {
        std::istringstream  stream (table);
        std::string         line;

        while (std::getline (stream, line))
        {
            size_t  equals = line.find ("=$");

            if (line.empty() || line[0] == ';' || equals == std::string::npos)
            {
                continue;
            }

            symbols.emplace ((Word) std::stoul (line.substr (equals + 2), nullptr, 16), line.substr (0, equals));
        }
    }

    return symbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::GetMliCalls
//
//  The ProDOS 8 MLI's call numbers. Source: ProDOS 8 Technical Reference
//  Manual (Apple Computer, 1985), chapter 4.
//
////////////////////////////////////////////////////////////////////////////////

const std::map<Byte, std::string> & PreviewDecoder::GetMliCalls()
{
    static const std::map<Byte, std::string>  s_calls =
    {
        { 0x40, "ALLOC_INTERRUPT" }, { 0x41, "DEALLOC_INTERRUPT" }, { 0x65, "QUIT"     },
        { 0x80, "READ_BLOCK"      }, { 0x81, "WRITE_BLOCK"       }, { 0x82, "GET_TIME" },
        { 0xC0, "CREATE"          }, { 0xC1, "DESTROY"           }, { 0xC2, "RENAME"   },
        { 0xC3, "SET_FILE_INFO"   }, { 0xC4, "GET_FILE_INFO"     }, { 0xC5, "ON_LINE"  },
        { 0xC6, "SET_PREFIX"      }, { 0xC7, "GET_PREFIX"        }, { 0xC8, "OPEN"     },
        { 0xC9, "NEWLINE"         }, { 0xCA, "READ"              }, { 0xCB, "WRITE"    },
        { 0xCC, "CLOSE"           }, { 0xCD, "FLUSH"             }, { 0xCE, "SET_MARK" },
        { 0xCF, "GET_MARK"        }, { 0xD0, "SET_EOF"           }, { 0xD1, "GET_EOF"  },
        { 0xD2, "SET_BUF"         }, { 0xD3, "GET_BUF"           },
    };



    return s_calls;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::Decide
//
////////////////////////////////////////////////////////////////////////////////

PreviewContent::Kind PreviewDecoder::Decide (
    const FileEntry  & entry,
    VolumeKind         kind,
    size_t             byteCount,
    Word               loadAddress,
    bool               hasLoadAddress)
{
    if (entry.isDirectory)
    {
        return PreviewContent::Kind::Catalog;
    }

    if (IsApplesoftType (entry.type, kind) || IsIntegerType (entry.type, kind))
    {
        return PreviewContent::Kind::Listing;
    }

    if (IsTextType (entry.type, kind))
    {
        return PreviewContent::Kind::Text;
    }

    if (kind == VolumeKind::ProDos && entry.type == ApplesoftVariables::kProDosType)
    {
        return PreviewContent::Kind::Details;
    }

    if (IsBinaryType (entry.type, kind) && hasLoadAddress
     && PicturePreview::Choose (loadAddress, byteCount) != PicturePreview::Mode::None)
    {
        return PreviewContent::Kind::Picture;
    }

    return PreviewContent::Kind::Hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::DecideCpm
//
//  CP/M has no file types, so this tests the bytes up to the text end. CP/M text
//  is seven bits, so a high bit isn't Apple text there, as it is elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

PreviewContent::Kind PreviewDecoder::DecideCpm (const FilePayload & payload)
{
    auto                   end     = std::find (payload.bytes.begin(), payload.bytes.end(), CpmVolume::kTextEnd);
    std::span<const Byte>  bytes   (payload.bytes.data(), (size_t) (end - payload.bytes.begin()));
    bool                   isAscii = std::all_of (bytes.begin(), bytes.end(), [] (Byte value) { return value < 0x80; });



    return (isAscii && LooksLikeText (bytes)) ? PreviewContent::Kind::Text : PreviewContent::Kind::Hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::LooksLikeText
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::LooksLikeText (std::span<const Byte> bytes)
{
    size_t  count = (std::min) (bytes.size(), s_kTextSampleBytes);
    size_t  i     = 0;



    //  Trailing zeros are the padding of a file saved in whole sectors.
    while (count > 0 && bytes[count - 1] == 0)
    {
        count--;
    }

    if (count == 0)
    {
        return false;
    }

    for (i = 0; i < count; i++)
    {
        Byte  value = (Byte) (bytes[i] & 0x7F);



        if ((value < 0x20 || value == 0x7F) && value != '\t' && value != '\r' && value != '\n')
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::SplitAppleText
//
//  A line feed after a return ends nothing more, so a file written on the
//  host with both reads the same as one written on the Apple.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::SplitAppleText (std::span<const Byte> bytes, std::vector<std::wstring> & outLines)
{
    static constexpr size_t  kTabStop = 8;
    std::wstring             line;
    Byte                     previous = 0;



    outLines.clear();

    for (Byte raw : bytes)
    {
        Byte  value = (Byte) (raw & 0x7F);

        if (value == '\r' || (value == '\n' && previous != '\r'))
        {
            outLines.push_back (std::move (line));
            line.clear();
        }
        else if (value == '\t')
        {
            line.append (kTabStop - line.size() % kTabStop, L' ');
        }
        else if (value != 0 && value != '\n')
        {
            line.push_back ((wchar_t) value);
        }

        previous = (value == 0) ? previous : value;
    }

    if (!line.empty())
    {
        outLines.push_back (std::move (line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::SplitIntoLines
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::SplitIntoLines (const std::string & text, std::vector<std::wstring> & outLines)
{
    std::wstring  line;



    outLines.clear();

    for (char c : text)
    {
        if (c == '\n')
        {
            outLines.push_back (line);
            line.clear();
        }
        else if (c != '\r')
        {
            line += (wchar_t) (Byte) c;
        }
    }

    if (!line.empty())
    {
        outLines.push_back (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderAppleSingle
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderAppleSingle (const AppleSingleFile & file, PreviewContent & outContent)
{
    wchar_t  hex[16] = {};



    outContent         = PreviewContent();
    outContent.kind    = PreviewContent::Kind::Details;
    outContent.details.push_back ({ L"Format", L"AppleSingle" });
    outContent.details.push_back ({ L"Name",   file.realName.empty() ? std::wstring (L"Not recorded") : TextEncoding::NarrowToWide (file.realName) });

    if (file.hasProDosInfo)
    {
        swprintf_s (hex, L" ($%02X)", (unsigned) (file.fileType & 0xFF));
        outContent.details.push_back ({ L"Type", CatalogModel::GetTypeText ((Byte) file.fileType, VolumeKind::ProDos) + hex });

        swprintf_s (hex, L"$%04X", (unsigned) (file.auxType & 0xFFFF));
        outContent.details.push_back ({ L"Aux type", hex });
    }
    else
    {
        outContent.details.push_back ({ L"Type", L"Not recorded" });
    }

    outContent.details.push_back ({ L"Size", std::to_wstring (file.data.size()) + L" bytes" });

    if (file.createDate)
    {
        outContent.details.push_back ({ L"Created", FormatAppleSingleDate (*file.createDate) });
    }

    if (file.modifyDate)
    {
        outContent.details.push_back ({ L"Modified", FormatAppleSingleDate (*file.modifyDate) });
    }

    if (file.backupDate)
    {
        outContent.details.push_back ({ L"Backed up", FormatAppleSingleDate (*file.backupDate) });
    }

    if (file.accessDate)
    {
        outContent.details.push_back ({ L"Accessed", FormatAppleSingleDate (*file.accessDate) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::FormatAppleSingleDate
//
//  AppleSingle counts seconds from 2000-01-01 UTC, and the date is shown in
//  UTC, since the container records no zone.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDecoder::FormatAppleSingleDate (int32_t secondsFrom2000)
{
    static constexpr int64_t  kUnixSecondsAt2000 = 946684800;
    static constexpr int64_t  kTicksPerSecond    = 10000000;
    static constexpr int64_t  kTicksToUnixEpoch  = 116444736000000000;
    int64_t                   ticks              = (kUnixSecondsAt2000 + secondsFrom2000) * kTicksPerSecond + kTicksToUnixEpoch;
    FILETIME                  utc                = { (DWORD) ticks, (DWORD) (ticks >> 32) };
    SYSTEMTIME                when               = {};
    SYSTEMTIME                local              = {};
    wchar_t                   date[80]           = {};
    wchar_t                   time[80]           = {};
    std::wstring              text;



    if (!FileTimeToSystemTime (&utc, &when) || !SystemTimeToTzSpecificLocalTime (nullptr, &when, &local)
        || GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, date, (int) std::size (date), nullptr) == 0
        || GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, (int) std::size (time)) == 0)
    {
        return L"Not valid";
    }

    text = std::wstring (date) + L" " + time;
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderDisassembly
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderDisassembly (
    std::span<const Byte>        bytes,
    Word                         origin,
    VolumeKind                   kind,
    bool                         relocatable,
    std::vector<std::wstring>  & outLines)
{
    std::vector<DisassembledLine>  lines;
    DisassemblyOptions             options;
    std::span<const Byte>          code     = bytes;
    std::span<const Byte>          trailer;
    size_t                         codeSize = 0;



    outLines.clear();

    options.symbols          = &GetSymbols (kind);
    options.nmosUndocumented = true;

    if (kind == VolumeKind::ProDos)
    {
        options.mliEntry = s_kMliEntry;
        options.mliCalls = &GetMliCalls();
    }

    //  An EDASM relocatable file is its code's length, the code, and then its
    //  relocation and external symbol dictionaries.
    if (relocatable && bytes.size() >= 2)
    {
        codeSize = (size_t) (bytes[0] | (bytes[1] << 8));

        if (2 + codeSize <= bytes.size())
        {
            code    = bytes.subspan (2, codeSize);
            trailer = bytes.subspan (2 + codeSize);
        }
    }

    Disassembler::Disassemble (code, origin, GetNmosTable(), lines, options);

    if (!trailer.empty())
    {
        AddRelocationLines (trailer, origin, (Word) (origin + codeSize), lines);
    }

    for (const DisassembledLine & line : lines)
    {
        std::string  text = Disassembler::FormatLine (line);

        outLines.emplace_back (text.begin(), text.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::AddRelocationLines
//
//  An EDASM relocation dictionary is four-byte records ended by a zero
//  flags byte: the flags, the offset in the code of the field to relocate,
//  and a byte that, for a reference to an external symbol, is the symbol's
//  number. Flags bit 4 marks that reference. Each field's instruction is
//  marked, and the records and the external symbol dictionary after them
//  are listed as data past the code.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::AddRelocationLines (std::span<const Byte> trailer, Word origin, Word end, std::vector<DisassembledLine> & ioLines)
{
    size_t  at        = 0;
    size_t  codeLines = ioLines.size();



    while (at + s_kRelocationRecordSize <= trailer.size() && trailer[at] != 0)
    {
        Byte              flags    = trailer[at];
        Word              field    = (Word) (origin + (trailer[at + 1] | (trailer[at + 2] << 8)));
        bool              external = (flags & s_kRelocationExternal) != 0;
        DisassembledLine  record;

        for (size_t i = 0; i < codeLines; i++)
        {
            DisassembledLine &  line = ioLines[i];

            if (field >= line.address && field < line.address + line.bytes.size())
            {
                line.comment = external ? std::format ("external symbol {}", (unsigned) trailer[at + 3]) : "relocated";
                break;
            }
        }

        record.address   = (Word) (end + at);
        record.mnemonic  = Disassembler::kByteDirective;
        record.operand   = std::format ("${:02X},${:02X},${:02X},${:02X}", (unsigned) trailer[at], (unsigned) trailer[at + 1], (unsigned) trailer[at + 2], (unsigned) trailer[at + 3]);
        record.comment   = "relocation record";
        record.isDefined = true;
        ioLines.push_back (record);

        at += s_kRelocationRecordSize;
    }

    if (at < trailer.size() && trailer[at] == 0)
    {
        DisassembledLine  last;

        last.address   = (Word) (end + at);
        last.mnemonic  = Disassembler::kByteDirective;
        last.operand   = "$00";
        last.comment   = "end of relocation records";
        last.isDefined = true;
        ioLines.push_back (last);

        at++;
    }

    //  The external symbol dictionary, a row of bytes at a time.
    while (at < trailer.size())
    {
        size_t            count = (std::min) (s_kDataBytesPerLine, trailer.size() - at);
        DisassembledLine  data;

        data.address   = (Word) (end + at);
        data.mnemonic  = Disassembler::kByteDirective;
        data.comment   = "external symbols";
        data.isDefined = true;

        for (size_t i = 0; i < count; i++)
        {
            data.operand += ((i == 0) ? "$" : ",$") + Disassembler::FormatHexByte (trailer[at + i]);
        }

        ioLines.push_back (data);
        at += count;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderCatalog
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderCatalog (const VolumeListing & listing, VolumeKind kind, PreviewContent & outContent)
{
    outContent      = PreviewContent();
    outContent.kind = PreviewContent::Kind::Catalog;

    CatalogModel::FromListing (listing, kind, outContent.rows);

    //  DOS 3.3 and ProDOS disks read as their own listings do.
    if (kind == VolumeKind::Dos33)
    {
        RenderDos33Catalog (listing, outContent.lines);
    }
    else if (kind == VolumeKind::ProDos)
    {
        RenderProDosCatalog (listing, outContent.lines);
    }
    else if (kind == VolumeKind::Pascal || kind == VolumeKind::Cpm)
    {
        RenderBlockCatalog (listing, kind, outContent.lines);
    }

    if (!outContent.lines.empty())
    {
        outContent.heading = TextEncoding::NarrowToWide (VolumeImage::GetSystemName (kind)) + L" disk";
    }

    outContent.warning = CatalogModel::DescribeDamage (listing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::Render
//
//  A listing that will not decode becomes the error kind with the decoder's
//  message, never a partial listing: the spec's refusal rule is the
//  detokenizers' own, and this only passes it to the pane.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PreviewDecoder::Render (
    const FileEntry    & entry,
    VolumeKind           kind,
    const FilePayload  & payload,
    HexForm              hexForm,
    MemoryBus          & bus,
    PreviewContent     & outContent,
    PicturePreview::Monitor  monitor,
    uint32_t                 customArgb)
{
    HRESULT                   hr          = S_OK;
    PreviewContent::Kind      decided     = (kind == VolumeKind::Cpm) ? DecideCpm (payload)
                                                                      : Decide (entry, kind, payload.bytes.size(), payload.loadAddress, payload.hasLoadAddress);
    Word                      origin      = payload.hasLoadAddress ? payload.loadAddress : 0;
    std::string               text;
    ApplesoftListingError     applesoftError;
    IntegerBasicListingError  integerError;
    PicturePreview::Mode      mode        = PicturePreview::Mode::None;
    bool                      relocatable = kind == VolumeKind::ProDos && entry.type == ProDosVolume::kTypeRelocatable;



    outContent      = PreviewContent();
    outContent.kind = decided;

    switch (decided)
    {
        case PreviewContent::Kind::Listing:
            if (payload.bytes.empty())
            {
                outContent.kind    = PreviewContent::Kind::Error;
                outContent.message = L"This file is empty.";
            }
            else if (IsApplesoftType (entry.type, kind))
            {
                hr = ApplesoftTokenizer::Detokenize (payload.bytes, text, applesoftError);

                //  A DOS 3.3 file's recorded length can stop short of the end of
                //  its program. The lines before the cut are shown, and why the
                //  file is not a valid program follows them.
                if (FAILED (hr) && IsCutOff (applesoftError) && !applesoftError.partialListing.empty())
                {
                    text               = applesoftError.partialListing;
                    outContent.warning = DescribeCutOff (applesoftError);
                    hr                 = S_OK;
                }
                else if (FAILED (hr))
                {
                    outContent.kind    = PreviewContent::Kind::Error;
                    outContent.message = std::format (L"Line {}: {}",
                                                      (unsigned) applesoftError.lineNumber,
                                                      TextEncoding::NarrowToWide (applesoftError.reason));
                    hr = S_OK;
                }

                //  The program's bytes stay with its listing, so the window can
                //  find where each line starts in memory.
                if (outContent.kind == PreviewContent::Kind::Listing)
                {
                    outContent.bytes = payload.bytes;
                }
            }
            else
            {
                hr = IntegerBasicDetokenizer::Detokenize (payload.bytes, text, integerError);

                if (FAILED (hr))
                {
                    outContent.kind    = PreviewContent::Kind::Error;
                    outContent.message = std::format (L"Offset {}: {}",
                                                      (unsigned) integerError.offset,
                                                      TextEncoding::NarrowToWide (integerError.reason));
                    outContent.offset  = integerError.offset;
                    hr = S_OK;
                }
                else
                {
                    outContent.bytes        = payload.bytes;
                    outContent.integerBasic = true;
                }
            }

            SplitIntoLines (text, outContent.lines);
            break;

        case PreviewContent::Kind::Text:
            //  Text is lines of text. A text file whose bytes are not text
            //  goes to the hex view, as a binary does.
            if (payload.textForm != PayloadTextForm::AppleHighAscii)
            {
                DecodeVolumeText (payload, text);
                SplitIntoLines (text, outContent.lines);
            }
            else if (LooksLikeText (payload.bytes))
            {
                SplitAppleText (payload.bytes, outContent.lines);
            }
            else
            {
                outContent.kind   = PreviewContent::Kind::Hex;
                outContent.bytes  = payload.bytes;
                outContent.origin = origin;
            }

            break;

        case PreviewContent::Kind::Picture:
            mode = PicturePreview::Choose (payload.loadAddress, payload.bytes.size(), entry.name, payload.bytes);
            hr   = PicturePreview::Render (mode, payload.bytes, payload.loadAddress, bus,
                                           outContent.bgra, outContent.width, outContent.height, monitor, customArgb);
            CHR (hr);

            outContent.bytes         = payload.bytes;
            outContent.origin        = payload.loadAddress;
            outContent.pictureMode   = mode;
            outContent.pictureChosen = mode;
            break;

        case PreviewContent::Kind::Hex:
            if (kind == VolumeKind::ProDos && entry.type == ProDosVolume::kTypeSystem && !payload.hasLoadAddress)
            {
                origin = s_kSystemLoadAddress;
            }

            if (kind == VolumeKind::ProDos && entry.type == ProDosVolume::kTypeCommand)
            {
                origin = s_kCommandLoadAddress;
            }

            //  A relocatable file's aux type is the address its code was assembled at.
            if (relocatable)
            {
                origin = payload.hasAuxType ? payload.auxType : entry.auxType;
            }

            outContent.executable = IsExecutableType (entry.type, kind);

            //  A SYS, CMD or REL file is run as code. A binary may be data, such
            //  as a font, which opens as bytes. So does a binary at an address
            //  where code can't be loaded.
            if (hexForm == HexForm::Disassembly || (hexForm == HexForm::Auto && outContent.executable &&
                                                    (IsAlwaysCodeType (entry.type, kind) || (IsCodeAddress (origin) && LooksLikeCode (payload.bytes, origin)))))
            {
                RenderDisassembly (payload.bytes, origin, kind, relocatable, outContent.lines);
                outContent.disassembly = true;
            }
            else
            {
                outContent.bytes    = payload.bytes;
                outContent.origin   = origin;
                outContent.textFile = LooksLikeText (payload.bytes);
            }

            break;

        case PreviewContent::Kind::Details:
            hr = ApplesoftVariables::Decode (payload.bytes, outContent.details);

            if (FAILED (hr))
            {
                outContent.kind    = PreviewContent::Kind::Error;
                outContent.message = L"Its variable tables run past the end of the file.";
                hr                 = S_OK;
            }

            break;

        case PreviewContent::Kind::Catalog:
        default:
            //  A directory entry's catalog is enumerated by the caller, which
            //  holds the volume; the payload of a directory is nothing.
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderDos33Catalog
//
//  CATALOG's own lines: the volume, then per file the lock, the type letter,
//  the sector count and the name, and the free sectors at the foot.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderDos33Catalog (const VolumeListing & listing, std::vector<std::wstring> & outLines)
{
    wchar_t  line[64] = {};



    outLines.clear();

    swprintf_s (line, L"DISK VOLUME %u", (unsigned) listing.volumeNumber);
    outLines.push_back (line);
    outLines.push_back (std::wstring());

    for (const FileEntry & entry : listing.entries)
    {
        swprintf_s (line, L"%lc%lc %03u ",
                    entry.isLocked ? L'*' : L' ',
                    (wchar_t) HostFileNaming::GetDos33TypeLetter (entry.type),
                    (unsigned) (entry.sizeUnits % 1000));
        outLines.push_back (line + TextEncoding::NarrowToWide (entry.name));
    }

    outLines.push_back (std::wstring());
    swprintf_s (line, L"%u sectors free of %u", (unsigned) listing.freeUnits, (unsigned) listing.totalUnits);
    outLines.push_back (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderBlockCatalog
//
//  The Filer's Extended list or CP/M's STAT *.*, the lines `disk list` prints.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderBlockCatalog (const VolumeListing & listing, VolumeKind kind, std::vector<std::wstring> & outLines)
{
    std::vector<std::string>  lines;



    outLines.clear();

    if (kind == VolumeKind::Pascal)
    {
        NativeListing::FormatPascal (listing, lines);
    }
    else
    {
        NativeListing::FormatCpm (listing, lines);
    }

    for (const std::string & line : lines)
    {
        outLines.push_back (TextEncoding::NarrowToWide (line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::DecodeVolumeText
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::DecodeVolumeText (const FilePayload & payload, std::string & outText)
{
    if (payload.textForm == PayloadTextForm::PascalPages)
    {
        PascalVolume::DecodeText (payload.bytes, outText);
    }
    else
    {
        CpmVolume::DecodeText (payload.bytes, outText);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderProDosCatalog
//
//  CAT's columns: the volume, a header, per file the lock, the name, the type,
//  the blocks and the date modified, and the blocks free and used at the foot.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderProDosCatalog (const VolumeListing & listing, std::vector<std::wstring> & outLines)
{
    wchar_t   line[96] = {};
    uint32_t  used     = (listing.totalUnits > listing.freeUnits) ? listing.totalUnits - listing.freeUnits : 0;



    outLines.clear();

    outLines.push_back (L"/" + TextEncoding::NarrowToWide (listing.volumeName));
    outLines.push_back (std::wstring());

    swprintf_s (line, L" %-15ls%4ls%8ls  %ls", L"NAME", L"TYPE", L"BLOCKS", L"MODIFIED");
    outLines.push_back (line);
    outLines.push_back (std::wstring());

    for (const FileEntry & entry : listing.entries)
    {
        swprintf_s (line, L"%lc%-15ls%4ls%8u  %ls",
                    entry.isLocked ? L'*' : L' ',
                    TextEncoding::NarrowToWide (entry.name).c_str(),
                    CatalogModel::GetTypeText (entry.type, VolumeKind::ProDos).c_str(),
                    (unsigned) entry.sizeUnits,
                    FormatProDosDate (entry).c_str());
        outLines.push_back (line);
    }

    outLines.push_back (std::wstring());
    swprintf_s (line, L"BLOCKS FREE:%5u     BLOCKS USED:%5u", (unsigned) listing.freeUnits, (unsigned) used);
    outLines.push_back (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::FormatProDosDate
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDecoder::FormatProDosDate (const FileEntry & entry)
{
    tm          parts    = {};
    __time64_t  seconds  = (__time64_t) entry.modifiedUnix;
    wchar_t     text[16] = {};
    errno_t     err      = 0;



    if (!entry.hasModified)
    {
        return L"<NO DATE>";
    }

    err = _gmtime64_s (&parts, &seconds);

    if (err != 0 || parts.tm_mon < 0 || parts.tm_mon > 11)
    {
        return L"<NO DATE>";
    }

    swprintf_s (text, L"%2d-%ls-%02d", parts.tm_mday, s_kMonths[parts.tm_mon], parts.tm_year % 100);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::ParseDetails
//
//  A detail line is two spaces, a label, and its value from column 16, or
//  after a two-space gap when the label fills the column. A line with no
//  label, its text in the value column, is the next line of the value above,
//  and is a detail with an empty label. A line at the left edge ending in a
//  colon is a section's heading, a detail with an empty value. Any other
//  line, such as the headline with the image's path, is not a detail.
//
//  A label and its value start with a capital, except under the WOZ
//  metadata heading, where values are as the image has them, and the custom
//  keys' heading, where labels are too.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::ParseDetails (const std::string & message, std::vector<std::pair<std::wstring, std::wstring>> & outDetails)
{
    constexpr size_t  s_kValueColumn = 16;



    size_t       start      = 0;
    size_t       end        = 0;
    size_t       gap        = 0;
    std::string  line;
    std::string  label;
    std::string  value;
    bool         hasHeading = false;
    bool         asStored   = false;   // under the custom keys' heading: labels and values as stored
    bool         keepValues = false;   // under the standard keys' heading: values as stored



    outDetails.clear();

    while (start < message.size())
    {
        end   = message.find ('\n', start);
        end   = (end == std::string::npos) ? message.size() : end;
        line  = message.substr (start, end - start);
        start = end + 1;

        if (!line.empty() && line[0] != ' ' && line.back() == ':')
        {
            outDetails.emplace_back (TextEncoding::NarrowToWide (line.substr (0, line.size() - 1)), std::wstring());
            hasHeading = true;
            asStored   = line.substr (0, line.size() - 1) == DiskImageSession::kCustomHeading;
            keepValues = asStored || line.substr (0, line.size() - 1) == DiskImageSession::kMetaHeading;
            continue;
        }

        if (!outDetails.empty() && line.size() > s_kValueColumn && line.find_first_not_of (' ') == s_kValueColumn)
        {
            value    = line.substr (s_kValueColumn);
            value[0] = keepValues ? value[0] : (char) toupper ((unsigned char) value[0]);
            outDetails.emplace_back (std::wstring(), TextEncoding::NarrowToWide (value));
            continue;
        }

        if (line.size() <= s_kValueColumn || line.compare (0, 2, "  ") != 0 || line[2] == ' ')
        {
            continue;
        }

        gap   = (line[s_kValueColumn - 1] == ' ') ? s_kValueColumn - 1 : line.find ("  ", 2);
        gap   = (gap == std::string::npos) ? s_kValueColumn : gap;
        label = line.substr (2, gap - 2);
        value = line.substr (gap);

        label.erase (label.find_last_not_of (' ') + 1);
        value.erase (0, value.find_first_not_of (' '));

        //  Custom keys and their values are as the image has them, and so are
        //  the standard keys' values.
        if (!label.empty() && !value.empty() && !asStored)
        {
            label[0] = (char) toupper ((unsigned char) label[0]);
            value[0] = keepValues ? value[0] : (char) toupper ((unsigned char) value[0]);
        }

        if (!label.empty() && !value.empty())
        {
            outDetails.emplace_back (TextEncoding::NarrowToWide (label), TextEncoding::NarrowToWide (value));
        }
    }

    //  The file system is analysis, so it starts the last section when there
    //  are sections.
    if (!outDetails.empty())
    {
        auto  at = outDetails.begin();

        for (auto entry = outDetails.begin(); hasHeading && entry != outDetails.end(); ++entry)
        {
            at = entry->second.empty() ? entry + 1 : at;
        }

        outDetails.insert (at, std::make_pair (std::wstring (L"File system"), std::wstring (L"Not DOS 3.3 or ProDOS")));
    }

    return !outDetails.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsCutOff
//
//  Whether a listing failed because the file ends too soon, rather than
//  because what it holds is not a program.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsCutOff (const ApplesoftListingError & error)
{
    return error.reason == "has no byte ending it"
        || error.reason == "the program ends before its last line does"
        || error.reason == "the program ends inside a line header";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::DescribeCutOff
//
//  Where the file ends, in the terms of the program's own lines.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDecoder::DescribeCutOff (const ApplesoftListingError & error)
{
    std::wstring  text;



    if (error.reason == "has no byte ending it")
    {
        text = std::format (L"Line {}: the file ends before the end-of-line byte", (unsigned) error.lineNumber);
    }
    else if (error.reason == "the program ends inside a line header")
    {
        text = error.hasLastLineNumber
             ? std::format (L"After line {}: the file ends partway through the next line's header", (unsigned) error.lastLineNumber)
             : std::wstring (L"The file ends partway through the first line's header");
    }
    else
    {
        text = error.hasLastLineNumber
             ? std::format (L"After line {}: the file ends without the end-of-program marker", (unsigned) error.lastLineNumber)
             : std::wstring (L"The file ends before its first line");
    }

    return text;
}
