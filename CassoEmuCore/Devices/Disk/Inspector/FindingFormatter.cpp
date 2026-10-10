#include "Pch.h"

#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter::Format
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindingFormatter::Format (const Finding & finding)
{
    std::wstring  text;



    switch (finding.category)
    {
        case FindingCategory::Field:      text = FormatFieldFinding (finding);                  break;
        case FindingCategory::ImageFile:  text = FormatImageFinding (finding);                  break;
        case FindingCategory::FileSystem: text = TextEncoding::Utf8ToWide (finding.detail);        break;
        default:                          text = FormatTrackFinding (finding);                  break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter::FormatCategory
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindingFormatter::FormatCategory (FindingCategory category)
{
    std::wstring  text;



    switch (category)
    {
        case FindingCategory::Track:        text = L"Track";        break;
        case FindingCategory::Field:        text = L"Field";        break;
        case FindingCategory::Sector:       text = L"Sector";       break;
        case FindingCategory::QuarterTrack: text = L"Quarter track"; break;
        case FindingCategory::ImageFile:    text = L"Image file";   break;
        case FindingCategory::FileSystem:   text = L"File system";  break;
        case FindingCategory::Protection:   text = L"Protection";   break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter::FormatDamageReason
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindingFormatter::FormatDamageReason (DamageReason reason)
{
    std::wstring  text;



    switch (reason)
    {
        case DamageReason::OutsideFile:           text = L"its data lies outside the file";                                   break;
        case DamageReason::CountExceedsBlocks:    text = L"its bit or byte count needs more than its blocks hold";            break;
        case DamageReason::TruncatedRun:          text = L"its flux data ends inside a run of 255s";                          break;
        case DamageReason::V1RecordPastTrks:      text = L"the record is past the end of the TRKS chunk";                     break;
        case DamageReason::RecordLocationMissing: text = L"it claims data but gives a start block or block count of zero";    break;
        case DamageReason::RecordInHeader:        text = L"its start block lies inside the file's header";                    break;
        case DamageReason::MapEntryOutOfRange:    text = L"its map entry points past the last track record";                  break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter::FormatFieldFinding
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindingFormatter::FormatFieldFinding (const Finding & finding)
{
    std::wstring  text;
    std::wstring  sector = (finding.sector >= 0) ? L"Sector " + InspectorFormat::FormatSector (finding.sector) + L": " : std::wstring();
    std::wstring  table  = (finding.value == static_cast<int> (DiskFieldKind::Sixteen)) ? L"6-and-2" : L"5-and-3";



    switch (finding.kind)
    {
        case FindingKind::AddressChecksumFailed:
            text = std::format (L"{}address field checksum failed: stored {}, computed {}", sector,
                                InspectorFormat::FormatHexByte (finding.value), InspectorFormat::FormatHexByte (finding.value2));
            break;

        case FindingKind::DataChecksumFailed:
            text = std::format (L"{}data field checksum failed: stored {}, computed {}", sector,
                                InspectorFormat::FormatHexByte (finding.value), InspectorFormat::FormatHexByte (finding.value2));
            break;

        case FindingKind::AddressTrackDiffers:
            text = std::format (L"{}address field gives track {}, on track {}", sector, finding.value, finding.value2);
            break;

        case FindingKind::VolumeDiffers:
            text = std::format (L"{}address field gives volume {}; the disk's most common volume is {}", sector, finding.value, finding.value2);
            break;

        case FindingKind::AddressEpilogueNonstandard:
            text = std::format (L"{}address field is followed by {}, not DE AA", sector, InspectorFormat::FormatByteSequence (finding.bytes));
            break;

        case FindingKind::DataEpilogueNonstandard:
            text = std::format (L"{}data field is followed by {}, not DE AA", sector, InspectorFormat::FormatByteSequence (finding.bytes));
            break;

        case FindingKind::AddressWithoutData:
            text = std::format (L"{}address field has no data field", sector);
            break;

        case FindingKind::DataWithoutAddress:
            text = L"Data field with no address field";
            break;

        case FindingKind::NibblesOutsideTable:
            text = std::format (L"{}data field holds nibbles outside the {} table:", sector, table);

            for (const InvalidNibbleCount & n : finding.nibbles)
            {
                text += std::format (L" {:02X} {}{}", n.value, s_kpszMultiplyX, n.count);
            }

            break;

        default:
            text = FormatTrackFinding (finding);
            break;
    }

    if (!text.empty() && text[0] >= L'a' && text[0] <= L'z')
    {
        text[0] = static_cast<wchar_t> (text[0] - L'a' + L'A');
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter::FormatTrackFinding
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindingFormatter::FormatTrackFinding (const Finding & finding)
{
    static constexpr double  kPermilleToPercent = 10.0;



    std::wstring  text;
    std::wstring  sectors;



    for (int s : finding.sectors)
    {
        sectors += (sectors.empty() ? L"" : L", ") + InspectorFormat::FormatSector (s);
    }

    switch (finding.kind)
    {
        case FindingKind::SectorRepeated:             text = std::format (L"Sector {} appears {} times", InspectorFormat::FormatSector (finding.value), finding.value2); break;
        case FindingKind::SectorsMissing:             text = L"Sectors missing: " + sectors;                                                                         break;
        case FindingKind::SectorNumberOutOfRange:     text = std::format (L"Sector number {} is outside the format's range", InspectorFormat::FormatSector (finding.value)); break;
        case FindingKind::StrayFieldMark:             text = std::format (L"{} outside any field", InspectorFormat::FormatByteSequence (finding.bytes));               break;
        case FindingKind::ThirteenSectorFields:       text = L"13-sector fields on a disk with 16-sector fields";                                                    break;
        case FindingKind::RandomBitsOnFormattedTrack: text = std::format (L"{} random-bit {}, {} cells in all", finding.value, finding.value == 1 ? L"region" : L"regions", InspectorFormat::FormatCount (static_cast<uint64_t> (finding.value2))); break;

        case FindingKind::TrackLengthDiffers:
            text = std::format (L"Track is {} cells, {:+.1f}% from nominal", InspectorFormat::FormatCount (static_cast<uint64_t> (finding.value)), finding.value2 / kPermilleToPercent);
            break;

        case FindingKind::RecordBetweenTracks:
            text = std::format (L"Track record {} is read at {}, between whole tracks, and is neither neighbor's record",
                                finding.slot, InspectorFormat::FormatQuarterTrackList (finding.quarterTracks));
            break;

        case FindingKind::RecordWithoutWholeTrack:
            text = std::format (L"Track record {} is read only between whole tracks, at {}", finding.slot, InspectorFormat::FormatQuarterTrackList (finding.quarterTracks));
            break;

        case FindingKind::RecordAcrossWholeTracks:
            text = std::format (L"Track record {} is read at more than one whole track: {}", finding.slot, InspectorFormat::FormatQuarterTrackList (finding.quarterTracks));
            break;

        default:
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindingFormatter::FormatImageFinding
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindingFormatter::FormatImageFinding (const Finding & finding)
{
    std::wstring  text;
    size_t        tab  = finding.detail.find ('\t');



    switch (finding.kind)
    {
        case FindingKind::DamagedRecord:
            text = std::format (L"Track record {} is damaged: {}", finding.slot, FormatDamageReason (static_cast<DamageReason> (finding.value)));
            break;

        case FindingKind::DamagedMapEntry:
            text = std::format (L"The {} entry for quarter track {} is {}, past the last track record; the entry is damaged",
                                finding.value2 != 0 ? L"FLUX" : L"TMAP", InspectorFormat::FormatQuarterTrack (finding.quarterTrack), finding.value);
            break;

        case FindingKind::ChecksumMismatch:     text = L"The file's checksum does not match its contents; the file is damaged";                                       break;
        case FindingKind::LargestTrackTooSmall: text = std::format (L"INFO gives a largest track of {} blocks, but a track record takes {}", finding.value, finding.value2); break;
        case FindingKind::ImageDateNotRfc3339:  text = L"META image_date \"" + TextEncoding::Utf8ToWide (finding.detail) + L"\" is not an RFC 3339 date";                               break;
        case FindingKind::DuplicateChunk:       text = L"The file holds more than one " + TextEncoding::Utf8ToWide (finding.detail) + L" chunk";                                          break;
        case FindingKind::ChunkOutOfOrder:      text = L"The " + TextEncoding::Utf8ToWide (finding.detail) + L" chunk is out of order";                                                   break;
        case FindingKind::DataPastLastChunk:    text = L"The file holds data past its last chunk";                                                                     break;
        case FindingKind::UnreferencedRecord:   text = std::format (L"Track record {} is not read by any quarter track", finding.value);                                 break;
        case FindingKind::NibTrackWithoutSync:  text = L"Track holds no sync nibbles";                                                                                 break;

        case FindingKind::MetaValueOutsideList:
            text = L"META " + TextEncoding::Utf8ToWide (finding.detail.substr (0, tab)) + L" is \"" + TextEncoding::Utf8ToWide (tab == std::string::npos ? std::string() : finding.detail.substr (tab + 1))
                 + L"\", which is not in the WOZ list";
            break;

        case FindingKind::MapEntryToEmptyRecord:
            text = std::format (L"The {} entry for quarter track {} points at track record {}, which is empty",
                                TextEncoding::Utf8ToWide (finding.detail), InspectorFormat::FormatQuarterTrack (finding.value), finding.value2);
            break;

        default:
            break;
    }

    return text;
}

