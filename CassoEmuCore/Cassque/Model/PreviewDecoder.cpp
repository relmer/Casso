#include "Pch.h"

#include "Cassque/Model/PreviewDecoder.h"
#include "Cassque/Model/PicturePreview.h"
#include "AppleTextCodec.h"
#include "ApplesoftTokenizer.h"
#include "Cpu.h"
#include "Disassembler.h"
#include "IntegerBasicDetokenizer.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"



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
                                       : (type == ProDosVolume::kTypeBasic);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsIntegerType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsIntegerType (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeInteger)
                                       : (type == s_kProDosTypeInteger);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsTextType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsTextType (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeText)
                                       : (type == ProDosVolume::kTypeText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::IsBinaryType
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::IsBinaryType (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? (type == Dos33Volume::kTypeBinary)
                                       : (type == ProDosVolume::kTypeBinary);
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

    if (IsBinaryType (entry.type, kind) && hasLoadAddress
     && PicturePreview::Choose (loadAddress, byteCount) != PicturePreview::Mode::None)
    {
        return PreviewContent::Kind::Picture;
    }

    return PreviewContent::Kind::Hex;
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
//  PreviewDecoder::RenderHexRows
//
//  `2000: A9 00 8D 00 C0 ..  |....|`, sixteen bytes to a row, addressed from
//  the origin so the offsets read as the addresses the file loads at.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderHexRows (std::span<const Byte> bytes, Word origin, std::vector<std::wstring> & outLines)
{
    size_t  at    = 0;
    size_t  count = bytes.size();



    outLines.clear();

    while (at < count)
    {
        size_t        inRow = std::min (kBytesPerHexRow, count - at);
        size_t        i     = 0;
        std::wstring  text  = std::format (L"{:04X}: ", (unsigned) (Word) (origin + at));
        std::wstring  ascii;

        for (i = 0; i < kBytesPerHexRow; i++)
        {
            if (i < inRow)
            {
                Byte  value = bytes[at + i];
                Byte  shown = (Byte) (value & 0x7F);

                text  += std::format (L"{:02X} ", (unsigned) value);
                ascii += (shown >= 0x20 && shown < 0x7F) ? (wchar_t) shown : L'.';
            }
            else
            {
                text += L"   ";
            }
        }

        text += L" |" + ascii + L"|";

        outLines.push_back (text);
        at += inRow;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::RenderDisassembly
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDecoder::RenderDisassembly (
    std::span<const Byte>        bytes,
    Word                         origin,
    const Microcode            * table,
    std::vector<std::wstring>  & outLines)
{
    std::vector<DisassembledLine>  lines;



    outLines.clear();

    Disassembler::Disassemble (bytes, origin, table, lines);

    for (const DisassembledLine & line : lines)
    {
        std::string  text = Disassembler::FormatLine (line);

        outLines.emplace_back (text.begin(), text.end());
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoder::Render
//
//  A listing that will not decode becomes the error kind with the decoder's
//  message, never a partial listing: the spec's refusal rule is the
//  detokenizers' own, and this only carries it to the pane.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PreviewDecoder::Render (
    const FileEntry    & entry,
    VolumeKind           kind,
    const FilePayload  & payload,
    bool                 disassemble,
    MemoryBus          & bus,
    PreviewContent     & outContent)
{
    HRESULT                   hr      = S_OK;
    PreviewContent::Kind      decided = Decide (entry, kind, payload.bytes.size(),
                                                payload.loadAddress, payload.hasLoadAddress);
    Word                      origin  = payload.hasLoadAddress ? payload.loadAddress : 0;
    std::string               text;
    ApplesoftListingError     applesoftError;
    IntegerBasicListingError  integerError;
    PicturePreview::Mode      mode    = PicturePreview::Mode::None;



    outContent      = PreviewContent();
    outContent.kind = decided;

    switch (decided)
    {
        case PreviewContent::Kind::Listing:
            if (IsApplesoftType (entry.type, kind))
            {
                hr = ApplesoftTokenizer::Detokenize (payload.bytes, text, applesoftError);

                if (FAILED (hr))
                {
                    outContent.kind    = PreviewContent::Kind::Error;
                    outContent.message = std::format (L"Line {}: {}",
                                                      (unsigned) applesoftError.lineNumber,
                                                      std::wstring (applesoftError.reason.begin(), applesoftError.reason.end()));
                    hr = S_OK;
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
                                                      std::wstring (integerError.reason.begin(), integerError.reason.end()));
                    outContent.offset  = integerError.offset;
                    hr = S_OK;
                }
            }

            SplitIntoLines (text, outContent.lines);
            break;

        case PreviewContent::Kind::Text:
            AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, text);
            SplitIntoLines (text, outContent.lines);
            break;

        case PreviewContent::Kind::Picture:
            mode = PicturePreview::Choose (payload.loadAddress, payload.bytes.size());
            hr   = PicturePreview::Render (mode, payload.bytes, payload.loadAddress, bus,
                                           outContent.bgra, outContent.width, outContent.height);
            CHR (hr);
            break;

        case PreviewContent::Kind::Hex:
            if (disassemble)
            {
                RenderDisassembly (payload.bytes, origin, GetNmosTable(), outContent.lines);
            }
            else
            {
                RenderHexRows (payload.bytes, origin, outContent.lines);
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
