#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/PicturePreview.h"
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
    enum class Kind { Listing, Text, Picture, Hex, Catalog, Details, Error, Shell };

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

    //  The rows are what an archive holds, shown as the file list shows a
    //  folder rather than as a catalog.
    bool                       archive = false;

    //  What an image with no file system says about itself, as label and value.
    std::vector<std::pair<std::wstring, std::wstring>>  details;
    std::wstring                                        message;
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

    //  Set for a hex preview of a file whose type is 6502 code, which is
    //  previewed in hex or as a disassembly by its type rather than for want
    //  of another preview.
    bool                       executable  = false;

    //  A picture's mode, as chosen and as drawn, with its bytes in `bytes` and
    //  its load address in `origin`, so it can be drawn again for another
    //  monitor, or a double hi-res picture the other way, without a read.
    PicturePreview::Mode       pictureMode   = PicturePreview::Mode::None;
    PicturePreview::Mode       pictureChosen = PicturePreview::Mode::None;

    //  A WOZ image, for Disk properties. Its catalog has the image's WOZ info
    //  and metadata in `details`, listed above it.
    std::wstring               wozPath;

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
    //  ProDOS BIN or SYS file.
    static bool  IsExecutableType (Byte type, VolumeKind kind);

    //  The whole preview for one file's payload. `bus` is handed to the video
    //  modes and never read; a test proves it with a bus that fails on read.
    static HRESULT  Render (const FileEntry    & entry,
                            VolumeKind           kind,
                            const FilePayload  & payload,
                            HexForm              hexForm,
                            MemoryBus          & bus,
                            PreviewContent     & outContent,
                            PicturePreview::Monitor  monitor    = PicturePreview::Monitor::Color,
                            uint32_t                 customArgb = PicturePreview::kDefaultCustomArgb);

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
};
