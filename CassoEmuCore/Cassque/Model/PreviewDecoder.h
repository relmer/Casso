#pragma once

#include "Pch.h"

#include "Cassque/Model/CatalogModel.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


class MemoryBus;
class Microcode;
enum class VolumeKind;
struct ApplesoftListingError;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewContent
//
//  What the preview pane shows for one entry, by kind. Text-shaped kinds
//  carry lines; a picture carries pixels; a catalog carries rows; an error
//  carries the decoder's own message and where it stopped.
//
////////////////////////////////////////////////////////////////////////////////

struct PreviewContent
{
    enum class Kind { Listing, Text, Picture, Hex, Catalog, Details, Error };

    Kind                       kind   = Kind::Hex;
    std::vector<std::wstring>  lines;

    //  Why a listing that is shown is not a valid program, shown after it.
    std::wstring               warning;
    std::vector<uint32_t>      bgra;
    int                        width  = 0;
    int                        height = 0;
    std::vector<CatalogRow>    rows;

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

    //  A hex preview of a file on the host, which the window reads from the
    //  file as it draws rather than from `bytes`.
    std::wstring               hostPath;

    //  Set for an Integer BASIC listing, whose lines are laid out differently
    //  from Applesoft's.
    bool                       integerBasic = false;
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
//  the hex view reads them directly; the disassembly toggle renders the same
//  bytes through the NMOS table from the same address into lines.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewDecoder
{
public:
    static PreviewContent::Kind  Decide (const FileEntry & entry, VolumeKind kind, size_t byteCount, Word loadAddress, bool hasLoadAddress);

    //  The whole preview for one file's payload. `bus` is handed to the video
    //  modes and never read; a test proves it with a bus that fails on read.
    static HRESULT  Render (const FileEntry    & entry,
                            VolumeKind           kind,
                            const FilePayload  & payload,
                            bool                 disassemble,
                            MemoryBus          & bus,
                            PreviewContent     & outContent);

    //  A disk image's catalog, for an image selected in the file list.
    static void  RenderCatalog (const VolumeListing & listing, VolumeKind kind, PreviewContent & outContent);

    //  A DOS 3.3 or ProDOS catalog as the guest's own CATALOG or CAT prints it.
    static void  RenderDos33Catalog  (const VolumeListing & listing, std::vector<std::wstring> & outLines);
    static void  RenderProDosCatalog (const VolumeListing & listing, std::vector<std::wstring> & outLines);

    //  The labeled lines of the disk command runner's description of an image
    //  with no file system, as label and value; false when the message has none.
    static bool  ParseDetails (const std::string & message, std::vector<std::pair<std::wstring, std::wstring>> & outDetails);

    static void  RenderDisassembly (std::span<const Byte> bytes, Word origin, const Microcode * table, std::vector<std::wstring> & outLines);

private:
    static bool  IsApplesoftType (Byte type, VolumeKind kind);
    static bool  IsIntegerType   (Byte type, VolumeKind kind);
    static bool  IsTextType      (Byte type, VolumeKind kind);
    static bool  IsBinaryType    (Byte type, VolumeKind kind);

    //  A ProDOS date as CAT prints it, 17-AUG-84, or <NO DATE>.
    static std::wstring  FormatProDosDate (const FileEntry & entry);
    static bool          IsCutOff         (const ApplesoftListingError & error);
    static std::wstring  DescribeCutOff   (const ApplesoftListingError & error);

    static constexpr const wchar_t *  s_kMonths[12] =
    {
        L"JAN", L"FEB", L"MAR", L"APR", L"MAY", L"JUN", L"JUL", L"AUG", L"SEP", L"OCT", L"NOV", L"DEC",
    };

    static void  SplitIntoLines (const std::string & text, std::vector<std::wstring> & outLines);
    static const Microcode *  GetNmosTable ();
};
