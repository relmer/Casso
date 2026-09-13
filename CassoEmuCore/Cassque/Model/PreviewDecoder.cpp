#include "Pch.h"

#include "Cassque/Model/PreviewDecoder.h"
#include "Cassque/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"
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

    //  DOS 3.3 and ProDOS disks read as their own listings do.
    if (kind == VolumeKind::Dos33)
    {
        RenderDos33Catalog (listing, outContent.lines);
    }
    else if (kind == VolumeKind::ProDos)
    {
        RenderProDosCatalog (listing, outContent.lines);
    }
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
                outContent.bytes  = payload.bytes;
                outContent.origin = origin;
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
//  after a two-space gap when the label fills the column. Any other line, such
//  as the headline naming the image, is not a detail.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDecoder::ParseDetails (const std::string & message, std::vector<std::pair<std::wstring, std::wstring>> & outDetails)
{
    constexpr size_t  s_kValueColumn = 16;



    size_t       start = 0;
    size_t       end   = 0;
    size_t       gap   = 0;
    std::string  line;
    std::string  label;
    std::string  value;



    outDetails.clear();

    while (start < message.size())
    {
        end   = message.find ('\n', start);
        end   = (end == std::string::npos) ? message.size() : end;
        line  = message.substr (start, end - start);
        start = end + 1;

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

        if (!label.empty() && !value.empty())
        {
            label[0] = (char) toupper ((unsigned char) label[0]);
            outDetails.emplace_back (TextEncoding::NarrowToWide (label), TextEncoding::NarrowToWide (value));
        }
    }

    if (!outDetails.empty())
    {
        outDetails.insert (outDetails.begin(), std::make_pair (std::wstring (L"File system"), std::wstring (L"Not DOS 3.3 or ProDOS")));
    }

    return !outDetails.empty();
}
