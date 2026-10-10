#include "Pch.h"

#include "Devices/Disk/Inspector/ImageDetails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  META value lists, from the WOZ 2.1 reference
//
////////////////////////////////////////////////////////////////////////////////

static constexpr std::string_view s_kLanguages[] =
{
    "English", "Spanish", "French", "German", "Chinese", "Japanese", "Italian",
    "Dutch", "Portuguese", "Danish", "Finnish", "Norwegian", "Swedish",
    "Russian", "Polish", "Turkish", "Arabic", "Thai", "Czech", "Hungarian",
    "Catalan", "Croatian", "Greek", "Hebrew", "Romanian", "Slovak",
    "Ukrainian", "Indonesian", "Malay", "Vietnamese", "Other",
};


static constexpr std::string_view s_kRamSizes[] =
{
    "16K", "24K", "32K", "48K", "64K", "128K", "256K", "512K", "768K", "1M",
    "1.25M", "1.5M+", "Unknown",
};


static constexpr std::string_view s_kMachines[] =
{
    "2", "2+", "2e", "2c", "2e+", "2gs", "2c+", "3", "3+",
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::MakeFromCopy
//
////////////////////////////////////////////////////////////////////////////////

ImageDetails ImageDetails::MakeFromCopy (const DiskCopy & copy)
{
    ImageDetails  details;



    details.fileName   = copy.fileName;
    details.format     = copy.format;
    details.fileSize   = copy.fileSize;
    details.isReadOnly = copy.isReadOnly;
    details.isWoz      = copy.format == DiskFormat::Woz && copy.woz.layout.hasMaps;
    details.info       = copy.woz.info;
    details.layout     = copy.woz.layout;
    details.isCrcMatch = !copy.hasCrcMismatch;

    if (!details.isWoz)
    {
        return details;
    }

    if (copy.hasCrcMismatch)
    {
        AddProblem (details.problems, FindingKind::ChecksumMismatch, static_cast<int> (details.layout.storedCrc), static_cast<int> (details.layout.computedCrc), "");
    }

    CheckLargestTrack (details.layout, details.info, details.problems);
    CheckMeta         (details.layout, details.problems);
    CheckChunks       (details.layout, details.problems);
    CheckRecords      (details.layout, details.problems);

    return details;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::CheckLargestTrack
//
//  INFO's largest track is the most blocks any bit record takes; a record
//  that takes more makes INFO wrong.
//
////////////////////////////////////////////////////////////////////////////////

void ImageDetails::CheckLargestTrack (const WozFileLayout & layout, const WozInfo & info, vector<Finding> & inOut)
{
    int     largest = 0;
    size_t  qt      = 0;



    if (!info.hasVersion2Fields || !layout.isV2)
    {
        return;
    }

    for (qt = 0; qt < layout.tmap.size(); qt++)
    {
        if (layout.tmap[qt] < layout.records.size())
        {
            largest = std::max (largest, static_cast<int> (layout.records[layout.tmap[qt]].blockCount));
        }
    }

    if (largest > info.largestTrack)
    {
        AddProblem (inOut, FindingKind::LargestTrackTooSmall, info.largestTrack, largest, "");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::CheckMeta
//
//  The three META keys whose values come from a list, and image_date, which
//  must be an RFC 3339 date. requires_machine is a list of machines joined by
//  "|". An empty value gives nothing and is not checked.
//
////////////////////////////////////////////////////////////////////////////////

void ImageDetails::CheckMeta (const WozFileLayout & layout, vector<Finding> & inOut)
{
    bool          isValid = true;
    size_t        start   = 0;
    size_t        bar     = 0;



    for (const WozMetaEntry & entry : layout.metaEntries)
    {
        if (entry.value.empty())
        {
            continue;
        }

        isValid = true;

        if (entry.key == "language")
        {
            isValid = IsInList (entry.value, s_kLanguages);
        }
        else if (entry.key == "requires_ram")
        {
            isValid = IsInList (entry.value, s_kRamSizes);
        }
        else if (entry.key == "requires_machine")
        {
            for (start = 0; isValid && start <= entry.value.size(); start = bar + 1)
            {
                bar     = std::min (entry.value.find ('|', start), entry.value.size());
                isValid = IsInList (std::string_view (entry.value).substr (start, bar - start), s_kMachines);
            }
        }
        else if (entry.key == "image_date" && !IsRfc3339 (entry.value))
        {
            AddProblem (inOut, FindingKind::ImageDateNotRfc3339, 0, 0, entry.value);
        }

        if (!isValid)
        {
            AddProblem (inOut, FindingKind::MetaValueOutsideList, 0, 0, entry.key + "\t" + entry.value);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::CheckChunks
//
//  Each of INFO, TMAP, TRKS, FLUX, WRIT and META at most once; INFO first;
//  TMAP before TRKS and FLUX after it; and nothing after the last chunk.
//
////////////////////////////////////////////////////////////////////////////////

void ImageDetails::CheckChunks (const WozFileLayout & layout, vector<Finding> & inOut)
{
    static constexpr std::string_view  kpszOrdered[] = { "INFO", "TMAP", "TRKS", "FLUX" };
    static constexpr std::string_view  kpszOnce[]    = { "INFO", "TMAP", "TRKS", "FLUX", "WRIT", "META" };



    std::string  id;
    int          highest = -1;
    int          rank    = 0;
    size_t       i       = 0;
    size_t       j       = 0;



    for (i = 0; i < layout.chunks.size(); i++)
    {
        id   = std::string (layout.chunks[i].id.begin(), layout.chunks[i].id.end());
        rank = -1;

        for (j = 0; j < std::size (kpszOrdered); j++)
        {
            rank = (id == kpszOrdered[j]) ? static_cast<int> (j) : rank;
        }

        if ((i == 0 && id != "INFO") || (rank >= 0 && rank < highest))
        {
            AddProblem (inOut, FindingKind::ChunkOutOfOrder, static_cast<int> (i), 0, id);
        }

        highest = std::max (highest, rank);

        for (j = 0; j < i; j++)
        {
            bool  isRepeat = layout.chunks[j].id == layout.chunks[i].id
                          && std::find (std::begin (kpszOnce), std::end (kpszOnce), id) != std::end (kpszOnce);

            if (isRepeat)
            {
                AddProblem (inOut, FindingKind::DuplicateChunk, static_cast<int> (i), 0, id);
                break;
            }
        }
    }

    if (layout.hasDataPastLastChunk)
    {
        AddProblem (inOut, FindingKind::DataPastLastChunk, 0, 0, "");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::CheckRecords
//
//  A record that holds something but that no map refers to, and a map
//  entry that points at a record whose fields are all zero (the form the
//  format gives an unused record; not damage).
//
////////////////////////////////////////////////////////////////////////////////

void ImageDetails::CheckRecords (const WozFileLayout & layout, vector<Finding> & inOut)
{
    vector<bool>  isReferenced (layout.records.size(), false);
    size_t        qt          = 0;
    size_t        r           = 0;
    Byte          entry       = 0;



    for (qt = 0; qt < layout.tmap.size(); qt++)
    {
        for (bool isFlux : { false, true })
        {
            entry = isFlux ? layout.flux[qt] : layout.tmap[qt];

            if (entry >= layout.records.size())
            {
                continue;
            }

            isReferenced[entry] = true;

            if (layout.records[entry].startBlock == 0 && layout.records[entry].blockCount == 0 && layout.records[entry].bitOrByteCount == 0)
            {
                AddProblem (inOut, FindingKind::MapEntryToEmptyRecord, static_cast<int> (qt), entry, isFlux ? "FLUX" : "TMAP");
                inOut.back().quarterTrack = static_cast<int> (qt);
            }
        }
    }

    for (r = 0; r < layout.records.size(); r++)
    {
        const WozTrackRecordFields &  rec = layout.records[r];

        if (!isReferenced[r] && (rec.startBlock != 0 || rec.blockCount != 0 || rec.bitOrByteCount != 0))
        {
            AddProblem (inOut, FindingKind::UnreferencedRecord, static_cast<int> (r), 0, "");
            inOut.back().slot = static_cast<int> (r);
        }
    }

    //  A v1 file keeps no record fields to test, so its list is the records
    //  the loader found holding bytes with no map entry.
    for (r = 0; !layout.isV2 && r < layout.unreferenced.size(); r++)
    {
        AddProblem (inOut, FindingKind::UnreferencedRecord, layout.unreferenced[r].index, 0, "");
        inOut.back().slot = layout.unreferenced[r].index;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::IsRfc3339
//
//  YYYY-MM-DDTHH:MM:SS, an optional fraction, then Z or an offset +HH:MM or
//  -HH:MM. The letter T and Z may be lower case, as RFC 3339 allows.
//
////////////////////////////////////////////////////////////////////////////////

bool ImageDetails::IsRfc3339 (std::string_view text)
{
    static constexpr std::string_view  kpszShape  = "dddd-dd-ddTdd:dd:dd";
    static constexpr std::string_view  kpszOffset = "+dd:dd";



    bool    isValid = text.size() > kpszShape.size();
    size_t  i       = 0;
    size_t  pos     = kpszShape.size();



    for (i = 0; isValid && i < kpszShape.size(); i++)
    {
        isValid = (kpszShape[i] == 'd') ? (text[i] >= '0' && text[i] <= '9')
                                        : (std::toupper (static_cast<unsigned char> (text[i])) == kpszShape[i]);
    }

    if (isValid && text[pos] == '.')
    {
        pos++;

        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9')
        {
            pos++;
        }
    }

    if (isValid && pos < text.size() && (text[pos] == 'Z' || text[pos] == 'z'))
    {
        isValid = pos + 1 == text.size();
    }
    else if (isValid && pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
    {
        isValid = text.size() - pos == kpszOffset.size();

        for (i = 1; isValid && i < kpszOffset.size(); i++)
        {
            isValid = (kpszOffset[i] == 'd') ? (text[pos + i] >= '0' && text[pos + i] <= '9') : text[pos + i] == kpszOffset[i];
        }
    }
    else
    {
        isValid = false;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::IsInList
//
////////////////////////////////////////////////////////////////////////////////

bool ImageDetails::IsInList (std::string_view value, std::span<const std::string_view> list)
{
    return std::find (list.begin(), list.end(), value) != list.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails::AddProblem
//
////////////////////////////////////////////////////////////////////////////////

void ImageDetails::AddProblem (vector<Finding> & inOut, FindingKind kind, int value, int value2, const std::string & detail)
{
    Finding  finding;



    finding.category = FindingCategory::ImageFile;
    finding.kind     = kind;
    finding.value    = value;
    finding.value2   = value2;
    finding.detail   = detail;

    inOut.push_back (finding);
}
