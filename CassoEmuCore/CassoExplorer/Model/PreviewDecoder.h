#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "FileTypes/DecodedFile.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


class MemoryBus;
class Microcode;
struct DisassembledLine;
enum class VolumeKind;
struct ApplesoftListingError;





struct AppleSingleFile;





////////////////////////////////////////////////////////////////////////////////
//
//  HostCharset
//
//  The character set a file from the host is text in, if any.
//
////////////////////////////////////////////////////////////////////////////////

enum class HostCharset
{
    None,       // not text
    Ascii,      // printable ASCII, tabs and line breaks only
    Dos,        // DOS code page 437, with its line-drawing characters
    Windows,    // Windows code page 1252
    Utf8,       // UTF-8, which a view of one character for each byte cannot draw
};





////////////////////////////////////////////////////////////////////////////////
//
//  VariableRow
//
//  One Applesoft variable, or one element of an array, from a VAR file.
//
////////////////////////////////////////////////////////////////////////////////

struct VariableRow
{
    std::wstring  name;          // A, N%, S$, FN F, or an element such as B%(2)
    std::wstring  type;          // Real, Integer, String or Function
    std::wstring  value;
    std::wstring  length;        // a string's length; empty for any other type
    int           array = -1;    // the element's array, in `arrays`; -1 for a simple variable
    size_t        index = 0;     // the element's place in its array

    bool  operator== (const VariableRow &) const = default;
};



//  An array in a VAR file: its name and the subscripts in its DIM.
struct VariableArray
{
    std::wstring  name;          // B%
    std::wstring  dimensions;    // (2) or (1,2)
    size_t        count = 0;

