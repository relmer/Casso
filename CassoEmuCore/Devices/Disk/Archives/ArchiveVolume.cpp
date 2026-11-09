#include "Pch.h"

#include "Devices/Disk/Archives/ArchiveVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::ArchiveVolume
//
////////////////////////////////////////////////////////////////////////////////

ArchiveVolume::ArchiveVolume (const vector<Byte> & bytes)
{
    if (NuFxArchive::IsNuFx (bytes))
    {
        m_isNuFx = true;
        m_parsed = NuFxArchive::Parse (bytes, m_nufx, m_error);
        return;
    }

    if (IsSelfExtracting (bytes))
    {
        m_isNuFx           = true;
        m_isSelfExtracting = true;
        m_parsed           = NuFxArchive::Parse (std::span<const Byte> (bytes).subspan (kSelfExtractingOffset), m_nufx, m_error);
        return;
    }

    m_parsed = BinaryTwoArchive::Parse (bytes, m_bny, m_error);

    if (SUCCEEDED (m_parsed) && m_bny.size() == 1 && NuFxArchive::IsNuFx (m_bny[0].data))
    {
        m_isNuFx       = true;
        m_wrappedInBny = true;
        m_parsed       = NuFxArchive::Parse (m_bny[0].data, m_nufx, m_error);
    }
    else if (SUCCEEDED (m_parsed) && m_bny.size() == 1 && IsSelfExtracting (m_bny[0].data))
    {
        m_isNuFx           = true;
        m_wrappedInBny     = true;
        m_isSelfExtracting = true;
        m_parsed           = NuFxArchive::Parse (std::span<const Byte> (m_bny[0].data).subspan (kSelfExtractingOffset), m_nufx, m_error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::IsSelfExtracting
//
//  GS ShrinkIt's self-extracting archive: the extractor program, then the
//  archive at a fixed offset.
//
////////////////////////////////////////////////////////////////////////////////

bool ArchiveVolume::IsSelfExtracting (const vector<Byte> & bytes)
{
    return bytes.size() > kSelfExtractingOffset && NuFxArchive::IsNuFx (std::span<const Byte> (bytes).subspan (kSelfExtractingOffset));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::IsArchive
//
////////////////////////////////////////////////////////////////////////////////

bool ArchiveVolume::IsArchive (const vector<Byte> & bytes)
{
    return NuFxArchive::IsNuFx (bytes) || IsSelfExtracting (bytes) || BinaryTwoArchive::IsBinaryTwo (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::SplitName
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ArchiveVolume::SplitName (const std::string & name, char separator)
{
    std::vector<std::string>  parts;
    std::string               part;



    for (char c : name)
    {
        if (c == separator)
        {
            if (!part.empty())
            {
                parts.push_back (part);
            }

            part.clear();
            continue;
        }

        part.push_back (c);
    }

    if (!part.empty())
    {
        parts.push_back (part);
    }

    return parts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::JoinName
//
////////////////////////////////////////////////////////////////////////////////

std::string ArchiveVolume::JoinName (const std::vector<std::string> & components)
{
    std::string  name;



    for (const std::string & component : components)
    {
        name += (name.empty() ? "" : "/") + component;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::IsSameName
//
//  Without regard to case, as ProDOS and GS/OS compare names.
//
////////////////////////////////////////////////////////////////////////////////

bool ArchiveVolume::IsSameName (const std::string & a, const std::string & b)
{
    return a.size() == b.size()
        && std::equal (a.begin(), a.end(), b.begin(), [] (char x, char y) { return toupper ((unsigned char) x) == toupper ((unsigned char) y); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::ToProDosDate
//
//  NuFX's second, minute, hour, year less 1900, day and month from 0 as a
//  ProDOS date and time.
//
////////////////////////////////////////////////////////////////////////////////

void ArchiveVolume::ToProDosDate (const std::array<Byte, 8> & when, Word & outDate, Word & outTime)
{
    constexpr int  kCentury = 100;



    int  year  = when[3] % kCentury;
    int  day   = when[4] + 1;
    int  month = when[5] + 1;



    outDate = (Word) ((year << 9) | (month << 5) | day);
    outTime = (Word) ((when[2] << 8) | when[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::FromProDosDate
//
//  A two-digit year below 40 is this century's, as ProDOS 8 reads it. No date
//  is all zeros, as it is in ProDOS.
//
////////////////////////////////////////////////////////////////////////////////

std::array<Byte, 8> ArchiveVolume::FromProDosDate (Word date, Word time)
{
    constexpr int  kPivot   = 40;
    constexpr int  kCentury = 100;



    std::array<Byte, 8>  when  = {};
    int                  year  = (date >> 9) & 0x7F;
    int                  month = (date >> 5) & 0x0F;
    int                  day   = date & 0x1F;



    if (date == 0 && time == 0)
    {
        return when;
    }

    when[1] = (Byte) (time & 0xFF);
    when[2] = (Byte) (time >> 8);
    when[3] = (Byte) (year < kPivot ? year + kCentury : year);
    when[4] = (Byte) (day > 0 ? day - 1 : 0);
    when[5] = (Byte) (month > 0 ? month - 1 : 0);

    return when;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::GetCount
//
////////////////////////////////////////////////////////////////////////////////

size_t ArchiveVolume::GetCount() const
{
    return m_isNuFx ? m_nufx.records.size() : m_bny.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::GetPath
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ArchiveVolume::GetPath (size_t index) const
{
    if (m_isNuFx)
    {
        const NuFxRecord  & record = m_nufx.records[index];

        return SplitName (NuFxArchive::GetName (record), (char) (record.fsInfo & 0xFF));
    }

    return SplitName (m_bny[index].name, '/');
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::TryFind
//
////////////////////////////////////////////////////////////////////////////////

bool ArchiveVolume::TryFind (const FilePath & path, size_t & outIndex) const
{
    const std::vector<std::string>  & wanted = path.GetComponents();
    size_t                            i      = 0;



    for (i = 0; i < GetCount(); i++)
    {
        std::vector<std::string>  have = GetPath (i);

        if (have.size() == wanted.size()
         && std::equal (have.begin(), have.end(), wanted.begin(), IsSameName))
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::ToEntry
//
//  A record's data fork length, or its disk's, with its resource fork's
//  beside it.
//
////////////////////////////////////////////////////////////////////////////////

FileEntry ArchiveVolume::ToEntry (size_t index, const std::string & leaf) const
{
    constexpr Byte  kWriteEnable = 0x02;



    FileEntry  entry;



    entry.name        = leaf;
    entry.hasAccess   = true;
    entry.hasAuxType  = true;
    entry.hasEofBytes = true;
    entry.hasDates    = true;

    if (m_isNuFx)
    {
        const NuFxRecord  & record = m_nufx.records[index];
        const NuFxThread  * data   = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindDataFork);
        const NuFxThread  * disk   = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindDiskImage);
        const NuFxThread  * fork   = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindResource);

        entry.type     = (Byte) record.fileType;
        entry.auxType  = (Word) record.extraType;
        entry.access   = (Byte) record.access;
        entry.isLocked = (record.access & kWriteEnable) == 0;
        entry.eofBytes = (data != nullptr) ? data->eof
                       : (disk != nullptr) ? record.extraType * (record.storageType == 0 ? 512u : record.storageType)
                                           : 0;

        if (fork != nullptr)
        {
            entry.resourceForkBytes = fork->eof;
            entry.hasResourceFork   = true;
        }

        ToProDosDate (record.created,  entry.createdDate,  entry.createdTime);
        ToProDosDate (record.modified, entry.modifiedDate, entry.modifiedTime);
    }
    else
    {
        const BinaryTwoEntry  & file = m_bny[index];

        entry.type         = file.fileType;
        entry.auxType      = file.auxType;
        entry.access       = file.access;
        entry.isLocked     = (file.access & kWriteEnable) == 0;
        entry.isDirectory  = file.fileType == BinaryTwoEntry::kTypeDirectory;
        entry.eofBytes     = (uint32_t) file.data.size();
        entry.createdDate  = file.createDate;
        entry.createdTime  = file.createTime;
        entry.modifiedDate = file.modDate;
        entry.modifiedTime = file.modTime;
    }

    entry.sizeUnits = (entry.eofBytes + 511) / 512;

    return entry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::Enumerate (VolumeListing & outListing) const
{
    return EnumerateDirectory (FilePath(), outListing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::EnumerateDirectory
//
//  The records one level under the directory, and a folder for each name
//  that continues below it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const
{
    HRESULT                           hr      = m_parsed;
    const std::vector<std::string>  & prefix  = directory.GetComponents();
    std::vector<std::string>          folders;
    size_t                            i       = 0;



    CHR (hr);

    outListing = VolumeListing();

    for (i = 0; i < GetCount(); i++)
    {
        std::vector<std::string>  path = GetPath (i);
        bool                      under = path.size() > prefix.size()
                                       && std::equal (prefix.begin(), prefix.end(), path.begin(), IsSameName);

        if (!under)
        {
            continue;
        }

        if (path.size() == prefix.size() + 1)
        {
            FileEntry  entry = ToEntry (i, path.back());

            if (entry.isDirectory)
            {
                folders.push_back (path.back());
            }

            entry.catalogIndex = outListing.entries.size();
            outListing.entries.push_back (std::move (entry));
            continue;
        }

        if (std::none_of (folders.begin(), folders.end(), [&] (const std::string & f) { return IsSameName (f, path[prefix.size()]); }))
        {
            FileEntry  folder;

            folder.name         = path[prefix.size()];
            folder.isDirectory  = true;
            folder.catalogIndex = outListing.entries.size();

            folders.push_back (folder.name);
            outListing.entries.push_back (std::move (folder));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    constexpr Byte  kTypeBinary = 0x06;



    HRESULT      hr    = m_parsed;
    size_t       index = 0;
    bool         found = false;
    FileEntry    entry;
    std::string  error;



    CHR (hr);

    found = TryFind (path, index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    outPayload = FilePayload();
    entry      = ToEntry (index, path.GetLeaf());

    if (m_isNuFx)
    {
        const NuFxRecord  & record = m_nufx.records[index];
        const NuFxThread  * data   = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindDataFork);
        const NuFxThread  * fork   = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindResource);

        if (data == nullptr)
        {
            data = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindDiskImage);
        }

        if (data != nullptr)
        {
            hr = NuFxArchive::ExpandThread (record, *data, outPayload.bytes, error);
            CHR (hr);
        }

        if (fork != nullptr)
        {
            hr = NuFxArchive::ExpandThread (record, *fork, outPayload.resourceFork, error);
            CHR (hr);

            outPayload.hasResourceFork = true;
        }
    }
    else
    {
        outPayload.bytes = m_bny[index].data;
    }

    outPayload.type           = entry.type;
    outPayload.auxType        = entry.auxType;
    outPayload.hasAuxType     = true;
    outPayload.access         = entry.access;
    outPayload.hasAccess      = true;
    outPayload.createdDate    = entry.createdDate;
    outPayload.createdTime    = entry.createdTime;
    outPayload.modifiedDate   = entry.modifiedDate;
    outPayload.modifiedTime   = entry.modifiedTime;
    outPayload.hasDates       = true;
    outPayload.loadAddress    = entry.auxType;
    outPayload.hasLoadAddress = entry.type == kTypeBinary;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::BuildBack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::BuildBack (const NuFxArchive & nufx, const std::vector<BinaryTwoEntry> & bny, vector<Byte> & outBuffer) const
{
    HRESULT                      hr = S_OK;
    std::vector<BinaryTwoEntry>  wrapper;



    outBuffer.clear();

    CBREx (!m_isSelfExtracting, HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT));

    if (!m_isNuFx)
    {
        BinaryTwoArchive::Build (bny, outBuffer);
        BAIL_OUT_IF (true, S_OK);
    }

    nufx.Build (outBuffer);

    if (m_wrappedInBny)
    {
        wrapper         = { m_bny[0] };
        wrapper[0].data = std::move (outBuffer);

        BinaryTwoArchive::Build (wrapper, outBuffer);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::Write
//
//  A record of the same name is replaced in place; a new one goes last. A new
//  NuFX record is version 3, its forks LZW/2-compressed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    constexpr Word  kStorageSeedling = 1;
    constexpr Word  kStorageExtended = 5;



    HRESULT                      hr     = m_parsed;
    size_t                       index  = 0;
    bool                         exists = false;
    bool                         named  = !path.IsEmpty();
    std::string                  name   = JoinName (path.GetComponents());
    Word                         aux    = payload.hasLoadAddress ? payload.loadAddress : payload.auxType;
    NuFxArchive                  nufx   = m_nufx;
    std::vector<BinaryTwoEntry>  bny    = m_bny;



    CHR (hr);
    CBREx (named, E_INVALIDARG);

    exists = TryFind (path, index);

    if (m_isNuFx)
    {
        NuFxRecord  record;

        record.fileType    = payload.type;
        record.extraType   = aux;
        record.access      = payload.hasAccess ? payload.access : kDefaultAccess;
        record.storageType = payload.hasResourceFork ? kStorageExtended : kStorageSeedling;
        record.created     = FromProDosDate (payload.createdDate,  payload.createdTime);
        record.modified    = FromProDosDate (payload.modifiedDate, payload.modifiedTime);
        record.threads.push_back (NuFxArchive::MakeFilenameThread (name));
        record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDataFork, payload.bytes));

        if (payload.hasResourceFork)
        {
            record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindResource, payload.resourceFork));
        }

        if (exists)
        {
            nufx.records[index] = std::move (record);
        }
        else
        {
            nufx.records.push_back (std::move (record));
        }
    }
    else
    {
        BinaryTwoEntry  entry;

        entry.name       = name;
        entry.fileType   = payload.type;
        entry.auxType    = aux;
        entry.access     = payload.hasAccess ? payload.access : kDefaultAccess;
        entry.modDate    = payload.modifiedDate;
        entry.modTime    = payload.modifiedTime;
        entry.createDate = payload.createdDate;
        entry.createTime = payload.createdTime;
        entry.data       = payload.bytes;

        if (exists)
        {
            bny[index] = std::move (entry);
        }
        else
        {
            bny.push_back (std::move (entry));
        }
    }

    hr = BuildBack (nufx, bny, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  outcome;



    return Delete (path, outBuffer, outcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    HRESULT                      hr    = m_parsed;
    size_t                       index = 0;
    bool                         found = false;
    NuFxArchive                  nufx  = m_nufx;
    std::vector<BinaryTwoEntry>  bny   = m_bny;



    CHR (hr);

    outOutcome = DeleteOutcome();

    found = TryFind (path, index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    if (m_isNuFx)
    {
        nufx.records.erase (nufx.records.begin() + (ptrdiff_t) index);
    }
    else
    {
        bny.erase (bny.begin() + (ptrdiff_t) index);
    }

    hr = BuildBack (nufx, bny, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::CreateDirectory
//
//  A Binary II archive has directory entries; a NuFX archive has none, so a
//  folder is made there by putting a file in it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::CreateDirectory (const FilePath & path, vector<Byte> & outBuffer) const
{
    HRESULT                      hr     = m_parsed;
    size_t                       index  = 0;
    bool                         exists = false;
    std::vector<BinaryTwoEntry>  bny    = m_bny;
    BinaryTwoEntry               folder;



    CHR (hr);
    CBREx (!m_isNuFx, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    exists = TryFind (path, index);
    CBREx (!exists, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    folder.name        = JoinName (path.GetComponents());
    folder.fileType    = BinaryTwoEntry::kTypeDirectory;
    folder.storageType = 0x0D;
    bny.push_back (folder);

    hr = BuildBack (m_nufx, bny, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::BuildRemovalPlan
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const
{
    constexpr Byte  kWriteEnable = 0x02;



    HRESULT                           hr     = m_parsed;
    const std::vector<std::string>  & prefix = path.GetComponents();
    size_t                            i      = 0;



    CHR (hr);

    outPlan = DirectoryRemovalPlan();

    for (i = 0; i < GetCount(); i++)
    {
        std::vector<std::string>  have  = GetPath (i);
        FileEntry                 entry;
        DirectoryRemovalEntry     listed;

        if (have.size() <= prefix.size() || !std::equal (prefix.begin(), prefix.end(), have.begin(), IsSameName))
        {
            continue;
        }

        entry = ToEntry (i, have.back());

        listed.path        = JoinName (have);
        listed.isDirectory = entry.isDirectory;
        listed.isLocked    = (entry.access & kWriteEnable) == 0;
        listed.blocks      = entry.sizeUnits;

        outPlan.hasLockedEntries = outPlan.hasLockedEntries || listed.isLocked;
        outPlan.blocksFreed     += listed.blocks;
        outPlan.entries.push_back (listed);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::RemoveDirectory
//
//  Every record under the folder, and its own entry in a Binary II archive.
//  A locked record stops the removal unless it is forced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::RemoveDirectory (const FilePath & path, bool force, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    HRESULT                           hr     = m_parsed;
    const std::vector<std::string>  & prefix = path.GetComponents();
    DirectoryRemovalPlan              plan;
    NuFxArchive                       nufx;
    std::vector<BinaryTwoEntry>       bny;
    size_t                            i      = 0;



    CHR (hr);

    outOutcome = DeleteOutcome();

    hr = BuildRemovalPlan (path, plan);
    CHR (hr);

    CBREx (force || !plan.hasLockedEntries, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    nufx = m_nufx;
    nufx.records.clear();

    for (i = 0; i < GetCount(); i++)
    {
        std::vector<std::string>  have = GetPath (i);
        bool                      gone = have.size() >= prefix.size() && std::equal (prefix.begin(), prefix.end(), have.begin(), IsSameName);

        if (gone)
        {
            continue;
        }

        if (m_isNuFx)
        {
            nufx.records.push_back (m_nufx.records[i]);
        }
        else
        {
            bny.push_back (m_bny[i]);
        }
    }

    hr = BuildBack (nufx, bny, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::BuildIntegrityReport
//
//  An archive has no blocks to claim, so the report has no units, and its
//  catalog parsed when the archive did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    outReport.Reset (0);
    outReport.SetCatalogFullyParsed (SUCCEEDED (m_parsed));
    outReport.Finish();

    return m_parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    HRESULT                      hr      = m_parsed;
    size_t                       index   = 0;
    size_t                       other   = 0;
    bool                         found   = false;
    bool                         taken   = false;
    bool                         named   = !to.empty() && to.find ('/') == std::string::npos;
    std::vector<std::string>     renamed = from.GetComponents();
    NuFxArchive                  nufx    = m_nufx;
    std::vector<BinaryTwoEntry>  bny     = m_bny;



    CHR (hr);
    CBREx (named, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    found = TryFind (from, index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    renamed.back() = to;

    taken = TryFind (FilePath::Parse (JoinName (renamed)), other) && other != index;
    CBREx (!taken, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    if (m_isNuFx)
    {
        NuFxRecord  & record = nufx.records[index];
        auto          name   = std::find_if (record.threads.begin(), record.threads.end(),
                                             [] (const NuFxThread & t) { return t.threadClass == NuFxThread::kClassFilename; });

        if (name != record.threads.end())
        {
            *name = NuFxArchive::MakeFilenameThread (JoinName (renamed));
        }
        else
        {
            record.headerName = JoinName (renamed);
        }

        record.fsInfo = '/';
    }
    else
    {
        bny[index].name = JoinName (renamed);
    }

    hr = BuildBack (nufx, bny, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArchiveVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    HRESULT                      hr    = m_parsed;
    size_t                       index = 0;
    bool                         found = false;
    NuFxArchive                  nufx  = m_nufx;
    std::vector<BinaryTwoEntry>  bny   = m_bny;



    CHR (hr);

    found = TryFind (path, index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    if (m_isNuFx)
    {
        NuFxRecord  & record = nufx.records[index];

        record.fileType  = attributes.hasType    ? attributes.type    : record.fileType;
        record.extraType = attributes.hasAuxType ? attributes.auxType : record.extraType;
        record.access    = attributes.hasAccess  ? attributes.access  : record.access;
        record.created   = attributes.hasCreated  ? FromProDosDate (attributes.createdDate,  attributes.createdTime)  : record.created;
        record.modified  = attributes.hasModified ? FromProDosDate (attributes.modifiedDate, attributes.modifiedTime) : record.modified;
    }
    else
    {
        BinaryTwoEntry  & entry = bny[index];

        entry.fileType   = attributes.hasType    ? attributes.type    : entry.fileType;
        entry.auxType    = attributes.hasAuxType ? attributes.auxType : entry.auxType;
        entry.access     = attributes.hasAccess  ? attributes.access  : entry.access;
        entry.createDate = attributes.hasCreated  ? attributes.createdDate  : entry.createDate;
        entry.createTime = attributes.hasCreated  ? attributes.createdTime  : entry.createTime;
        entry.modDate    = attributes.hasModified ? attributes.modifiedDate : entry.modDate;
        entry.modTime    = attributes.hasModified ? attributes.modifiedTime : entry.modTime;
    }

    hr = BuildBack (nufx, bny, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume::CollectDamage
//
//  Each record whose header CRC failed, and each thread that doesn't expand
//  or doesn't match its CRC, as Test reports them.
//
////////////////////////////////////////////////////////////////////////////////

void ArchiveVolume::CollectDamage (std::vector<std::string> & outDamage) const
{
    std::vector<Byte>  expanded;
    std::string        error;



    outDamage.clear();

    if (FAILED (m_parsed))
    {
        outDamage.push_back (m_error);
        return;
    }

    for (const NuFxRecord & record : m_nufx.records)
    {
        std::string  name = NuFxArchive::GetName (record);

        if (!record.headerCrcOk)
        {
            outDamage.push_back (name + ": the record header doesn't match its CRC.");
        }

        for (const NuFxThread & thread : record.threads)
        {
            if (thread.threadClass != NuFxThread::kClassData)
            {
                continue;
            }

            HRESULT  hr = NuFxArchive::ExpandThread (record, thread, expanded, error);

            if (FAILED (hr))
            {
                outDamage.push_back (name + ": " + error);
            }
        }
    }
}
