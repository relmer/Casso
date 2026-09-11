#pragma once

#include "Pch.h"

#include "Cassque/Model/CatalogModel.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


class MemoryBus;
class Microcode;
enum class VolumeKind;





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
    enum class Kind { Listing, Text, Picture, Hex, Catalog, Error };

    Kind                       kind   = Kind::Hex;
    std::vector<std::wstring>  lines;
    std::vector<uint32_t>      bgra;
    int                        width  = 0;
    int                        height = 0;
    std::vector<CatalogRow>    rows;
    std::wstring               message;
    size_t                     offset = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder
//
//  Which kind of preview an entry gets, and the rendering of it.
//
//  THE KIND FOLLOWS THE TYPE BYTE and, for a binary, the graphics rule; nothing
//  here sniffs contents, because the catalog already said what the file is
//  and a type A file that will not detokenize is shown as the error it is
//  rather than as the hex dump it also is.
//
//  Hex rows carry sixteen bytes each, addressed from the load address when the
//  entry records one, and the disassembly toggle re-renders the same bytes
//  through the NMOS table from the same address.
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

    static void  RenderHexRows (std::span<const Byte> bytes, Word origin, std::vector<std::wstring> & outLines);
    static void  RenderDisassembly (std::span<const Byte> bytes, Word origin, const Microcode * table, std::vector<std::wstring> & outLines);

    static constexpr size_t  kBytesPerHexRow = 16;

private:
    static bool  IsApplesoftType (Byte type, VolumeKind kind);
    static bool  IsIntegerType   (Byte type, VolumeKind kind);
    static bool  IsTextType      (Byte type, VolumeKind kind);
    static bool  IsBinaryType    (Byte type, VolumeKind kind);

    static void  SplitIntoLines (const std::string & text, std::vector<std::wstring> & outLines);
    static const Microcode *  GetNmosTable ();
};