    bool  operator== (const VariableArray &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewContent
//
//  What the preview pane shows for one entry, by kind. Text-shaped kinds
//  hold lines; a picture holds pixels; a catalog holds rows; an error
//  holds the decoder's own message and where it stopped.
//
////////////////////////////////////////////////////////////////////////////////

struct PreviewContent
{
    //  Shell is Windows' own preview of a host file, which the window hosts.
    //  Variables is a VAR file's Applesoft variables.
    //  Document is styled paragraphs, and Grid headings and rows, from a
    //  format FileFormatTable decodes.
    enum class Kind { Listing, Text, Picture, Hex, Catalog, Details, Error, Shell, Variables, Document, Grid };

    Kind                       kind   = Kind::Hex;
    std::vector<std::wstring>  lines;

    //  A catalog's first line, the file system, which the pane draws dimmer than
    //  the lines below it.
    std::wstring               heading;

    //  Why a listing that is shown is not a valid program, or what a catalog
    //  could not read, shown after it.
    std::wstring               warning;
    std::vector<uint32_t>      bgra;
    int                        width  = 0;
    int                        height = 0;
    std::vector<CatalogRow>    rows;
    std::vector<Paragraph>     paragraphs;
    Grid                       grid;

    //  The rows are what an archive holds, shown as the file list shows a
    //  folder rather than as a catalog.
    bool                       archive = false;

    //  What an image with no file system says about itself, as label and value.
    std::vector<std::pair<std::wstring, std::wstring>>  details;
    std::wstring                                        message;

    //  A VAR file's simple variables and array elements, and its arrays.
    std::vector<VariableRow>                            variables;
    std::vector<VariableArray>                          arrays;
    size_t                                              offset  = 0;

    //  A hex preview hands its bytes over as they are, for a view that reads
    //  the rows it draws, rather than as rendered lines of text.
    std::vector<Byte>          bytes;
    Word                       origin = 0;

    //  A hex preview of text, which opens showing only its characters.
    bool                       textFile = false;

    //  The character set of a file the shell lists, such as one inside a zip.
    HostCharset                hostCharset = HostCharset::None;

    //  A hex preview of a file on the host, which the window reads from the
    //  file as it draws rather than from `bytes`.
    std::wstring               hostPath;

    //  Set for an Integer BASIC listing, whose lines are laid out differently
    //  from Applesoft's.
    bool                       integerBasic = false;

    //  Set for a text preview of an assembler's source, drawn with its line
    //  numbers.
    bool                       source = false;

    //  Set for a hex preview rendered as 6502 instructions in `lines`.
    bool                       disassembly = false;

    //  Set for a hex preview of a file whose type is 6502 code, which its type
    //  assigns to the hex view or a disassembly. A file in the hex view only
    //  because no other preview fits it is not one.
    bool                       executable  = false;

    //  A picture's mode, as chosen and as drawn, with its bytes in `bytes` and
    //  its load address in `origin`, so it can be drawn again for another
    //  monitor, or a double hi-res picture the other way, without a read.
    PicturePreview::Mode       pictureMode   = PicturePreview::Mode::None;
    PicturePreview::Mode       pictureChosen = PicturePreview::Mode::None;

    //  A WOZ image, for Disk properties. Its catalog has the image's WOZ info
    //  and metadata in `details`, listed above it.
    std::wstring               wozPath;

    //  The character set text was read in, Default when the preview has no
    //  choice of one, and the record length a random-access text file was
    //  split by, 0 for any other file. The sheet form is the one a
    //  spreadsheet with formulas is listed in, None for any other file.
    Charset                    charset      = Charset::Default;
    size_t                     recordLength = 0;
    SheetForm                  sheetForm    = SheetForm::None;

    bool  operator== (const PreviewContent &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder
//
//  Which kind of preview an entry gets, and the rendering of it.
//
//  THE KIND FOLLOWS THE TYPE BYTE and, for a binary, the graphics rule. File
//  contents are not inspected, since the catalog type already identifies the
//  file, and a type A file that will not detokenize is shown as an error, not
//  as a hex dump.
//
//  A hex preview contains the payload's bytes and their start address, and
//  the hex view reads them directly. A disassembly renders the same bytes
//  through the NMOS table from the same address into lines. An executable
//  file opens as a disassembly, and any other binary as bytes.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewDecoder
{
public:
    //  How a hex preview is drawn: Auto is a disassembly for an executable
    //  file and bytes for any other.
    enum class HexForm { Auto, Bytes, Disassembly };

    static PreviewContent::Kind  Decide (const FileEntry & entry, VolumeKind kind, size_t byteCount, Word loadAddress, bool hasLoadAddress);

    //  Whether a file's type marks it as 6502 code: a DOS 3.3 B file, or a
    //  ProDOS BIN, SYS, REL or CMD file.
    static bool  IsExecutableType (Byte type, VolumeKind kind);
    static bool  IsAlwaysCodeType (Byte type, VolumeKind kind);

    //  Whether a binary's first instructions read as 6502 code rather than
    //  as data.
    static bool  LooksLikeCode (std::span<const Byte> bytes, Word origin);

    //  Whether code can be loaded at an address: not over the zero page or
    //  the stack, and not into the I/O space at $C000 to $CFFF.
    static bool  IsCodeAddress (Word origin);

    //  The whole preview for one file's payload. `bus` is handed to the video
    //  modes and never read; a test proves it with a bus that fails on read.
    //  A format FileFormatTable identifies is decoded first; one that fails
    //  to decode is rendered by its type, with the reason as the warning.
    static HRESULT  Render (const FileEntry    & entry,
                            VolumeKind           kind,
                            const FilePayload  & payload,
                            HexForm              hexForm,
                            MemoryBus          & bus,
                            PreviewContent     & outContent,
                            PicturePreview::Monitor  monitor    = PicturePreview::Monitor::Color,
                            uint32_t                 customArgb = PicturePreview::kDefaultCustomArgb,
                            const DecodeOptions    & options    = DecodeOptions());

    static HRESULT  RenderByType (const FileEntry    & entry,
                                  VolumeKind           kind,
                                  const FilePayload  & payload,
                                  HexForm              hexForm,
                                  MemoryBus          & bus,
                                  PreviewContent     & outContent,
                                  PicturePreview::Monitor  monitor,
                                  uint32_t                 customArgb);

    //  A decoded file as a preview.
    static void  FromDecoded (const DecodedFile & decoded, PreviewContent & outContent);

    //  A disk image's catalog, for an image selected in the file list.
    static void  RenderCatalog (const VolumeListing & listing, VolumeKind kind, PreviewContent & outContent);

    //  A DOS 3.3 or ProDOS catalog as the guest's own CATALOG or CAT prints it.
    static void  RenderDos33Catalog  (const VolumeListing & listing, std::vector<std::wstring> & outLines);
    static void  RenderProDosCatalog (const VolumeListing & listing, std::vector<std::wstring> & outLines);

    //  An Apple Pascal or CP/M catalog as the Filer or STAT prints it.
    static void  RenderBlockCatalog  (const VolumeListing & listing, VolumeKind kind, std::vector<std::wstring> & outLines);

    static PreviewContent::Kind  DecideCpm (const FilePayload & payload);

    //  A text file that isn't high-bit ASCII, as host text.
    static void  DecodeVolumeText    (const FilePayload & payload, std::string & outText);

    //  The labeled lines of the disk command runner's description of an image
    //  with no file system, as label and value; false when the message has none.
    static bool  ParseDetails (const std::string & message, std::vector<std::pair<std::wstring, std::wstring>> & outDetails);

    //  What an AppleSingle host file holds: its name, type, aux type, size
    //  and dates, as label and value.
    static void  RenderAppleSingle (const AppleSingleFile & file, PreviewContent & outContent);

    //  6502 code with the names of the Monitor's, Applesoft's and the disk's
    //  operating system's entry points, the NMOS undocumented opcodes, and on
    //  ProDOS its MLI calls. A relocatable file's code is read past its length,
    //  and its relocation dictionary is listed after it.
    static void  RenderDisassembly (std::span<const Byte> bytes, Word origin, VolumeKind kind, bool relocatable, std::vector<std::wstring> & outLines);

    static const std::map<Word, std::string>  & GetSymbols  (VolumeKind kind);
    static const std::map<Byte, std::string>  & GetMliCalls ();

    //  Whether a file's leading bytes read as text, with or without the high
    //  bit Apple text uses; a DOS 3.3 file's zero padding does not count.
    static bool  LooksLikeText (std::span<const Byte> bytes);

    //  An Apple text file's lines: the high bit cleared, a return ending each
    //  line, zero bytes dropped, and tabs expanded to the next stop of eight.
    static void  SplitAppleText (std::span<const Byte> bytes, std::vector<std::wstring> & outLines);

private:
    static bool  IsApplesoftType (Byte type, VolumeKind kind);
    static bool  IsIntegerType   (Byte type, VolumeKind kind);
    static bool  IsTextType      (Byte type, VolumeKind kind);
    static bool  IsBinaryType    (Byte type, VolumeKind kind);

    //  A ProDOS date as CAT prints it, 17-AUG-84, or <NO DATE>.
    static std::wstring  FormatProDosDate      (const FileEntry & entry);
    static std::wstring  FormatAppleSingleDate (int32_t secondsFrom2000);
    static bool          IsCutOff              (const ApplesoftListingError & error);
    static std::wstring  DescribeCutOff        (const ApplesoftListingError & error);

    static constexpr size_t  s_kTextSampleBytes   = 4096;

    //  Where ProDOS loads a SYS file. A SYS file's directory entry doesn't have a
    //  load address.
    static constexpr Word    s_kSystemLoadAddress = 0x2000;

    //  Where BASIC.SYSTEM loads and runs a CMD file.
    static constexpr Word    s_kCommandLoadAddress = 0x4000;

    //  Hi-res page 1, where an unpacked LZ4FH picture is drawn from.
    static constexpr Word    s_kHiResPage = 0x2000;

    //  The first address past the stack, and the I/O space's range.
    static constexpr Word    s_kFirstCodeAddress = 0x0200;
    static constexpr Word    s_kIoSpaceStart     = 0xC000;
    static constexpr Word    s_kIoSpaceEnd       = 0xD000;

    static constexpr const wchar_t *  s_kMonths[12] =
    {
        L"JAN", L"FEB", L"MAR", L"APR", L"MAY", L"JUN", L"JUL", L"AUG", L"SEP", L"OCT", L"NOV", L"DEC",
    };

    static void               SplitIntoLines (const std::string & text, std::vector<std::wstring> & outLines);
    static const Microcode  * GetNmosTable   ();

    static std::map<Word, std::string>  ParseSymbols       (std::initializer_list<const char *> tables);
    static void                         AddRelocationLines (std::span<const Byte> trailer, Word origin, Word end, std::vector<DisassembledLine> & ioLines);

    //  The machine whose Monitor and switch names a disassembly uses.
    static constexpr const char *  s_kpszSymbolMachine = "Apple //e";

    //  The MLI's entry, on ProDOS's global page.
    static constexpr Word          s_kMliEntry = 0xBF00;

    //  An EDASM relocation record: flags, the field's offset, and a byte.
    static constexpr size_t        s_kRelocationRecordSize = 4;
    static constexpr Byte          s_kRelocationExternal   = 0x10;

    //  Bytes of a dictionary listed a line.
    static constexpr size_t        s_kDataBytesPerLine = 8;

    //  How much of a binary LooksLikeCode reads, and the share of its first
    //  instructions that may be undocumented opcodes.
    static constexpr size_t        s_kCodeSampleBytes         = 256;
    static constexpr size_t        s_kCodeSampleInstructions  = 64;
    static constexpr size_t        s_kCodeUndocumentedPercent = 8;
};
