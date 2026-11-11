#include "Pch.h"

#include "Machines/Apple2/Common/HfsVolume.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::HfsVolume
//
////////////////////////////////////////////////////////////////////////////////

HfsVolume::HfsVolume (const BlockImage & image)
    : m_image (image)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool HfsVolume::IsFound (const BlockImage & image)
{
    HfsVolume  volume (image);
    Mdb        mdb;
    HRESULT    hr = volume.ReadMdb (mdb);



    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::MapToProDos
//
////////////////////////////////////////////////////////////////////////////////

void HfsVolume::MapToProDos (uint32_t macType, uint32_t macCreator, Byte & outType, Word & outAux)
{
    static constexpr uint32_t  s_kPdos = 0x70646F73;   // 'pdos'
    static constexpr uint32_t  s_kPsys = 0x50535953;   // 'PSYS'
    static constexpr uint32_t  s_kPs16 = 0x50533136;   // 'PS16'
    static constexpr uint32_t  s_kText = 0x54455854;   // 'TEXT'
    static constexpr Byte      s_kP    = 'p';



    outType = 0x00;
    outAux  = 0x0000;

    if (macCreator == s_kPdos && (Byte) (macType >> 24) == s_kP)
    {
        outType = (Byte) (macType >> 16);
        outAux  = (Word) macType;
    }
    else if (macType == s_kPsys)
    {
        outType = 0xFF;
    }
    else if (macType == s_kPs16)
    {
        outType = 0xB3;
    }
    else if (macType == s_kText)
    {
        outType = 0x04;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::DecodeName
//
////////////////////////////////////////////////////////////////////////////////

std::string HfsVolume::DecodeName (const Byte * name, size_t length)
{
    wchar_t  wide[kMaxNameLength * 2 + 1] = {};
    int      count                        = 0;



    length = std::min (length, kMaxNameLength);
    count  = MultiByteToWideChar (10000, 0, (const char *) name, (int) length, wide, (int) ARRAYSIZE (wide) - 1);

    return TextEncoding::WideToUtf8 (std::wstring (wide, (size_t) std::max (count, 0)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::ReadExtents
//
////////////////////////////////////////////////////////////////////////////////

void HfsVolume::ReadExtents (const vector<Byte> & bytes, size_t at, Extent (& outExtents)[3])
{
    for (size_t i = 0; i < 3; i++)
    {
        outExtents[i].start = GetWord (bytes, at + i * 4);
        outExtents[i].count = GetWord (bytes, at + i * 4 + 2);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::ReadMdb
//
//  Fails with ERROR_BAD_FORMAT for a block 2 with no signature, or an
//  allocation that doesn't fit the image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::ReadMdb (Mdb & outMdb) const
{
    HRESULT       hr       = S_OK;
    vector<Byte>  block (kSectorBytes);
    bool          sound    = false;
    bool          isLarge  = m_image.size() >= kMdbOffset + kSectorBytes;
    bool          isSigned = false;
    size_t        length   = 0;



    outMdb = Mdb();

    CBREx (isLarge, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    for (size_t i = 0; i < kSectorBytes; i++)
    {
        block[i] = ReadByte (kMdbOffset + i);
    }

    isSigned = GetWord (block, 0) == kSignature;
    CBREx (isSigned, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    outMdb.volumeBitmapStart = GetWord (block, 14);
    outMdb.allocationBlocks  = GetWord (block, 18);
    outMdb.allocationBytes   = GetLong (block, 20);
    outMdb.firstBlock        = GetWord (block, 28);
    outMdb.freeBlocks        = GetWord (block, 34);
    length                   = std::min<size_t> (block[36], 27);
    outMdb.name              = DecodeName (&block[37], length);
    outMdb.extentsFileBytes  = GetLong (block, 130);
    ReadExtents (block, 134, outMdb.extentsFile);
    outMdb.catalogFileBytes  = GetLong (block, 146);
    ReadExtents (block, 150, outMdb.catalogFile);

    sound = outMdb.allocationBytes >= kSectorBytes && outMdb.allocationBytes % kSectorBytes == 0 && outMdb.allocationBlocks > 0 &&
            (uint64_t) outMdb.firstBlock * kSectorBytes + (uint64_t) outMdb.allocationBlocks * outMdb.allocationBytes <= m_image.size() + outMdb.allocationBytes;
    CBREx (sound, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::GetForkExtents
//
//  The first three extents, then those of the extents overflow file for the
//  same file and fork, in order of their first allocation block, until the
//  fork's physical length is covered.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::GetForkExtents (const Mdb & mdb, uint32_t fileId, Byte fork, const Extent (& first)[3], uint32_t physicalBytes, vector<Extent> & outExtents) const
{
    HRESULT                         hr       = S_OK;
    uint64_t                        needed   = (physicalBytes + (uint64_t) mdb.allocationBytes - 1) / mdb.allocationBytes;
    uint64_t                        covered  = 0;
    std::map<Word, vector<Extent>>  byStart;



    outExtents.clear();

    for (const Extent & extent : first)
    {
        if (extent.count != 0)
        {
            outExtents.push_back (extent);
            covered += extent.count;
        }
    }

    BAIL_OUT_IF (covered >= needed || fileId == kExtentsFileId, S_OK);

    {
        vector<Extent>  extentsFile;
        Extent          mdbExtents[3];

        std::copy (std::begin (mdb.extentsFile), std::end (mdb.extentsFile), std::begin (mdbExtents));

        for (const Extent & extent : mdbExtents)
        {
            if (extent.count != 0)
            {
                extentsFile.push_back (extent);
            }
        }

        hr = ReadLeafRecords (mdb, extentsFile, [&] (const vector<Byte> & node, size_t key, size_t keyEnd)
        {
            Extent  three[3];

            if (node[key] >= 7 && node[key + 1] == fork && GetLong (node, key + 2) == fileId && keyEnd + 12 <= node.size())
            {
                ReadExtents (node, keyEnd, three);
                byStart[GetWord (node, key + 6)].assign (std::begin (three), std::end (three));
            }
        });
        CHR (hr);
    }

    for (const auto & [start, extents] : byStart)
    {
        for (const Extent & extent : extents)
        {
            if (extent.count != 0 && covered < needed)
            {
                outExtents.push_back (extent);
                covered += extent.count;
            }
        }
    }

    CBREx (covered >= needed, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::ReadForkBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::ReadForkBytes (const Mdb & mdb, const vector<Extent> & extents, uint64_t offset, size_t length, vector<Byte> & outBytes) const
{
    HRESULT   hr        = S_OK;
    uint64_t  forkStart = 0;
    size_t    done      = 0;



    outBytes.assign (length, 0);

    for (const Extent & extent : extents)
    {
        uint64_t  extentBytes = (uint64_t) extent.count * mdb.allocationBytes;
        uint64_t  imageStart  = (uint64_t) mdb.firstBlock * kSectorBytes + (uint64_t) extent.start * mdb.allocationBytes;

        while (done < length && offset + done < forkStart + extentBytes)
        {
            uint64_t  within = offset + done - forkStart;
            size_t    run    = (size_t) std::min<uint64_t> (length - done, extentBytes - within);
            bool      inside = imageStart + within + run <= m_image.size();

            CBREx (inside, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

            for (size_t i = 0; i < run; i++)
            {
                outBytes[done + i] = m_image[(size_t) (imageStart + within + i)];
            }

            done += run;
        }

        forkStart += extentBytes;
    }

    CBREx (done == length, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    hr = m_image.GetReadError();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::ReadNode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::ReadNode (const Mdb & mdb, const vector<Extent> & extents, uint32_t node, vector<Byte> & outNode) const
{
    return ReadForkBytes (mdb, extents, (uint64_t) node * kNodeBytes, kNodeBytes, outNode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::ReadLeafRecords
//
//  Each record of each leaf node, from the first leaf the header node has,
//  along the forward links. `key` is the record's key length byte, and
//  `keyEnd` where its data starts, at an even offset.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::ReadLeafRecords (const Mdb & mdb, const vector<Extent> & extents, const std::function<void (const vector<Byte> & node, size_t key, size_t keyEnd)> & visit) const
{
    HRESULT       hr    = S_OK;
    vector<Byte>  node;
    uint32_t      next  = 0;
    int           steps = 0;



    hr = ReadNode (mdb, extents, 0, node);
    CHR (hr);

    next = GetLong (node, 24);

    while (next != 0)
    {
        Word  records = 0;

        CBREx (++steps <= kMaxNodes, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        hr = ReadNode (mdb, extents, next, node);
        CHR (hr);

        CBREx (node[8] == kNodeLeaf, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        records = GetWord (node, 10);

        for (Word i = 0; i < records && kNodeBytes >= 2 * ((size_t) i + 1); i++)
        {
            size_t  key    = GetWord (node, kNodeBytes - 2 * ((size_t) i + 1));
            size_t  keyEnd = 0;

            if (key + 1 >= kNodeBytes || node[key] == 0)
            {
                continue;
            }

            keyEnd = key + 1 + node[key];
            keyEnd += keyEnd % 2;

            if (keyEnd < kNodeBytes)
            {
                visit (node, key, keyEnd);
            }
        }

        next = GetLong (node, 0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::LoadCatalog
//
//  Every directory and file record. Threads are left out.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::LoadCatalog (const Mdb & mdb, vector<CatalogRecord> & outRecords) const
{
    HRESULT         hr = S_OK;
    vector<Extent>  catalog;



    outRecords.clear();

    hr = GetForkExtents (mdb, kCatalogFileId, kForkData, mdb.catalogFile, mdb.catalogFileBytes, catalog);
    CHR (hr);

    hr = ReadLeafRecords (mdb, catalog, [&] (const vector<Byte> & node, size_t key, size_t keyEnd)
    {
        CatalogRecord  record;
        size_t         length = std::min<size_t> (node[key + 6], kMaxNameLength);
        Byte           type   = node[keyEnd];

        if (key + 7 + length > kNodeBytes)
        {
            return;
        }

        record.parentId = GetLong (node, key + 2);
        record.name     = DecodeName (&node[key + 7], length);
        record.type     = type;

        if (type == kRecordDirectory && keyEnd + 70 <= kNodeBytes)
        {
            record.id       = GetLong (node, keyEnd + 6);
            record.created  = GetLong (node, keyEnd + 10);
            record.modified = GetLong (node, keyEnd + 14);
            outRecords.push_back (std::move (record));
        }
        else if (type == kRecordFile && keyEnd + 102 <= kNodeBytes)
        {
            record.isLocked     = (node[keyEnd + 2] & 0x01) != 0;
            record.macType      = GetLong (node, keyEnd + 4);
            record.macCreator   = GetLong (node, keyEnd + 8);
            record.id           = GetLong (node, keyEnd + 20);
            record.dataBytes    = GetLong (node, keyEnd + 26);
            record.dataPhysical = GetLong (node, keyEnd + 30);
            record.forkBytes    = GetLong (node, keyEnd + 36);
            record.forkPhysical = GetLong (node, keyEnd + 40);
            record.created      = GetLong (node, keyEnd + 44);
            record.modified     = GetLong (node, keyEnd + 48);
            ReadExtents (node, keyEnd + 74, record.dataExtents);
            ReadExtents (node, keyEnd + 86, record.forkExtents);

            record.finderInfo.assign (node.begin() + (ptrdiff_t) (keyEnd + 4), node.begin() + (ptrdiff_t) (keyEnd + 20));
            record.finderInfo.insert (record.finderInfo.end(), node.begin() + (ptrdiff_t) (keyEnd + 56), node.begin() + (ptrdiff_t) (keyEnd + 72));

            outRecords.push_back (std::move (record));
        }
    });
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::FindDirectory
//
//  A name matches without regard to case, as in the Finder.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::FindDirectory (const vector<CatalogRecord> & records, const FilePath & path, uint32_t & outId) const
{
    HRESULT  hr = S_OK;



    outId = kRootId;

    for (const std::string & component : path.GetComponents())
    {
        std::wstring  wanted = TextEncoding::Utf8ToWide (component);
        bool          found  = false;

        for (const CatalogRecord & record : records)
        {
            std::wstring  name = TextEncoding::Utf8ToWide (record.name);

            if (!found && record.type == kRecordDirectory && record.parentId == outId &&
                CompareStringOrdinal (name.c_str(), (int) name.size(), wanted.c_str(), (int) wanted.size(), TRUE) == CSTR_EQUAL)
            {
                outId = record.id;
                found = true;
            }
        }

        CBREx (found, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::ListDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::ListDirectory (const Mdb & mdb, const vector<CatalogRecord> & records, uint32_t id, VolumeListing & outListing) const
{
    outListing               = VolumeListing();
    outListing.volumeName    = mdb.name;
    outListing.hasVolumeName = true;
    outListing.totalUnits    = (uint32_t) ((uint64_t) mdb.allocationBlocks * mdb.allocationBytes / kSectorBytes);
    outListing.freeUnits     = (uint32_t) ((uint64_t) mdb.freeBlocks * mdb.allocationBytes / kSectorBytes);

    for (const CatalogRecord & record : records)
    {
        FileEntry  entry;

        if (record.parentId != id)
        {
            continue;
        }

        entry.name         = record.name;
        entry.isDirectory  = record.type == kRecordDirectory;
        entry.modifiedUnix = (int64_t) record.modified - kMacToUnixSeconds;
        entry.hasModified  = record.modified != 0;
        entry.catalogIndex = outListing.entries.size();

        if (!entry.isDirectory)
        {
            MapToProDos (record.macType, record.macCreator, entry.type, entry.auxType);

            entry.hasAuxType        = true;
            entry.isLocked          = record.isLocked;
            entry.eofBytes          = record.dataBytes;
            entry.hasEofBytes       = true;
            entry.sizeUnits         = (uint32_t) (((uint64_t) record.dataPhysical + record.forkPhysical) / kSectorBytes);
            entry.resourceForkBytes = record.forkBytes;
            entry.hasResourceFork   = record.forkBytes > 0;
        }
        else
        {
            entry.type = 0x0F;
        }

        outListing.entries.push_back (std::move (entry));
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::Enumerate (VolumeListing & outListing) const
{
    return EnumerateDirectory (FilePath(), outListing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::EnumerateDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const
{
    HRESULT                hr = S_OK;
    Mdb                    mdb;
    vector<CatalogRecord>  records;
    uint32_t               id = kRootId;



    outListing = VolumeListing();

    hr = ReadMdb (mdb);
    CHR (hr);

    hr = LoadCatalog (mdb, records);
    CHR (hr);

    hr = FindDirectory (records, directory, id);
    CHR (hr);

    hr = ListDirectory (mdb, records, id, outListing);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::Read
//
//  Both forks, the Finder information as FInfo then FXInfo, and the ProDOS
//  type and aux type GS/OS maps the file to.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT                     hr        = S_OK;
    Mdb                         mdb;
    vector<CatalogRecord>       records;
    uint32_t                    parent    = kRootId;
    const CatalogRecord       * file      = nullptr;
    std::vector<std::string>    parts     = path.GetComponents();
    bool                        hasName   = !parts.empty();
    std::wstring                wanted;
    vector<Extent>              extents;
    std::string                 directory;



    outPayload = FilePayload();

    CBREx (hasName, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = ReadMdb (mdb);
    CHR (hr);

    hr = LoadCatalog (mdb, records);
    CHR (hr);

    //  An HFS name may contain a '/', so a path that doesn't resolve as folders
    //  is looked up again with its last components joined into one name.
    for (size_t split = parts.size() - 1; file == nullptr && split != SIZE_MAX; split--)
    {
        std::string  leaf;
        HRESULT      hrFound = S_OK;

        directory.clear();

        for (size_t i = 0; i < split; i++)
        {
            directory += (directory.empty() ? "" : "/") + parts[i];
        }

        for (size_t i = split; i < parts.size(); i++)
        {
            leaf += (leaf.empty() ? "" : "/") + parts[i];
        }

        hrFound = FindDirectory (records, FilePath::Parse (directory), parent);

        if (FAILED (hrFound))
        {
            continue;
        }

        wanted = TextEncoding::Utf8ToWide (leaf);

        for (const CatalogRecord & record : records)
        {
            std::wstring  name = TextEncoding::Utf8ToWide (record.name);

            if (file == nullptr && record.type == kRecordFile && record.parentId == parent &&
                CompareStringOrdinal (name.c_str(), (int) name.size(), wanted.c_str(), (int) wanted.size(), TRUE) == CSTR_EQUAL)
            {
                file = &record;
            }
        }
    }

    CBREx (file != nullptr, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = GetForkExtents (mdb, file->id, kForkData, file->dataExtents, file->dataPhysical, extents);
    CHR (hr);

    hr = ReadForkBytes (mdb, extents, 0, file->dataBytes, outPayload.bytes);
    CHR (hr);

    if (file->forkBytes > 0)
    {
        hr = GetForkExtents (mdb, file->id, kForkResource, file->forkExtents, file->forkPhysical, extents);
        CHR (hr);

        hr = ReadForkBytes (mdb, extents, 0, file->forkBytes, outPayload.resourceFork);
        CHR (hr);

        outPayload.hasResourceFork = true;
    }

    MapToProDos (file->macType, file->macCreator, outPayload.type, outPayload.auxType);

    outPayload.hasAuxType = true;
    outPayload.finderInfo = file->finderInfo;
    outPayload.access     = file->isLocked ? FilePayload::kAccessLocked : FilePayload::kAccessUnlocked;
    outPayload.hasAccess  = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::BuildIntegrityReport
//
//  Each fork's allocation blocks counted against its file, the catalog and
//  extents files' by theirs, and the volume bitmap's bits, a set bit being
//  an allocated block.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT                hr    = S_OK;
    Mdb                    mdb;
    vector<CatalogRecord>  records;
    vector<Extent>         extents;
    uint16_t               owner = 1;



    hr = ReadMdb (mdb);
    CHR (hr);

    outReport.Reset (mdb.allocationBlocks);

    for (Word block = 0; block < mdb.allocationBlocks; block++)
    {
        Byte  bits = ReadByte ((size_t) mdb.volumeBitmapStart * kSectorBytes + block / 8);

        outReport.SetAllocatedInFreeMap (block, (bits & (0x80 >> (block % 8))) != 0);
    }

    for (const auto & [first, bytes] : { std::pair<const Extent *, uint32_t> (mdb.extentsFile, mdb.extentsFileBytes),
                                         std::pair<const Extent *, uint32_t> (mdb.catalogFile, mdb.catalogFileBytes) })
    {
        for (size_t i = 0; i < 3; i++)
        {
            for (Word block = 0; block < first[i].count; block++)
            {
                outReport.AddClaim ((uint32_t) first[i].start + block, owner);
            }
        }

        owner++;
    }

    hr = LoadCatalog (mdb, records);

    if (FAILED (hr))
    {
        outReport.SetCatalogFullyParsed (false);
        outReport.Finish();
        BAIL_OUT_IF (true, S_OK);
    }

    for (const CatalogRecord & record : records)
    {
        if (record.type != kRecordFile)
        {
            continue;
        }

        for (const auto & [fork, first, physical] : { std::tuple<Byte, const Extent *, uint32_t> (kForkData, record.dataExtents, record.dataPhysical),
                                                      std::tuple<Byte, const Extent *, uint32_t> (kForkResource, record.forkExtents, record.forkPhysical) })
        {
            Extent   three[3] = { first[0], first[1], first[2] };
            HRESULT  hrFork   = GetForkExtents (mdb, record.id, fork, three, physical, extents);

            if (FAILED (hrFork))
            {
                outReport.MarkChainUnfollowable (owner);
                continue;
            }

            for (const Extent & extent : extents)
            {
                for (Word block = 0; block < extent.count; block++)
                {
                    outReport.AddClaim ((uint32_t) extent.start + block, owner);
                }
            }
        }

        owner = (uint16_t) std::min<uint32_t> ((uint32_t) owner + 1, UINT16_MAX);
    }

    outReport.SetCatalogFullyParsed (true);
    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::Write
//
//  Every change fails until HFS writes are built.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::Write (const FilePath &, const FilePayload &, vector<Byte> &) const
{
    return HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::Delete (const FilePath &, vector<Byte> &) const
{
    return HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::Delete (const FilePath &, vector<Byte> &, DeleteOutcome &) const
{
    return HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::SetStartupProgram (const FilePath &, vector<Byte> &) const
{
    return HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::Rename (const FilePath &, const std::string &, vector<Byte> &) const
{
    return HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsVolume::SetAttributes (const FilePath &, const FileAttributes &, vector<Byte> &) const
{
    return HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
}
