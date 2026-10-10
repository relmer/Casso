#include "Pch.h"

#include "Ui/DiskInspector/InspectorTables.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Ui/DiskInspector/InspectorText.h"





//  An em dash stands for a value that does not apply.
static constexpr LPCWSTR  s_kpszNone = s_kpszEmDash;

enum FindingColumn
{
    kFindingCategory,
    kFindingTrack,
    kFindingSector,
    kFindingCell,
    kFindingText,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::GetFindingColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> InspectorTables::GetFindingColumns()
{
    return { L"Category", L"Track", L"Sector", L"Cell", L"Finding" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::GetTrackColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> InspectorTables::GetTrackColumns()
{
    return { L"Track", L"Recorded", L"Shared with", L"Class", L"Sectors good", L"Nibbles", L"Length", L"Longest sync",
             L"Sector 0", L"RPM", L"Cell timing", L"Findings", L"Reach" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::GetFieldColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> InspectorTables::GetFieldColumns()
{
    return { L"Cell", L"Angle", L"Sector", L"Volume", L"Track", L"Address marks", L"Address checksum", L"Gap",
             L"Data marks", L"Data checksum", L"Encoding", L"Sync before", L"Findings" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::BuildFindings
//
//  In the disk's own order, quarter track then cell, unless a column sorts
//  them; a finding whose category is not in the mask is left out.
//
////////////////////////////////////////////////////////////////////////////////

vector<TableRow> InspectorTables::BuildFindings (const DiskAnalysis & analysis, uint32_t categoryMask, int sortColumn, bool isDescending)
{
    vector<TableRow>  rows;
    size_t            i    = 0;



    for (i = 0; i < analysis.findings.size(); i++)
    {
        const Finding &  f   = analysis.findings[i];
        TableRow         row;

        if ((categoryMask & (1u << static_cast<int> (f.category))) == 0)
        {
            continue;
        }

        row.quarterTrack = f.quarterTrack;
        row.field        = f.field;
        row.finding      = static_cast<int> (i);
        row.cells        = { FindingFormatter::FormatCategory (f.category),
                             f.quarterTrack >= 0 ? InspectorFormat::FormatQuarterTrack (f.quarterTrack) : std::wstring (s_kpszNone),
                             f.sector >= 0       ? InspectorFormat::FormatSector (f.sector)             : std::wstring (s_kpszNone),
                             f.hasCell           ? InspectorFormat::FormatCount (f.cell)                 : std::wstring (s_kpszNone),
                             FindingFormatter::Format (f) };

        rows.push_back (std::move (row));
    }

    if (sortColumn >= 0 && sortColumn <= kFindingText)
    {
        std::stable_sort (rows.begin(), rows.end(), [sortColumn, isDescending] (const TableRow & a, const TableRow & b)
        {
            return isDescending ? IsLess (b.cells[sortColumn], a.cells[sortColumn]) : IsLess (a.cells[sortColumn], b.cells[sortColumn]);
        });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::BuildTracks
//
//  All 160 quarter tracks (FR-047). A quarter track that shares its record
//  with others lists them; the measurements are its record's.
//
////////////////////////////////////////////////////////////////////////////////

vector<TableRow> InspectorTables::BuildTracks (const DiskAnalysis & analysis)
{
    static constexpr double  kNominalCells = 51200.0;



    vector<TableRow>  rows;
    int               qt   = 0;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        const QuarterTrackEntry &  e     = analysis.entries[qt];
        const TrackAnalysis *      track = (e.slot >= 0 && e.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[e.slot].get() : nullptr;
        bool                       has   = track != nullptr && track->framed.cellCount > 0;
        bool                       flux  = has && track->framed.isFlux;
        TableRow                   row;

        row.quarterTrack = qt;
        row.cells        =
        {
            FormatQuarterTrackCell (qt),
            FormatContent (e),
            FormatSharesWith (e),
            InspectorText::FormatTrackClass (e.trackClass),
            (e.sectorsFound > 0) ? std::format (L"{} of {}", e.sectorsGood, e.sectorsFound) : std::wstring (s_kpszNone),
            has  ? InspectorFormat::FormatCount (track->framed.nibbles.size()) : std::wstring (s_kpszNone),
            has  ? InspectorFormat::FormatCount (track->framed.cellCount) + L" (" + InspectorFormat::FormatPercent (track->framed.cellCount / kNominalCells - 1.0) + L")"
                 : std::wstring (s_kpszNone),
            (has && track->measurements.longestSync.count > 0) ? std::to_wstring (track->measurements.longestSync.count) : std::wstring (s_kpszNone),
            (has && track->measurements.sector0Angle >= 0) ? InspectorFormat::FormatDegrees (track->measurements.sector0Angle) : std::wstring (s_kpszNone),
            flux ? std::format (L"{:.1f}", track->measurements.rpm) : std::wstring (s_kpszNone),
            flux ? InspectorFormat::FormatPercent (track->measurements.deviation) : std::wstring (s_kpszNone),
            (e.findingCount > 0) ? std::to_wstring (e.findingCount) : std::wstring(),
            e.isBeyondHeadReach ? L"Beyond the head's reach" : L"",
        };

        rows.push_back (std::move (row));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::BuildFields
//
//  Each address field, and each data field with none, in passing order
//  (FR-042). A row for an address field also describes its data field.
//
////////////////////////////////////////////////////////////////////////////////

vector<TableRow> InspectorTables::BuildFields (const DiskAnalysis & analysis, int quarterTrack)
{
    const QuarterTrackEntry &  e      = analysis.entries[quarterTrack];
    const TrackAnalysis *      track  = (e.slot >= 0 && e.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[e.slot].get() : nullptr;
    vector<TableRow>           rows;
    DecodeChecks               checks = analysis.settings.GetChecksForTrack (track != nullptr ? track->context.physicalTrack : 0);
    size_t                     f      = 0;
    size_t                     s      = 0;



    for (f = 0; track != nullptr && f < track->fields.size(); f++)
    {
        const LocatedField &  field = track->fields[f];
        const LocatedField *  data  = nullptr;
        const LocatedField *  addr  = nullptr;
        TableRow              row;
        int                   count = 0;

        if (field.role == FieldRole::Data && field.pairedField >= 0)
        {
            continue;
        }

        addr = (field.role == FieldRole::Address) ? &field : nullptr;
        data = (field.role == FieldRole::Data) ? &field : (field.pairedField >= 0 ? &track->fields[field.pairedField] : nullptr);

        for (const Finding & finding : track->findings)
        {
            count += (finding.field == static_cast<int> (f) || (field.pairedField >= 0 && finding.field == field.pairedField)) ? 1 : 0;
        }

        for (s = 0; s < track->sectors.size(); s++)
        {
            if (track->sectors[s].addressField == static_cast<int> (f) || track->sectors[s].dataField == static_cast<int> (f))
            {
                row.sectorIndex = static_cast<int> (s);
            }
        }

        row.quarterTrack = quarterTrack;
        row.field        = static_cast<int> (f);
        row.cells        =
        {
            InspectorFormat::FormatCount (field.startCell),
            InspectorFormat::FormatDegrees (TrackAnalyzer::GetAngle (*track, field.startCell)),
            addr != nullptr ? InspectorFormat::FormatSector (addr->sector) : std::wstring (s_kpszNone),
            addr != nullptr ? std::to_wstring (addr->volume) : std::wstring (s_kpszNone),
            addr != nullptr ? std::to_wstring (addr->track)  : std::wstring (s_kpszNone),
            addr != nullptr ? FormatMarks (addr->prologueFound) + L" / " + FormatMarks (std::span<const Byte> (addr->epilogueFound.data(), addr->hasEpilogueTail ? 3 : 2))
                            : std::wstring (s_kpszNone),
            addr == nullptr ? std::wstring (s_kpszNone) : (checks.isAddressChecksumOn ? FormatChecksum (addr->checksumStored, addr->checksumComputed, addr->isAddressChecksumGood) : FormatUnchecked (addr->checksumStored, addr->checksumComputed)),
            (data != nullptr && addr != nullptr) ? std::to_wstring (data->firstNibble - (addr->firstNibble + addr->nibbleCount)) : std::wstring (s_kpszNone),
            data != nullptr ? FormatMarks (data->prologueFound) + L" / " + FormatMarks (std::span<const Byte> (data->epilogueFound.data(), data->hasEpilogueTail ? 3 : 2))
                            : L"No data field",
            data == nullptr ? std::wstring (s_kpszNone) : (checks.isDataChecksumOn ? FormatChecksum (data->data.storedChecksum, data->data.computedChecksum, data->data.isChecksumGood) : FormatUnchecked (data->data.storedChecksum, data->data.computedChecksum)),
            field.kind == DiskFieldKind::Sixteen ? L"6-and-2" : L"5-and-3",
            (f < track->fieldGaps.size() && track->fieldGaps[f].syncBefore.count > 0)
                ? std::format (L"{} {} {}", track->fieldGaps[f].syncBefore.count, s_kpszMultiplyX, track->fieldGaps[f].syncBefore.widthCells)
                : std::wstring (s_kpszNone),
            count > 0 ? std::to_wstring (count) : std::wstring(),
        };

        rows.push_back (std::move (row));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::CountCategories
//
////////////////////////////////////////////////////////////////////////////////

std::array<int, InspectorTables::kCategoryCount> InspectorTables::CountCategories (const DiskAnalysis & analysis)
{
    std::array<int, kCategoryCount>  counts = {};



    for (const Finding & f : analysis.findings)
    {
        counts[static_cast<int> (f.category)]++;
    }

    return counts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::GetAllCategories
//
////////////////////////////////////////////////////////////////////////////////

uint32_t InspectorTables::GetAllCategories()
{
    return (1u << kCategoryCount) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::IsAlignmentNoteShown
//
//  A WOZ whose INFO says its tracks were not imaged in sync (FR-047).
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTables::IsAlignmentNoteShown (const DiskAnalysis & analysis)
{
    return analysis.copy != nullptr && analysis.copy->format == DiskFormat::Woz && !analysis.copy->woz.info.isSynchronized;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatQuarterTrackCell
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatQuarterTrackCell (int quarterTrack)
{
    return InspectorFormat::FormatQuarterTrack (quarterTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatContent
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatContent (const QuarterTrackEntry & entry)
{
    std::wstring  text;



    switch (entry.content)
    {
        case QuarterTrackContent::Nothing:   text = L"Nothing";    break;
        case QuarterTrackContent::BitTrack:  text = L"Bit track";  break;
        case QuarterTrackContent::FluxTrack: text = L"Flux track"; break;
        case QuarterTrackContent::Damaged:   text = L"Damaged";    break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatSharesWith
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatSharesWith (const QuarterTrackEntry & entry)
{
    return entry.sharesWith.empty() ? std::wstring() : InspectorFormat::FormatQuarterTrackList (entry.sharesWith);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatChecksum
//
//  The stored value, then the computed one when they differ.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatChecksum (Byte stored, Byte computed, bool isGood)
{
    return isGood ? InspectorFormat::FormatHexByte (stored) + L" good"
                  : InspectorFormat::FormatHexByte (stored) + L", computed " + InspectorFormat::FormatHexByte (computed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatMarks
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatMarks (std::span<const Byte> found)
{
    return InspectorFormat::FormatByteSequence (found);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::IsLess
//
//  Numbers sort by value, a leading "$" marking hex and commas ignored, and
//  before text; text sorts without regard to case.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTables::IsLess (const std::wstring & a, const std::wstring & b)
{
    double  x      = 0;
    double  y      = 0;
    bool    isNumA = TryParseNumber (a, x);
    bool    isNumB = TryParseNumber (b, y);
    bool    isLess = false;



    if (isNumA && isNumB)
    {
        isLess = x < y;
    }
    else if (isNumA != isNumB)
    {
        isLess = isNumA;
    }
    else
    {
        isLess = _wcsicmp (a.c_str(), b.c_str()) < 0;
    }

    return isLess;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::TryParseNumber
//
//  The number a cell starts with: hex after a "$", otherwise decimal with
//  commas ignored, as in "51,007 (+0.4%)" or "-5.0%".
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTables::TryParseNumber (const std::wstring & s, double & outValue)
{
    std::wstring  digits;
    bool          isHex  = !s.empty() && s[0] == L'$';
    size_t        i      = isHex ? 1 : 0;
    int           value  = 0;
    bool          isNum  = false;



    for (; i < s.size() && (IsNumberChar (s[i]) || s[i] == L',' || s[i] == L'.' || s[i] == L'+' || s[i] == L'-'); i++)
    {
        if (s[i] != L',')
        {
            digits += s[i];
        }
    }

    if (isHex)
    {
        isNum    = InspectorFormat::TryParseHex (digits, value);
        outValue = value;
    }
    else if (!digits.empty() && ((digits[0] >= L'0' && digits[0] <= L'9') || ((digits[0] == L'+' || digits[0] == L'-') && digits.size() > 1)))
    {
        outValue = _wtof (digits.c_str());
        isNum    = true;
    }

    return isNum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::IsNumberChar
//
//  A decimal or hex digit.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTables::IsNumberChar (wchar_t ch)
{
    return (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F') || (ch >= L'a' && ch <= L'f');
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatUnchecked
//
//  A checksum the decode settings do not check still shows both values
//  (FR-020).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatUnchecked (Byte stored, Byte computed)
{
    return L"Not checked (" + InspectorFormat::FormatHexByte (stored) + L", computed " + InspectorFormat::FormatHexByte (computed) + L")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::GetImageColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> InspectorTables::GetImageColumns()
{
    return { L"Item", L"Value" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::BuildImage
//
//  The Image tab (FR-050): the file, then for a WOZ its checksum, INFO,
//  META, maps, records, records no map refers to and chunks, and for a
//  sector image its order; then the file's problems (FR-051). A section
//  starts with a row holding only its title.
//
////////////////////////////////////////////////////////////////////////////////

vector<TableRow> InspectorTables::BuildImage (const ImageDetails & image)
{
    vector<TableRow>  rows;
    bool              isProDos = image.format == DiskFormat::Po;



    AddRow (rows, L"File", L"");
    AddRow (rows, L"File name", TextEncoding::Utf8ToWide (image.fileName));
    AddRow (rows, L"Format",    FormatImageFormat (image));
    AddRow (rows, L"Size",      InspectorFormat::FormatCount (image.fileSize) + L" bytes");
    AddRow (rows, L"Read-only", image.isReadOnly ? L"Yes" : L"No");

    if (image.isWoz)
    {
        AddRow (rows, L"Checksum", image.isCrcMatch ? std::format (L"Matches (${:08X})", image.layout.storedCrc)
                                                    : std::format (L"Stored ${:08X}, computed ${:08X}", image.layout.storedCrc, image.layout.computedCrc));
        AddInfoRows   (rows, image.info);
        AddMetaRows   (rows, image.layout);
        AddMapRows    (rows, L"TMAP", image.layout.tmap);

        if (image.layout.hasFluxMap)
        {
            AddMapRows (rows, L"FLUX", image.layout.flux);
        }

        AddRecordRows (rows, image.layout);
        AddChunkRows  (rows, image.layout);
    }
    else if (image.format == DiskFormat::Dsk || image.format == DiskFormat::Do || isProDos)
    {
        AddRow (rows, L"Sector order", isProDos ? L"ProDOS" : L"DOS 3.3");
        AddRow (rows, L"Tracks",       L"Casso builds each track from the sectors with volume 254");
    }

    if (!image.problems.empty())
    {
        AddRow (rows, L"Problems", L"");

        for (const Finding & problem : image.problems)
        {
            AddRow (rows, L"", FindingFormatter::Format (problem));
        }
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::AddInfoRows
//
//  Every INFO field the file's version has.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTables::AddInfoRows (vector<TableRow> & inOut, const WozInfo & info)
{
    static constexpr LPCWSTR  kBootFormats[] = { L"Unknown", L"16-sector", L"13-sector", L"16-sector and 13-sector" };
    static constexpr LPCWSTR  kMachines[]    = { L"][", L"][+", L"//e", L"//c", L"//e Enhanced", L"IIgs", L"//c+", L"///", L"///+" };
    static constexpr double   kTickNs        = 125.0;



    std::wstring  hardware;
    size_t        bit      = 0;



    AddRow (inOut, L"INFO", L"");
    AddRow (inOut, L"Version",           std::to_wstring (info.version));
    AddRow (inOut, L"Disk type",         info.diskType == WozLoader::kDiskType35 ? L"3.5\"" : info.diskType == WozLoader::kDiskType525 ? L"5.25\"" : std::to_wstring (info.diskType));
    AddRow (inOut, L"Write protected",   info.isWriteProtected ? L"Yes" : L"No");
    AddRow (inOut, L"Synchronized",      info.isSynchronized ? L"Yes" : L"No");
    AddRow (inOut, L"Cleaned",           info.isCleaned ? L"Yes" : L"No");
    AddRow (inOut, L"Creator",           TextEncoding::Utf8ToWide (info.creator));

    if (info.hasVersion2Fields)
    {
        for (bit = 0; bit < std::size (kMachines); bit++)
        {
            hardware += ((info.compatibleHardware >> bit) & 1) ? (hardware.empty() ? L"" : L", ") + std::wstring (kMachines[bit]) : L"";
        }

        AddRow (inOut, L"Sides",               std::to_wstring (info.sides));
        AddRow (inOut, L"Boot sector format",  info.bootSectorFormat < std::size (kBootFormats) ? kBootFormats[info.bootSectorFormat] : std::to_wstring (info.bootSectorFormat));
        AddRow (inOut, L"Optimal bit timing",  std::format (L"{} ({:.3f} {}s)", info.optimalBitTiming, info.optimalBitTiming * kTickNs / 1000.0, s_kpszMicro));
        AddRow (inOut, L"Compatible hardware", hardware.empty() ? L"Unknown" : hardware);
        AddRow (inOut, L"Required RAM",        info.requiredRamK == 0 ? L"Unknown" : std::format (L"{}K", info.requiredRamK));
        AddRow (inOut, L"Largest track",       std::format (L"{} blocks", info.largestTrack));
    }

    if (info.hasVersion3Fields)
    {
        AddRow (inOut, L"Flux block",          std::to_wstring (info.fluxBlock));
        AddRow (inOut, L"Largest flux track",  std::format (L"{} blocks", info.largestFluxTrack));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::AddMetaRows
//
//  Every META entry, in file order.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTables::AddMetaRows (vector<TableRow> & inOut, const WozFileLayout & layout)
{
    if (!layout.metaEntries.empty())
    {
        AddRow (inOut, L"META", L"");
    }

    for (const WozMetaEntry & entry : layout.metaEntries)
    {
        AddRow (inOut, TextEncoding::Utf8ToWide (entry.key), TextEncoding::Utf8ToWide (entry.value));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::AddMapRows
//
//  A map as the file holds it, each run of quarter tracks with the same
//  entry on one row.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTables::AddMapRows (vector<TableRow> & inOut, LPCWSTR title, const std::array<Byte, WozFileLayout::kMapEntries> & map)
{
    size_t  start = 0;
    size_t  qt    = 0;



    AddRow (inOut, title, L"");

    for (qt = 1; qt <= map.size(); qt++)
    {
        if (qt == map.size() || map[qt] != map[start])
        {
            AddRow (inOut, InspectorFormat::FormatQuarterTrack (static_cast<int> (start)) + ((qt - 1 > start) ? L"-" + InspectorFormat::FormatQuarterTrack (static_cast<int> (qt - 1)) : L""),
                    map[start] == WozFileLayout::kNoTrack ? L"No track (255)" : std::format (L"Record {}", map[start]));
            start = qt;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::AddRecordRows
//
//  Each record the file holds, in the fields its version stores, and then
//  the records no map refers to. A v2 record with every field zero is an
//  unused slot, not a record, and is left out.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTables::AddRecordRows (vector<TableRow> & inOut, const WozFileLayout & layout)
{
    vector<bool>  isFlux (std::max (layout.records.size(), layout.v1Records.size()), false);
    size_t        r      = 0;



    for (Byte entry : layout.flux)
    {
        if (layout.hasFluxMap && entry < isFlux.size())
        {
            isFlux[entry] = true;
        }
    }

    AddRow (inOut, L"Track records", L"");

    for (r = 0; r < layout.records.size(); r++)
    {
        const WozTrackRecordFields &  rec = layout.records[r];

        if (rec.startBlock != 0 || rec.blockCount != 0 || rec.bitOrByteCount != 0)
        {
            AddRow (inOut, std::format (L"Record {}", r), std::format (L"Start block {}, {} blocks, {} {}", rec.startBlock, rec.blockCount,
                                                                          InspectorFormat::FormatCount (rec.bitOrByteCount), isFlux[r] ? L"bytes" : L"bits"));
        }
    }

    for (r = 0; r < layout.v1Records.size(); r++)
    {
        const WozV1RecordFields &  rec = layout.v1Records[r];

        AddRow (inOut, std::format (L"Record {}", r), std::format (L"{} bytes used, {} bits, splice point {}, splice nibble ${:02X}, splice bit count {}",
                                                                      InspectorFormat::FormatCount (rec.bytesUsed), InspectorFormat::FormatCount (rec.bitCount),
                                                                      rec.splicePoint, rec.spliceNibble, rec.spliceBitCount));
    }

    if (!layout.unreferenced.empty())
    {
        AddRow (inOut, L"Records no map refers to", L"");
    }

    for (const WozUnreferencedRecord & rec : layout.unreferenced)
    {
        AddRow (inOut, std::format (L"Record {}", rec.index), InspectorFormat::FormatCount (rec.bitOrByteCount) + L" bits or bytes");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::AddChunkRows
//
//  Every chunk's ID and size in file order, WRIT and unknown ones included.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTables::AddChunkRows (vector<TableRow> & inOut, const WozFileLayout & layout)
{
    std::wstring  id;



    AddRow (inOut, L"Chunks", L"");

    for (const WozChunkEntry & chunk : layout.chunks)
    {
        id.clear();

        for (Byte ch : chunk.id)
        {
            id.push_back ((ch >= 0x20 && ch < 0x7F) ? static_cast<wchar_t> (ch) : L'?');
        }

        AddRow (inOut, id, std::format (L"{} bytes at offset {}", InspectorFormat::FormatCount (chunk.size), InspectorFormat::FormatCount (chunk.offset)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::FormatImageFormat
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTables::FormatImageFormat (const ImageDetails & image)
{
    std::wstring  text;



    switch (image.format)
    {
        case DiskFormat::Woz: text = image.info.version >= 3 ? L"WOZ 2.1" : image.info.version == 2 ? L"WOZ 2" : L"WOZ 1"; break;
        case DiskFormat::Po:  text = L"ProDOS-order sector image";                                                      break;
        case DiskFormat::Nib: text = L"Nibble image";                                                                   break;
        default:              text = L"DOS-order sector image";                                                         break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables::AddRow
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTables::AddRow (vector<TableRow> & inOut, const std::wstring & item, const std::wstring & value)
{
    TableRow  row;



    row.cells = { item, value };
    inOut.push_back (std::move (row));
}
