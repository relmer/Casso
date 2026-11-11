#include "Pch.h"

#include "Machines/Apple2/Common/HfsWriter.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/HfsVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::PutWord
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::PutWord (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    bytes[at]     = (Byte) (value >> 8);
    bytes[at + 1] = (Byte) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::PutLong
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::PutLong (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    PutWord (bytes, at,     value >> 16);
    PutWord (bytes, at + 2, value & 0xFFFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::GetMacNow
//
//  The local time as seconds since 1904, as HFS dates are.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsWriter::GetMacNow()
{
    time_t  now   = time (nullptr);
    tm      local = {};



    localtime_s (&local, &now);

    return (uint32_t) (_mkgmtime (&local) + kMacToUnixSeconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::EncodeName
//
////////////////////////////////////////////////////////////////////////////////

bool HfsWriter::EncodeName (const std::string & name, std::vector<Byte> & outName)
{
    std::wstring  wide        = TextEncoding::Utf8ToWide (name);
    char          bytes[64]   = {};
    BOOL          substituted = FALSE;
    int           count       = 0;



    outName.clear();

    if (wide.empty() || wide.find (L':') != std::wstring::npos)
    {
        return false;
    }

    count = WideCharToMultiByte (10000, WC_NO_BEST_FIT_CHARS, wide.c_str(), (int) wide.size(), bytes, (int) sizeof (bytes), nullptr, &substituted);

    if (count <= 0 || count > (int) kMaxNameLength || substituted)
    {
        return false;
    }

    outName.assign (bytes, bytes + count);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::IsValidName
//
////////////////////////////////////////////////////////////////////////////////

bool HfsWriter::IsValidName (const std::string & name)
{
    std::vector<Byte>  encoded;



    return EncodeName (name, encoded);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::ReadCatalogRecords
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::ReadCatalogRecords (std::vector<Byte> & image, std::vector<HfsBTree::Record> & outRecords)
{
    HRESULT  hr = S_OK;
    State    state;



    outRecords.clear();

    hr = Load (image, state);
    CHR (hr);

    outRecords = std::move (state.catalog);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::MakeCatalogKey
//
//  Its length, a reserved byte, the parent ID and the name.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> HfsWriter::MakeCatalogKey (uint32_t parent, const std::vector<Byte> & name)
{
    std::vector<Byte>  key (7 + name.size(), 0);



    key[0] = (Byte) (6 + name.size());
    PutLong (key, 2, parent);
    key[6] = (Byte) name.size();
    std::copy (name.begin(), name.end(), key.begin() + 7);

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::MapFromProDos
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::MapFromProDos (Byte type, Word auxType, uint32_t & outMacType, uint32_t & outCreator)
{
    static constexpr uint32_t  s_kPdos = 0x70646F73;   // 'pdos'
    static constexpr uint32_t  s_kPsys = 0x50535953;   // 'PSYS'
    static constexpr uint32_t  s_kPs16 = 0x50533136;   // 'PS16'
    static constexpr uint32_t  s_kText = 0x54455854;   // 'TEXT'
    static constexpr uint32_t  s_kP    = 0x70000000;   // 'p' in the high byte



    outCreator = s_kPdos;

    if (type == 0x04)
    {
        outMacType = s_kText;
    }
    else if (type == 0xFF)
    {
        outMacType = s_kPsys;
    }
    else if (type == 0xB3)
    {
        outMacType = s_kPs16;
    }
    else
    {
        outMacType = s_kP | ((uint32_t) type << 16) | auxType;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Load
//
//  The master directory block's fields this class changes, and both trees'
//  records. A tree file with extents in the overflow file isn't written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Load (std::vector<Byte> & image, State & outState)
{
    HRESULT  hr       = S_OK;
    bool     sized    = image.size() >= kMdbOffset + kSectorBytes;
    bool     isSigned = false;



    outState       = State();
    outState.image = &image;

    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    isSigned = GetWord (image, kMdbOffset) == kSignature;
    CBREx (isSigned, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    outState.bitmapStart      = GetWord (image, kMdbOffset + 14);
    outState.allocationBlocks = GetWord (image, kMdbOffset + 18);
    outState.allocationBytes  = GetLong (image, kMdbOffset + 20);
    outState.firstBlock       = GetWord (image, kMdbOffset + 28);
    outState.nextId           = GetLong (image, kMdbOffset + 30);
    outState.extentsBytes     = GetLong (image, kMdbOffset + 130);
    outState.catalogBytes     = GetLong (image, kMdbOffset + 146);

    for (size_t i = 0; i < 3; i++)
    {
        outState.extentsFile[i] = Extent { GetWord (image, kMdbOffset + 134 + i * 4), GetWord (image, kMdbOffset + 136 + i * 4) };
        outState.catalogFile[i] = Extent { GetWord (image, kMdbOffset + 150 + i * 4), GetWord (image, kMdbOffset + 152 + i * 4) };
    }

    CBREx (outState.allocationBytes >= kSectorBytes && outState.allocationBytes % kSectorBytes == 0, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    hr = ReadTree (outState, outState.extentsFile, outState.extentsBytes, outState.extents);
    CHR (hr);

    hr = ReadTree (outState, outState.catalogFile, outState.catalogBytes, outState.catalog);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::ReadTree
//
//  Each leaf record, from the first leaf the header node has, along the
//  forward links, as its key and its data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::ReadTree (State & state, const Extent (& file)[3], uint32_t bytes, std::vector<HfsBTree::Record> & outRecords)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  fork;
    uint32_t           next  = 0;
    uint32_t           steps = 0;
    uint64_t           total = 0;



    outRecords.clear();

    for (const Extent & extent : file)
    {
        size_t  at     = GetBlockOffset (state, extent.start);
        size_t  length = (size_t) extent.count * state.allocationBytes;
        size_t  size   = state.image->size();

        CBREx (at + length <= size, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        fork.insert (fork.end(), state.image->begin() + (ptrdiff_t) at, state.image->begin() + (ptrdiff_t) (at + length));
        total += length;
    }

    CBREx (total >= bytes && bytes >= HfsBTree::kNodeBytes, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    fork.resize (bytes);
    next = GetLong (fork, 24);

    while (next != 0)
    {
        size_t  node    = (size_t) next * HfsBTree::kNodeBytes;
        size_t  size    = fork.size();
        Word    records = 0;

        CBREx (++steps <= bytes / HfsBTree::kNodeBytes && node + HfsBTree::kNodeBytes <= size, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));
        CBREx (fork[node + 8] == HfsBTree::kNodeLeaf, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        records = GetWord (fork, node + 10);

        for (Word i = 0; i < records; i++)
        {
            size_t             start = GetWord (fork, node + HfsBTree::kNodeBytes - 2 * ((size_t) i + 1));
            size_t             end   = GetWord (fork, node + HfsBTree::kNodeBytes - 2 * ((size_t) i + 2));
            size_t             key   = 0;
            size_t             data  = 0;
            HfsBTree::Record   record;

            CBREx (start < end && end <= HfsBTree::kNodeBytes, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

            key  = (size_t) fork[node + start] + 1;
            data = start + key + (key % 2);

            if (key == 1 || data > end)
            {
                continue;
            }

            record.key.assign  (fork.begin() + (ptrdiff_t) (node + start), fork.begin() + (ptrdiff_t) (node + start + key));
            record.data.assign (fork.begin() + (ptrdiff_t) (node + data),  fork.begin() + (ptrdiff_t) (node + end));
            outRecords.push_back (std::move (record));
        }

        next = GetLong (fork, node);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::WriteTree
//
//  The tree built again in its file, which gains a run of blocks when the
//  tree has outgrown it and the master directory block has room for one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::WriteTree (State & state, HfsBTree::Kind kind, Extent (& file)[3], uint32_t & ioBytes, const std::vector<HfsBTree::Record> & records)
{
    HRESULT              hr      = S_OK;
    uint32_t             needed  = HfsBTree::CountNodes (kind, records);
    std::vector<Byte>    nodes;
    size_t               written = 0;



    if (needed > ioBytes / HfsBTree::kNodeBytes)
    {
        hr = GrowTree (state, file, ioBytes, needed);
        CHR (hr);
    }

    hr = HfsBTree::Build (kind, records, ioBytes / (uint32_t) HfsBTree::kNodeBytes, nodes);
    CHR (hr);

    for (const Extent & extent : file)
    {
        size_t  length = std::min ((size_t) extent.count * state.allocationBytes, nodes.size() - written);

        std::copy_n (nodes.begin() + (ptrdiff_t) written, length, state.image->begin() + (ptrdiff_t) GetBlockOffset (state, extent.start));
        written += length;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::GrowTree
//
//  At least `needed` nodes, and at least double the file's size, so a tree
//  that grows one file at a time is rarely moved. The last extent is
//  lengthened when the blocks after it are free. Otherwise the file is
//  moved to new runs, at most three; the tree is built again from its
//  records, so the old blocks are freed without being copied.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::GrowTree (State & state, Extent (& file)[3], uint32_t & ioBytes, uint32_t needed)
{
    HRESULT              hr      = S_OK;
    uint32_t             current = ioBytes / state.allocationBytes;
    uint32_t             minimum = (needed * (uint32_t) HfsBTree::kNodeBytes + state.allocationBytes - 1) / state.allocationBytes;
    uint32_t             target  = std::max (minimum, 2 * current);
    int                  last    = -1;
    uint32_t             free    = 0;
    std::vector<Extent>  runs;



    for (int i = 0; i < 3; i++)
    {
        last = (file[i].count != 0) ? i : last;
    }

    if (last >= 0)
    {
        uint32_t  end = (uint32_t) file[last].start + file[last].count;

        while (end + free < state.allocationBlocks && current + free < target && !IsAllocated (state, (Word) (end + free)))
        {
            free++;
        }

        free = (current + free >= minimum) ? free : 0;
    }

    if (free > 0)
    {
        runs.push_back (Extent { (Word) ((uint32_t) file[last].start + file[last].count), (Word) free });

        for (Word block = 0; block < runs[0].count; block++)
        {
            SetAllocated (state, (Word) (runs[0].start + block), true);
        }

        file[last].count = (Word) (file[last].count + free);
        ioBytes         += free * state.allocationBytes;
    }
    else
    {
        for (const Extent & extent : file)
        {
            Free (state, extent);
        }

        hr = Allocate (state, target, runs);

        if (FAILED (hr))
        {
            hr = Allocate (state, minimum, runs);
        }

        if (FAILED (hr))
        {
            for (const Extent & extent : file)
            {
                for (Word block = 0; block < extent.count; block++)
                {
                    SetAllocated (state, (Word) (extent.start + block), true);
                }
            }
        }

        CHR (hr);

        ioBytes = 0;

        for (size_t i = 0; i < 3; i++)
        {
            file[i]  = (i < runs.size()) ? runs[i] : Extent();
            ioBytes += (uint32_t) file[i].count * state.allocationBytes;
        }
    }

    for (const Extent & run : runs)
    {
        std::fill_n (state.image->begin() + (ptrdiff_t) GetBlockOffset (state, run.start), (size_t) run.count * state.allocationBytes, (Byte) 0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Save
//
//  Both trees, then the master directory block's counts and the copy of it
//  in the volume's second-last block.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Save (State & state)
{
    HRESULT              hr    = S_OK;
    std::vector<Byte> &  image = *state.image;
    uint32_t             free  = 0;
    size_t               copy  = image.size() - 2 * kSectorBytes;



    HfsBTree::Sort (HfsBTree::Kind::Catalog, state.catalog);
    HfsBTree::Sort (HfsBTree::Kind::Extents, state.extents);

    hr = WriteTree (state, HfsBTree::Kind::Extents, state.extentsFile, state.extentsBytes, state.extents);
    CHR (hr);

    hr = WriteTree (state, HfsBTree::Kind::Catalog, state.catalogFile, state.catalogBytes, state.catalog);
    CHR (hr);

    for (Word block = 0; block < state.allocationBlocks; block++)
    {
        free += IsAllocated (state, block) ? 0 : 1;
    }

    PutLong (image, kMdbOffset + 6,   GetMacNow());
    PutLong (image, kMdbOffset + 30,  state.nextId);
    PutWord (image, kMdbOffset + 34,  free);
    PutLong (image, kMdbOffset + 70,  GetLong (image, kMdbOffset + 70) + 1);
    PutLong (image, kMdbOffset + 130, state.extentsBytes);
    PutLong (image, kMdbOffset + 146, state.catalogBytes);

    for (size_t i = 0; i < 3; i++)
    {
        PutWord (image, kMdbOffset + 134 + i * 4, state.extentsFile[i].start);
        PutWord (image, kMdbOffset + 136 + i * 4, state.extentsFile[i].count);
        PutWord (image, kMdbOffset + 150 + i * 4, state.catalogFile[i].start);
        PutWord (image, kMdbOffset + 152 + i * 4, state.catalogFile[i].count);
    }

    std::copy_n (image.begin() + (ptrdiff_t) kMdbOffset, kSectorBytes, image.begin() + (ptrdiff_t) copy);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::IsAllocated
//
////////////////////////////////////////////////////////////////////////////////

bool HfsWriter::IsAllocated (const State & state, Word block)
{
    size_t  at = (size_t) state.bitmapStart * kSectorBytes + block / 8;



    return ((*state.image)[at] & (0x80 >> (block % 8))) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::SetAllocated
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::SetAllocated (State & state, Word block, bool allocated)
{
    size_t  at   = (size_t) state.bitmapStart * kSectorBytes + block / 8;
    Byte    mask = (Byte) (0x80 >> (block % 8));



    (*state.image)[at] = allocated ? (Byte) ((*state.image)[at] | mask) : (Byte) ((*state.image)[at] & ~mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Allocate
//
//  One run long enough when the volume has one, the first such; otherwise
//  the longest runs, up to three, until they are enough.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Allocate (State & state, uint32_t blocks, std::vector<Extent> & outRuns)
{
    HRESULT              hr      = S_OK;
    std::vector<Extent>  free;
    uint32_t             covered = 0;



    outRuns.clear();
    BAIL_OUT_IF (blocks == 0, S_OK);

    for (uint32_t block = 0; block <= state.allocationBlocks; block++)
    {
        bool  isFree = block < state.allocationBlocks && !IsAllocated (state, (Word) block);

        if (isFree && (free.empty() || free.back().start + free.back().count != block))
        {
            free.push_back (Extent { (Word) block, 0 });
        }

        if (isFree)
        {
            free.back().count++;
        }
    }

    for (const Extent & run : free)
    {
        if (outRuns.empty() && run.count >= blocks)
        {
            outRuns.push_back (Extent { run.start, (Word) blocks });
        }
    }

    if (outRuns.empty())
    {
        std::sort (free.begin(), free.end(), [] (const Extent & a, const Extent & b) { return a.count > b.count; });

        for (const Extent & run : free)
        {
            if (covered < blocks && outRuns.size() < 3)
            {
                Word  take = (Word) std::min<uint32_t> (run.count, blocks - covered);

                outRuns.push_back (Extent { run.start, take });
                covered += take;
            }
        }

        CBREx (covered >= blocks, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));
    }

    for (const Extent & run : outRuns)
    {
        for (Word block = 0; block < run.count; block++)
        {
            SetAllocated (state, (Word) (run.start + block), true);
        }
    }

Error:
    if (FAILED (hr))
    {
        outRuns.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Free
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::Free (State & state, const Extent & run)
{
    for (Word block = 0; block < run.count && run.start + block < state.allocationBlocks; block++)
    {
        SetAllocated (state, (Word) (run.start + block), false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::WriteFork
//
//  The bytes in newly allocated blocks, the last block's tail zeroed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::WriteFork (State & state, const std::vector<Byte> & bytes, std::vector<Extent> & outRuns)
{
    HRESULT   hr     = S_OK;
    uint32_t  blocks = (uint32_t) ((bytes.size() + state.allocationBytes - 1) / state.allocationBytes);
    size_t    done   = 0;



    hr = Allocate (state, blocks, outRuns);
    CHR (hr);

    for (const Extent & run : outRuns)
    {
        size_t  at     = GetBlockOffset (state, run.start);
        size_t  length = (size_t) run.count * state.allocationBytes;
        size_t  copy   = std::min (length, bytes.size() - done);

        std::copy_n (bytes.begin() + (ptrdiff_t) done, copy, state.image->begin() + (ptrdiff_t) at);
        std::fill_n (state.image->begin() + (ptrdiff_t) (at + copy), length - copy, (Byte) 0);
        done += copy;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::FindRecord
//
//  The catalog record under `parent` with this name, of `type`, or of
//  either a directory's or a file's for type 0. -1 when none is.
//
////////////////////////////////////////////////////////////////////////////////

int HfsWriter::FindRecord (const State & state, uint32_t parent, const std::vector<Byte> & name, Byte type)
{
    for (size_t at = 0; at < state.catalog.size(); at++)
    {
        const HfsBTree::Record &  record = state.catalog[at];
        size_t                    length = (record.key.size() > 6) ? std::min<size_t> (record.key[6], record.key.size() - 7) : 0;
        Byte                      kind   = record.data.empty() ? 0 : record.data[0];
        bool                      typed  = (type == 0) ? (kind == kRecordDirectory || kind == kRecordFile) : kind == type;

        if (typed && GetLong (record.key, 2) == parent && HfsBTree::CompareNames (record.key.data() + 7, length, name.data(), name.size()) == 0)
        {
            return (int) at;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::FindThread
//
//  A directory's or a file's thread record, keyed by the directory's or
//  file's ID with an empty name.
//
////////////////////////////////////////////////////////////////////////////////

int HfsWriter::FindThread (const State & state, uint32_t id)
{
    for (size_t at = 0; at < state.catalog.size(); at++)
    {
        const HfsBTree::Record &  record = state.catalog[at];
        Byte                      kind   = record.data.empty() ? 0 : record.data[0];

        if ((kind == kRecordDirThread || kind == kRecordFileThread) && GetLong (record.key, 2) == id)
        {
            return (int) at;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::FindDirectory
//
//  The ID of the directory the path's first `components` name, from the
//  root.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::FindDirectory (const State & state, const FilePath & path, size_t components, uint32_t & outId)
{
    HRESULT  hr = S_OK;



    outId = kRootId;

    for (size_t i = 0; i < components && i < path.GetComponents().size(); i++)
    {
        std::vector<Byte>  name;
        bool               valid = EncodeName (path.GetComponents()[i], name);
        int                found = -1;

        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

        found = FindRecord (state, outId, name, kRecordDirectory);
        CBREx (found >= 0, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

        outId = GetLong (state.catalog[(size_t) found].data, 6);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::ResolveLeaf
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::ResolveLeaf (const State & state, const FilePath & path, uint32_t & outParent, std::vector<Byte> & outName)
{
    HRESULT  hr    = S_OK;
    size_t   depth = path.GetComponents().size();
    bool     valid = false;



    CBREx (depth > 0, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    valid = EncodeName (path.GetComponents().back(), outName);
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    hr = FindDirectory (state, path, depth - 1, outParent);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::AdjustValence
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::AdjustValence (State & state, uint32_t directory, int delta)
{
    for (HfsBTree::Record & record : state.catalog)
    {
        if (!record.data.empty() && record.data[0] == kRecordDirectory && GetLong (record.data, 6) == directory)
        {
            PutWord (record.data, 4, (uint32_t) std::max (0, (int) GetWord (record.data, 4) + delta));
            PutLong (record.data, 14, GetMacNow());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::AdjustCount
//
//  A count in the master directory block: the root's files at 12 and
//  directories at 82 are words, and the volume's at 84 and 88 longs.
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::AdjustCount (State & state, size_t at, int delta)
{
    std::vector<Byte> &  image  = *state.image;
    bool                 isWord = at == 12 || at == 82;



    if (isWord)
    {
        PutWord (image, kMdbOffset + at, (uint32_t) std::max (0, (int) GetWord (image, kMdbOffset + at) + delta));
    }
    else
    {
        PutLong (image, kMdbOffset + at, (uint32_t) std::max<int64_t> (0, (int64_t) GetLong (image, kMdbOffset + at) + delta));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::FreeForkExtents
//
//  A fork's three extents in its record and any of its extents in the
//  overflow file, whose records are removed.
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::FreeForkExtents (State & state, const std::vector<Byte> & data, size_t at, uint32_t fileId, Byte fork, DeleteOutcome & outOutcome)
{
    auto  release = [&] (const Extent & run)
    {
        Free (state, run);

        for (Word block = 0; block < run.count; block++)
        {
            outOutcome.freedUnits.push_back ((uint32_t) run.start + block);
        }
    };



    for (size_t i = 0; i < 3; i++)
    {
        release (Extent { GetWord (data, at + i * 4), GetWord (data, at + i * 4 + 2) });
    }

    for (const HfsBTree::Record & record : state.extents)
    {
        if (record.key.size() >= 8 && record.key[1] == fork && GetLong (record.key, 2) == fileId)
        {
            for (size_t i = 0; i < 3 && i * 4 + 4 <= record.data.size(); i++)
            {
                release (Extent { GetWord (record.data, i * 4), GetWord (record.data, i * 4 + 2) });
            }
        }
    }

    std::erase_if (state.extents, [fileId, fork] (const HfsBTree::Record & record)
    {
        return record.key.size() >= 8 && record.key[1] == fork && GetLong (record.key, 2) == fileId;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::RemoveFile
//
//  The file's blocks freed, its record and any thread removed, and its
//  directory's and the volume's counts brought down.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::RemoveFile (State & state, size_t record, DeleteOutcome & outOutcome)
{
    std::vector<Byte>  data   = state.catalog[record].data;
    uint32_t           parent = GetLong (state.catalog[record].key, 2);
    uint32_t           fileId = GetLong (data, 20);
    int                thread = 0;



    FreeForkExtents (state, data, 74, fileId, kForkData,     outOutcome);
    FreeForkExtents (state, data, 86, fileId, kForkResource, outOutcome);

    state.catalog.erase (state.catalog.begin() + (ptrdiff_t) record);

    thread = FindThread (state, fileId);

    if (thread >= 0)
    {
        state.catalog.erase (state.catalog.begin() + thread);
    }

    AdjustValence (state, parent, -1);
    AdjustCount   (state, 84, -1);

    if (parent == kRootId)
    {
        AdjustCount (state, 12, -1);
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Write
//
//  A file of that name is replaced. Both forks are written. The Finder
//  information comes from the payload when the payload has some.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Write (std::vector<Byte> & ioImage, const FilePath & path, const FilePayload & payload)
{
    HRESULT              hr       = S_OK;
    State                state;
    uint32_t             parent   = 0;
    std::vector<Byte>    name;
    std::vector<Extent>  dataRuns;
    std::vector<Extent>  forkRuns;
    std::vector<Byte>    data (kFileBytes, 0);
    uint32_t             macType  = 0;
    uint32_t             creator  = 0;
    uint32_t             now      = GetMacNow();
    uint32_t             fileId   = 0;
    int                  existing = -1;
    DeleteOutcome        replaced;



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    existing = FindRecord (state, parent, name, kRecordDirectory);
    CBREx (existing < 0, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    existing = FindRecord (state, parent, name, kRecordFile);

    if (existing >= 0)
    {
        hr = RemoveFile (state, (size_t) existing, replaced);
        CHR (hr);
    }

    hr = WriteFork (state, payload.bytes, dataRuns);
    CHR (hr);

    hr = WriteFork (state, payload.resourceFork, forkRuns);
    CHR (hr);

    fileId = state.nextId++;
    MapFromProDos (payload.type, payload.hasAuxType ? payload.auxType : payload.loadAddress, macType, creator);

    data[0] = kRecordFile;
    data[2] = payload.IsLocked() ? kFileLocked : 0;
    PutLong (data, 4, macType);
    PutLong (data, 8, creator);
    PutLong (data, 20, fileId);
    PutWord (data, 24, dataRuns.empty() ? 0 : dataRuns[0].start);
    PutLong (data, 26, (uint32_t) payload.bytes.size());
    PutLong (data, 30, (uint32_t) ((payload.bytes.size() + state.allocationBytes - 1) / state.allocationBytes * state.allocationBytes));
    PutWord (data, 34, forkRuns.empty() ? 0 : forkRuns[0].start);
    PutLong (data, 36, (uint32_t) payload.resourceFork.size());
    PutLong (data, 40, (uint32_t) ((payload.resourceFork.size() + state.allocationBytes - 1) / state.allocationBytes * state.allocationBytes));
    PutLong (data, 44, now);
    PutLong (data, 48, now);

    //  FInfo, then FXInfo, when the payload has them.
    if (payload.finderInfo.size() >= 16)
    {
        std::copy_n (payload.finderInfo.begin(), 16, data.begin() + 4);
    }

    if (payload.finderInfo.size() >= 32)
    {
        std::copy_n (payload.finderInfo.begin() + 16, 16, data.begin() + 56);
    }

    for (size_t i = 0; i < dataRuns.size(); i++)
    {
        PutWord (data, 74 + i * 4, dataRuns[i].start);
        PutWord (data, 76 + i * 4, dataRuns[i].count);
    }

    for (size_t i = 0; i < forkRuns.size(); i++)
    {
        PutWord (data, 86 + i * 4, forkRuns[i].start);
        PutWord (data, 88 + i * 4, forkRuns[i].count);
    }

    state.catalog.push_back (HfsBTree::Record { MakeCatalogKey (parent, name), data });

    AdjustValence (state, parent, 1);
    AdjustCount   (state, 84, 1);

    if (parent == kRootId)
    {
        AdjustCount (state, 12, 1);
    }

    hr = Save (state);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Delete
//
//  A locked file fails with ERROR_ACCESS_DENIED.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Delete (std::vector<Byte> & ioImage, const FilePath & path, DeleteOutcome & outOutcome)
{
    HRESULT            hr     = S_OK;
    State              state;
    uint32_t           parent = 0;
    std::vector<Byte>  name;
    int                found  = -1;



    outOutcome = DeleteOutcome();

    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    found = FindRecord (state, parent, name, kRecordFile);
    CBREx (found >= 0, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    CBREx ((state.catalog[(size_t) found].data[2] & kFileLocked) == 0, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = RemoveFile (state, (size_t) found, outOutcome);
    CHR (hr);

    hr = Save (state);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::CreateDirectory
//
//  The directory's record and its thread, under a new ID.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::CreateDirectory (std::vector<Byte> & ioImage, const FilePath & path)
{
    HRESULT            hr       = S_OK;
    State              state;
    uint32_t           parent   = 0;
    uint32_t           id       = 0;
    uint32_t           now      = GetMacNow();
    int                existing = -1;
    std::vector<Byte>  name;
    std::vector<Byte>  directory (kDirectoryBytes, 0);
    std::vector<Byte>  thread (kThreadBytes, 0);



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    existing = FindRecord (state, parent, name, 0);
    CBREx (existing < 0, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    id = state.nextId++;

    directory[0] = kRecordDirectory;
    PutLong (directory, 6,  id);
    PutLong (directory, 10, now);
    PutLong (directory, 14, now);

    thread[0] = kRecordDirThread;
    PutLong (thread, 10, parent);
    thread[14] = (Byte) name.size();
    std::copy (name.begin(), name.end(), thread.begin() + 15);

    state.catalog.push_back (HfsBTree::Record { MakeCatalogKey (parent, name), directory });
    state.catalog.push_back (HfsBTree::Record { MakeCatalogKey (id, {}), thread });

    AdjustValence (state, parent, 1);
    AdjustCount   (state, 88, 1);

    if (parent == kRootId)
    {
        AdjustCount (state, 82, 1);
    }

    hr = Save (state);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::RemoveDirectory
//
//  The directory and everything below it. A locked file fails the whole
//  removal with ERROR_ACCESS_DENIED unless `force` is set.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::RemoveDirectory (std::vector<Byte> & ioImage, const FilePath & path, bool force, DeleteOutcome & outOutcome)
{
    HRESULT                hr     = S_OK;
    State                  state;
    uint32_t               id     = 0;
    std::vector<uint32_t>  folders;



    outOutcome = DeleteOutcome();

    hr = Load (ioImage, state);
    CHR (hr);

    hr = FindDirectory (state, path, path.GetComponents().size(), id);
    CHR (hr);

    CBREx (id != kRootId, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    folders.push_back (id);

    for (size_t at = 0; at < folders.size(); at++)
    {
        for (const HfsBTree::Record & record : state.catalog)
        {
            if (!record.data.empty() && GetLong (record.key, 2) == folders[at])
            {
                if (record.data[0] == kRecordDirectory)
                {
                    folders.push_back (GetLong (record.data, 6));
                }

                CBREx (force || record.data[0] != kRecordFile || (record.data[2] & kFileLocked) == 0, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));
            }
        }
    }

    //  The files, then each folder from the deepest up.
    for (uint32_t folder : folders)
    {
        for (int at = (int) state.catalog.size() - 1; at >= 0; at--)
        {
            const HfsBTree::Record &  record = state.catalog[(size_t) at];

            if (!record.data.empty() && record.data[0] == kRecordFile && GetLong (record.key, 2) == folder)
            {
                hr = RemoveFile (state, (size_t) at, outOutcome);
                CHR (hr);

                at = std::min (at, (int) state.catalog.size());
            }
        }
    }

    for (auto folder = folders.rbegin(); folder != folders.rend(); folder++)
    {
        int  thread = FindThread (state, *folder);

        for (size_t at = 0; at < state.catalog.size(); at++)
        {
            const HfsBTree::Record &  record = state.catalog[at];

            if (!record.data.empty() && record.data[0] == kRecordDirectory && GetLong (record.data, 6) == *folder)
            {
                uint32_t  parent = GetLong (record.key, 2);

                state.catalog.erase (state.catalog.begin() + (ptrdiff_t) at);
                AdjustValence (state, parent, -1);
                AdjustCount   (state, 88, -1);

                if (parent == kRootId)
                {
                    AdjustCount (state, 82, -1);
                }

                break;
            }
        }

        thread = FindThread (state, *folder);

        if (thread >= 0)
        {
            state.catalog.erase (state.catalog.begin() + thread);
        }
    }

    hr = Save (state);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Rename
//
//  The record's key takes the new name, and a directory's or file's thread
//  record its name too. A name another entry of the directory has fails
//  with ERROR_ALREADY_EXISTS.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Rename (std::vector<Byte> & ioImage, const FilePath & from, const std::string & to)
{
    HRESULT            hr      = S_OK;
    State              state;
    uint32_t           parent  = 0;
    uint32_t           id      = 0;
    std::vector<Byte>  name;
    std::vector<Byte>  renamed;
    int                found   = -1;
    int                thread  = -1;
    int                clash   = -1;
    bool               valid   = false;



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, from, parent, name);
    CHR (hr);

    valid = EncodeName (to, renamed);
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    found = FindRecord (state, parent, name, 0);
    CBREx (found >= 0, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    clash = FindRecord (state, parent, renamed, 0);
    CBREx (clash < 0 || clash == found, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    state.catalog[(size_t) found].key = MakeCatalogKey (parent, renamed);

    id     = GetLong (state.catalog[(size_t) found].data, state.catalog[(size_t) found].data[0] == kRecordDirectory ? 6 : 20);
    thread = FindThread (state, id);

    if (thread >= 0)
    {
        std::vector<Byte> &  data = state.catalog[(size_t) thread].data;

        std::fill (data.begin() + 14, data.end(), (Byte) 0);
        data[14] = (Byte) renamed.size();
        std::copy (renamed.begin(), renamed.end(), data.begin() + 15);
    }

    hr = Save (state);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::ToMacDate
//
//  ProDOS date and time words as seconds since 1904. A two-digit year
//  under 40 is 2000 or later, as in ProDOS 8 Technical Note #28.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsWriter::ToMacDate (Word date, Word time)
{
    tm   when = {};
    int  year = (date >> 9) & 0x7F;



    when.tm_year = year + ((year < 40) ? 100 : 0);
    when.tm_mon  = ((date >> 5) & 0x0F) - 1;
    when.tm_mday = date & 0x1F;
    when.tm_hour = (time >> 8) & 0x1F;
    when.tm_min  = time & 0x3F;

    return (uint32_t) (_mkgmtime (&when) + kMacToUnixSeconds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::SetAttributes
//
//  A ProDOS type or aux type is mapped to a Finder type and creator as GS/OS
//  maps it. When only one of the two is set, the other is the file's current
//  value. Access locks or unlocks the file. Dates are ProDOS date and time
//  words.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::SetAttributes (std::vector<Byte> & ioImage, const FilePath & path, const FileAttributes & attributes)
{
    HRESULT            hr      = S_OK;
    State              state;
    uint32_t           parent  = 0;
    std::vector<Byte>  name;
    int                found   = -1;
    Byte               type    = 0;
    Word               auxType = 0;
    uint32_t           macType = 0;
    uint32_t           creator = 0;



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    found = FindRecord (state, parent, name, kRecordFile);
    CBREx (found >= 0, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    {
        std::vector<Byte> &  data = state.catalog[(size_t) found].data;

        HfsVolume::MapToProDos (GetLong (data, 4), GetLong (data, 8), type, auxType);

        if (attributes.hasType || attributes.hasAuxType)
        {
            MapFromProDos (attributes.hasType ? attributes.type : type, attributes.hasAuxType ? attributes.auxType : auxType, macType, creator);
            PutLong (data, 4, macType);
            PutLong (data, 8, creator);
        }

        if (attributes.hasAccess)
        {
            data[2] = (attributes.access & FilePayload::kAccessWriteEnable) != 0 ? (Byte) (data[2] & ~kFileLocked) : (Byte) (data[2] | kFileLocked);
        }

        if (attributes.hasCreated)
        {
            PutLong (data, 44, ToMacDate (attributes.createdDate, attributes.createdTime));
        }

        if (attributes.hasModified)
        {
            PutLong (data, 48, ToMacDate (attributes.modifiedDate, attributes.modifiedTime));
        }
    }

    hr = Save (state);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::Format
//
//  Boot blocks 0 and 1, the master directory block in block 2, the volume
//  bitmap from block 3, then the allocation blocks. An allocation block is
//  the smallest power of two of 512-byte blocks that brings the count under
//  65,536. The extents and catalog files are
//  the first allocation blocks, each about a 128th of the volume, and the
//  root directory and its thread the catalog's only records. The copy of the
//  master directory block is in the second-last block.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Format (std::vector<Byte> & outImage, size_t sectors, const std::string & name)
{
    HRESULT            hr           = S_OK;
    std::vector<Byte>  encoded;
    bool               valid        = false;
    uint32_t           perBlock     = 1;
    uint32_t           bitmap       = 0;
    uint32_t           blocks       = 0;
    uint32_t           treeBlocks   = 0;
    uint32_t           now          = GetMacNow();
    State              state;
    std::vector<Byte>  root (kDirectoryBytes, 0);
    std::vector<Byte>  thread (kThreadBytes, 0);



    outImage.clear();

    CBREx (sectors >= 800 && sectors <= 4194304, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));
    valid = EncodeName (name, encoded) && encoded.size() <= 27;
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    while ((sectors - 5) / perBlock > 65535)
    {
        perBlock *= 2;
    }

    bitmap     = (uint32_t) (((sectors - 5) / perBlock + 4095) / 4096);
    blocks     = (uint32_t) ((sectors - 3 - bitmap - 2) / perBlock);
    treeBlocks = std::max<uint32_t> (4, (uint32_t) (sectors / 128 / perBlock));

    outImage.assign (sectors * kSectorBytes, 0);

    PutWord (outImage, kMdbOffset + 0,   kSignature);
    PutLong (outImage, kMdbOffset + 2,   now);
    PutLong (outImage, kMdbOffset + 6,   now);
    PutWord (outImage, kMdbOffset + 10,  0x0100);
    PutWord (outImage, kMdbOffset + 14,  3);
    PutWord (outImage, kMdbOffset + 18,  blocks);
    PutLong (outImage, kMdbOffset + 20,  perBlock * (uint32_t) kSectorBytes);
    PutLong (outImage, kMdbOffset + 24,  perBlock * (uint32_t) kSectorBytes * 4);
    PutWord (outImage, kMdbOffset + 28,  3 + bitmap);
    PutLong (outImage, kMdbOffset + 30,  kFirstUserId);
    outImage[kMdbOffset + 36] = (Byte) encoded.size();
    std::copy (encoded.begin(), encoded.end(), outImage.begin() + (ptrdiff_t) (kMdbOffset + 37));
    PutLong (outImage, kMdbOffset + 74,  treeBlocks * perBlock * (uint32_t) kSectorBytes);
    PutLong (outImage, kMdbOffset + 78,  treeBlocks * perBlock * (uint32_t) kSectorBytes);
    PutLong (outImage, kMdbOffset + 130, treeBlocks * perBlock * (uint32_t) kSectorBytes);
    PutWord (outImage, kMdbOffset + 134, 0);
    PutWord (outImage, kMdbOffset + 136, treeBlocks);
    PutLong (outImage, kMdbOffset + 146, treeBlocks * perBlock * (uint32_t) kSectorBytes);
    PutWord (outImage, kMdbOffset + 150, treeBlocks);
    PutWord (outImage, kMdbOffset + 152, treeBlocks);

    state.image            = &outImage;
    state.bitmapStart      = 3;
    state.allocationBlocks = (Word) blocks;
    state.allocationBytes  = perBlock * (uint32_t) kSectorBytes;
    state.firstBlock       = (Word) (3 + bitmap);
    state.nextId           = kFirstUserId;
    state.extentsBytes     = treeBlocks * state.allocationBytes;
    state.catalogBytes     = treeBlocks * state.allocationBytes;
    state.extentsFile[0]   = Extent { 0, (Word) treeBlocks };
    state.catalogFile[0]   = Extent { (Word) treeBlocks, (Word) treeBlocks };

    for (Word block = 0; block < 2 * treeBlocks; block++)
    {
        SetAllocated (state, block, true);
    }

    root[0] = kRecordDirectory;
    PutLong (root, 6,  kRootId);
    PutLong (root, 10, now);
    PutLong (root, 14, now);

    thread[0] = kRecordDirThread;
    PutLong (thread, 10, kRootParentId);
    thread[14] = (Byte) encoded.size();
    std::copy (encoded.begin(), encoded.end(), thread.begin() + 15);

    state.catalog.push_back (HfsBTree::Record { MakeCatalogKey (kRootParentId, encoded), root });
    state.catalog.push_back (HfsBTree::Record { MakeCatalogKey (kRootId, {}), thread });

    hr = Save (state);
    CHR (hr);

    PutLong (outImage, kMdbOffset + 70, 0);
    std::copy_n (outImage.begin() + (ptrdiff_t) kMdbOffset, kSectorBytes, outImage.end() - (ptrdiff_t) (2 * kSectorBytes));

Error:
    if (FAILED (hr))
    {
        outImage.clear();
    }

    return hr;
}
