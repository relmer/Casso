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
//  HfsWriter::IsValidVolumeName
//
////////////////////////////////////////////////////////////////////////////////

bool HfsWriter::IsValidVolumeName (const std::string & name)
{
    std::vector<Byte>  encoded;



    return EncodeName (name, encoded) && encoded.size() <= kMaxVolumeNameLength;
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

    hr = state.catalog.editor->ReadAll (outRecords);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::CheckTrees
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::CheckTrees (std::vector<Byte> & image, std::string & outProblems)
{
    HRESULT      hr = S_OK;
    State        state;
    std::string  extents;



    outProblems.clear();

    hr = Load (image, state);
    CHR (hr);

    state.catalog.editor->Check (outProblems);
    state.extents.editor->Check (extents);

    outProblems += extents.empty() ? std::string() : "extents: " + extents;

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
//  The master directory block's fields this class changes, and both tree
//  files. A tree file with extents in the overflow file isn't written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::Load (std::vector<Byte> & image, State & outState)
{
    HRESULT  hr       = S_OK;
    bool     sized    = image.size() >= kMdbOffset + kSectorBytes;
    bool     isSigned = false;



    outState.image = &image;

    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    isSigned = GetWord (image, kMdbOffset) == kSignature;
    CBREx (isSigned, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    outState.bitmapStart      = GetWord (image, kMdbOffset + 14);
    outState.allocationBlocks = GetWord (image, kMdbOffset + 18);
    outState.allocationBytes  = GetLong (image, kMdbOffset + 20);
    outState.firstBlock       = GetWord (image, kMdbOffset + 28);
    outState.nextId           = GetLong (image, kMdbOffset + 30);
    outState.extents.bytes    = GetLong (image, kMdbOffset + 130);
    outState.catalog.bytes    = GetLong (image, kMdbOffset + 146);

    for (size_t i = 0; i < 3; i++)
    {
        outState.extents.file[i] = Extent { GetWord (image, kMdbOffset + 134 + i * 4), GetWord (image, kMdbOffset + 136 + i * 4) };
        outState.catalog.file[i] = Extent { GetWord (image, kMdbOffset + 150 + i * 4), GetWord (image, kMdbOffset + 152 + i * 4) };
    }

    CBREx (outState.allocationBytes >= kSectorBytes && outState.allocationBytes % kSectorBytes == 0, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

    hr = LoadTree (outState, HfsBTree::Kind::Extents, outState.extents);
    CHR (hr);

    hr = LoadTree (outState, HfsBTree::Kind::Catalog, outState.catalog);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::LoadTree
//
//  The tree file's nodes, from its three extents.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::LoadTree (State & state, HfsBTree::Kind kind, Tree & ioTree)
{
    HRESULT   hr    = S_OK;
    uint64_t  total = 0;
    bool      whole = false;



    ioTree.nodes.clear();

    for (const Extent & extent : ioTree.file)
    {
        size_t  at     = GetBlockOffset (state, extent.start);
        size_t  length = (size_t) extent.count * state.allocationBytes;
        size_t  size   = state.image->size();

        CBREx (at + length <= size, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        ioTree.nodes.insert (ioTree.nodes.end(), state.image->begin() + (ptrdiff_t) at, state.image->begin() + (ptrdiff_t) (at + length));
        total += length;
    }

    whole = total >= ioTree.bytes && ioTree.bytes >= HfsBTree::kNodeBytes;
    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    ioTree.nodes.resize (ioTree.bytes);
    ioTree.editor = std::make_unique<HfsBTreeEditor> (kind, ioTree.nodes);

    hr = ioTree.editor->Open();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::WriteTree
//
//  The nodes the editor changed, or every node of a tree that moved, into
//  the tree file's extents.
//
////////////////////////////////////////////////////////////////////////////////

void HfsWriter::WriteTree (State & state, const Tree & tree)
{
    std::vector<uint32_t>  nodes;
    uint32_t               total = (uint32_t) (tree.nodes.size() / HfsBTree::kNodeBytes);



    if (tree.isMoved)
    {
        for (uint32_t node = 0; node < total; node++)
        {
            nodes.push_back (node);
        }
    }
    else
    {
        nodes.assign (tree.editor->GetChangedNodes().begin(), tree.editor->GetChangedNodes().end());
    }

    for (uint32_t node : nodes)
    {
        size_t  offset = (size_t) node * HfsBTree::kNodeBytes;

        for (const Extent & extent : tree.file)
        {
            size_t  length = (size_t) extent.count * state.allocationBytes;

            if (offset < length)
            {
                std::copy_n (tree.nodes.begin() + (ptrdiff_t) ((size_t) node * HfsBTree::kNodeBytes), HfsBTree::kNodeBytes,
                             state.image->begin() + (ptrdiff_t) (GetBlockOffset (state, extent.start) + offset));
                break;
            }

            offset -= length;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::GrowTree
//
//  At least `needed` nodes, and at least double the file's size, so a tree
//  that grows one record at a time is rarely moved. The last extent is
//  lengthened when the blocks after it are free. Otherwise the file is
//  moved to new runs, at most three, and every node is written there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::GrowTree (State & state, Tree & ioTree, uint32_t needed)
{
    HRESULT              hr      = S_OK;
    uint32_t             current = ioTree.bytes / state.allocationBytes;
    uint32_t             minimum = (needed * (uint32_t) HfsBTree::kNodeBytes + state.allocationBytes - 1) / state.allocationBytes;
    uint32_t             target  = std::max (minimum, 2 * current);
    int                  last    = -1;
    uint32_t             free    = 0;
    std::vector<Extent>  runs;



    for (int i = 0; i < 3; i++)
    {
        last = (ioTree.file[i].count != 0) ? i : last;
    }

    if (last >= 0)
    {
        uint32_t  end = (uint32_t) ioTree.file[last].start + ioTree.file[last].count;

        while (end + free < state.allocationBlocks && current + free < target && !IsAllocated (state, (Word) (end + free)))
        {
            free++;
        }

        free = (current + free >= minimum) ? free : 0;
    }

    if (free > 0)
    {
        runs.push_back (Extent { (Word) ((uint32_t) ioTree.file[last].start + ioTree.file[last].count), (Word) free });

        for (Word block = 0; block < runs[0].count; block++)
        {
            SetAllocated (state, (Word) (runs[0].start + block), true);
        }

        ioTree.file[last].count = (Word) (ioTree.file[last].count + free);
        ioTree.bytes           += free * state.allocationBytes;
    }
    else
    {
        for (const Extent & extent : ioTree.file)
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
            for (const Extent & extent : ioTree.file)
            {
                for (Word block = 0; block < extent.count; block++)
                {
                    SetAllocated (state, (Word) (extent.start + block), true);
                }
            }
        }

        CHR (hr);

        ioTree.bytes   = 0;
        ioTree.isMoved = true;

        for (size_t i = 0; i < 3; i++)
        {
            ioTree.file[i]  = (i < runs.size()) ? runs[i] : Extent();
            ioTree.bytes   += (uint32_t) ioTree.file[i].count * state.allocationBytes;
        }
    }

    for (const Extent & run : runs)
    {
        std::fill_n (state.image->begin() + (ptrdiff_t) GetBlockOffset (state, run.start), (size_t) run.count * state.allocationBytes, (Byte) 0);
    }

    hr = ioTree.editor->Extend (ioTree.bytes / (uint32_t) HfsBTree::kNodeBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::EnsureNodes
//
//  At least `count` free nodes in the tree, its file grown for them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::EnsureNodes (State & state, Tree & ioTree, uint32_t count)
{
    HRESULT  hr    = S_OK;
    int      tries = 0;



    while (ioTree.editor->GetFreeNodes() < count)
    {
        uint32_t  needed = ioTree.editor->GetTotalNodes() + (count - ioTree.editor->GetFreeNodes());

        CBREx (++tries <= 4, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

        hr = GrowTree (state, ioTree, needed);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::InsertRecord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::InsertRecord (State & state, Tree & ioTree, const HfsBTree::Record & record)
{
    HRESULT  hr = S_OK;



    hr = EnsureNodes (state, ioTree, ioTree.editor->GetNodesForInsert());
    CHR (hr);

    hr = ioTree.editor->Insert (record);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::ReplaceRecord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::ReplaceRecord (State & state, Tree & ioTree, const HfsBTree::Record & record)
{
    HRESULT  hr = S_OK;



    hr = EnsureNodes (state, ioTree, ioTree.editor->GetNodesForInsert());
    CHR (hr);

    hr = ioTree.editor->Replace (record);
    CHR (hr);

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
    std::vector<Byte> &  image = *state.image;
    uint32_t             free  = 0;
    size_t               copy  = image.size() - 2 * kSectorBytes;



    WriteTree (state, state.extents);
    WriteTree (state, state.catalog);

    for (Word block = 0; block < state.allocationBlocks; block++)
    {
        free += IsAllocated (state, block) ? 0 : 1;
    }

    PutLong (image, kMdbOffset + 6,   GetMacNow());
    PutLong (image, kMdbOffset + 30,  state.nextId);
    PutWord (image, kMdbOffset + 34,  free);
    PutLong (image, kMdbOffset + 70,  GetLong (image, kMdbOffset + 70) + 1);
    PutLong (image, kMdbOffset + 130, state.extents.bytes);
    PutLong (image, kMdbOffset + 146, state.catalog.bytes);

    for (size_t i = 0; i < 3; i++)
    {
        PutWord (image, kMdbOffset + 134 + i * 4, state.extents.file[i].start);
        PutWord (image, kMdbOffset + 136 + i * 4, state.extents.file[i].count);
        PutWord (image, kMdbOffset + 150 + i * 4, state.catalog.file[i].start);
        PutWord (image, kMdbOffset + 152 + i * 4, state.catalog.file[i].count);
    }

    std::copy_n (image.begin() + (ptrdiff_t) kMdbOffset, kSectorBytes, image.begin() + (ptrdiff_t) copy);

    return S_OK;
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
//  either a directory's or a file's for type 0.
//
////////////////////////////////////////////////////////////////////////////////

bool HfsWriter::FindRecord (const State & state, uint32_t parent, const std::vector<Byte> & name, Byte type, HfsBTree::Record & outRecord)
{
    HRESULT  hr    = state.catalog.editor->Find (MakeCatalogKey (parent, name), outRecord);
    Byte     kind  = (SUCCEEDED (hr) && !outRecord.data.empty()) ? outRecord.data[0] : (Byte) 0;



    return (type == 0) ? (kind == kRecordDirectory || kind == kRecordFile) : kind == type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::FindThread
//
//  A directory's or a file's thread record, keyed by the directory's or
//  file's ID with an empty name.
//
////////////////////////////////////////////////////////////////////////////////

bool HfsWriter::FindThread (const State & state, uint32_t id, HfsBTree::Record & outRecord)
{
    HRESULT  hr   = state.catalog.editor->Find (MakeCatalogKey (id, {}), outRecord);
    Byte     kind = (SUCCEEDED (hr) && !outRecord.data.empty()) ? outRecord.data[0] : (Byte) 0;



    return kind == kRecordDirThread || kind == kRecordFileThread;
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
        HfsBTree::Record   record;
        bool               valid = EncodeName (path.GetComponents()[i], name);
        bool               found = false;

        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

        found = FindRecord (state, outId, name, kRecordDirectory, record);
        CBREx (found, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

        outId = GetLong (record.data, 6);
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
//  The directory's record, found through its thread, with its count of
//  entries changed and its modification date now.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::AdjustValence (State & state, uint32_t directory, int delta)
{
    HRESULT            hr     = S_OK;
    HfsBTree::Record   thread;
    HfsBTree::Record   record;
    std::vector<Byte>  name;
    bool               found  = FindThread (state, directory, thread);



    BAIL_OUT_IF (!found || thread.data.size() < kThreadBytes, S_OK);

    name.assign (thread.data.begin() + 15, thread.data.begin() + 15 + std::min<size_t> (thread.data[14], kMaxNameLength));

    found = FindRecord (state, GetLong (thread.data, 10), name, kRecordDirectory, record);
    BAIL_OUT_IF (!found, S_OK);

    PutWord (record.data, 4, (uint32_t) std::max (0, (int) GetWord (record.data, 4) + delta));
    PutLong (record.data, 14, GetMacNow());

    hr = ReplaceRecord (state, state.catalog, record);
    CHR (hr);

Error:
    return hr;
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

HRESULT HfsWriter::FreeForkExtents (State & state, const std::vector<Byte> & data, size_t at, uint32_t fileId, Byte fork, DeleteOutcome & outOutcome)
{
    HRESULT                         hr    = S_OK;
    std::vector<Byte>               from  = { 7, fork, 0, 0, 0, 0, 0, 0 };
    std::vector<HfsBTree::Record>   overflow;
    auto                            release = [&] (const Extent & run)
    {
        Free (state, run);

        for (Word block = 0; block < run.count; block++)
        {
            outOutcome.freedUnits.push_back ((uint32_t) run.start + block);
        }
    };



    PutLong (from, 2, fileId);

    for (size_t i = 0; i < 3; i++)
    {
        release (Extent { GetWord (data, at + i * 4), GetWord (data, at + i * 4 + 2) });
    }

    hr = state.extents.editor->Scan (from, [&] (const HfsBTree::Record & record)
    {
        bool  isFork = record.key.size() >= 8 && record.key[1] == fork && GetLong (record.key, 2) == fileId;

        if (isFork)
        {
            overflow.push_back (record);
        }

        return isFork;
    });
    CHR (hr);

    for (const HfsBTree::Record & record : overflow)
    {
        for (size_t i = 0; i < 3 && i * 4 + 4 <= record.data.size(); i++)
        {
            release (Extent { GetWord (record.data, i * 4), GetWord (record.data, i * 4 + 2) });
        }

        hr = state.extents.editor->Remove (record.key);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter::RemoveFile
//
//  The file's blocks freed, its record and any thread removed, and its
//  directory's and the volume's counts brought down.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsWriter::RemoveFile (State & state, const HfsBTree::Record & record, DeleteOutcome & outOutcome)
{
    HRESULT           hr     = S_OK;
    uint32_t          parent = GetLong (record.key, 2);
    uint32_t          fileId = GetLong (record.data, 20);
    HfsBTree::Record  thread;
    bool              found  = false;



    hr = FreeForkExtents (state, record.data, 74, fileId, kForkData, outOutcome);
    CHR (hr);

    hr = FreeForkExtents (state, record.data, 86, fileId, kForkResource, outOutcome);
    CHR (hr);

    hr = state.catalog.editor->Remove (record.key);
    CHR (hr);

    found = FindThread (state, fileId, thread);

    if (found)
    {
        hr = state.catalog.editor->Remove (thread.key);
        CHR (hr);
    }

    hr = AdjustValence (state, parent, -1);
    CHR (hr);

    AdjustCount (state, 84, -1);

    if (parent == kRootId)
    {
        AdjustCount (state, 12, -1);
    }

Error:
    return hr;
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
    bool                 existing = false;
    HfsBTree::Record     record;
    DeleteOutcome        replaced;



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    existing = FindRecord (state, parent, name, kRecordDirectory, record);
    CBREx (!existing, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    existing = FindRecord (state, parent, name, kRecordFile, record);

    if (existing)
    {
        hr = RemoveFile (state, record, replaced);
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

    hr = InsertRecord (state, state.catalog, HfsBTree::Record { MakeCatalogKey (parent, name), data });
    CHR (hr);

    hr = AdjustValence (state, parent, 1);
    CHR (hr);

    AdjustCount (state, 84, 1);

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
    HfsBTree::Record   record;
    bool               found  = false;



    outOutcome = DeleteOutcome();

    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    found = FindRecord (state, parent, name, kRecordFile, record);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    CBREx ((record.data[2] & kFileLocked) == 0, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = RemoveFile (state, record, outOutcome);
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
    bool               existing = false;
    HfsBTree::Record   record;
    std::vector<Byte>  name;
    std::vector<Byte>  directory (kDirectoryBytes, 0);
    std::vector<Byte>  thread (kThreadBytes, 0);



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    existing = FindRecord (state, parent, name, 0, record);
    CBREx (!existing, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    id = state.nextId++;

    directory[0] = kRecordDirectory;
    PutLong (directory, 6,  id);
    PutLong (directory, 10, now);
    PutLong (directory, 14, now);

    thread[0] = kRecordDirThread;
    PutLong (thread, 10, parent);
    thread[14] = (Byte) name.size();
    std::copy (name.begin(), name.end(), thread.begin() + 15);

    hr = InsertRecord (state, state.catalog, HfsBTree::Record { MakeCatalogKey (parent, name), directory });
    CHR (hr);

    hr = InsertRecord (state, state.catalog, HfsBTree::Record { MakeCatalogKey (id, {}), thread });
    CHR (hr);

    hr = AdjustValence (state, parent, 1);
    CHR (hr);

    AdjustCount (state, 88, 1);

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
    HRESULT                        hr       = S_OK;
    State                          state;
    uint32_t                       id       = 0;
    bool                           isLocked = false;
    std::vector<uint32_t>          folders;
    std::vector<HfsBTree::Record>  files;



    outOutcome = DeleteOutcome();

    hr = Load (ioImage, state);
    CHR (hr);

    hr = FindDirectory (state, path, path.GetComponents().size(), id);
    CHR (hr);

    CBREx (id != kRootId, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    folders.push_back (id);

    for (size_t at = 0; at < folders.size(); at++)
    {
        uint32_t  folder = folders[at];

        hr = state.catalog.editor->Scan (MakeCatalogKey (folder, {}), [&] (const HfsBTree::Record & record)
        {
            bool  isChild = GetLong (record.key, 2) == folder && !record.data.empty();

            if (isChild && record.data[0] == kRecordDirectory)
            {
                folders.push_back (GetLong (record.data, 6));
            }
            else if (isChild && record.data[0] == kRecordFile)
            {
                files.push_back (record);
                isLocked = isLocked || (record.data[2] & kFileLocked) != 0;
            }

            return isChild;
        });
        CHR (hr);
    }

    CBREx (force || !isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    for (const HfsBTree::Record & file : files)
    {
        hr = RemoveFile (state, file, outOutcome);
        CHR (hr);
    }

    //  Each folder from the deepest up: its record, found through its
    //  thread, then the thread.
    for (auto folder = folders.rbegin(); folder != folders.rend(); folder++)
    {
        HfsBTree::Record   thread;
        uint32_t           parent = 0;
        std::vector<Byte>  name;
        bool               found  = FindThread (state, *folder, thread);

        CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        parent = GetLong (thread.data, 10);
        name.assign (thread.data.begin() + 15, thread.data.begin() + 15 + std::min<size_t> (thread.data[14], kMaxNameLength));

        hr = state.catalog.editor->Remove (MakeCatalogKey (parent, name));
        CHR (hr);

        hr = state.catalog.editor->Remove (thread.key);
        CHR (hr);

        hr = AdjustValence (state, parent, -1);
        CHR (hr);

        AdjustCount (state, 88, -1);

        if (parent == kRootId)
        {
            AdjustCount (state, 82, -1);
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
//  The record is removed and inserted again under the new name, and a
//  directory's or file's thread record takes the name too. A name another
//  entry of the directory has fails with ERROR_ALREADY_EXISTS.
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
    HfsBTree::Record   record;
    HfsBTree::Record   other;
    HfsBTree::Record   thread;
    bool               found   = false;
    bool               clash   = false;
    bool               valid   = false;



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, from, parent, name);
    CHR (hr);

    valid = EncodeName (to, renamed);
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    found = FindRecord (state, parent, name, 0, record);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    clash = FindRecord (state, parent, renamed, 0, other) && HfsBTree::CompareCatalogKeys (other.key, record.key) != 0;
    CBREx (!clash, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    hr = state.catalog.editor->Remove (record.key);
    CHR (hr);

    hr = InsertRecord (state, state.catalog, HfsBTree::Record { MakeCatalogKey (parent, renamed), record.data });
    CHR (hr);

    id    = GetLong (record.data, record.data[0] == kRecordDirectory ? 6 : 20);
    found = FindThread (state, id, thread);

    if (found)
    {
        std::fill (thread.data.begin() + 14, thread.data.end(), (Byte) 0);
        thread.data[14] = (Byte) renamed.size();
        std::copy (renamed.begin(), renamed.end(), thread.data.begin() + 15);

        hr = ReplaceRecord (state, state.catalog, thread);
        CHR (hr);
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
    HfsBTree::Record   record;
    bool               found   = false;
    Byte               type    = 0;
    Word               auxType = 0;
    uint32_t           macType = 0;
    uint32_t           creator = 0;



    hr = Load (ioImage, state);
    CHR (hr);

    hr = ResolveLeaf (state, path, parent, name);
    CHR (hr);

    found = FindRecord (state, parent, name, kRecordFile, record);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    HfsVolume::MapToProDos (GetLong (record.data, 4), GetLong (record.data, 8), type, auxType);

    if (attributes.hasType || attributes.hasAuxType)
    {
        MapFromProDos (attributes.hasType ? attributes.type : type, attributes.hasAuxType ? attributes.auxType : auxType, macType, creator);
        PutLong (record.data, 4, macType);
        PutLong (record.data, 8, creator);
    }

    if (attributes.hasAccess)
    {
        record.data[2] = (attributes.access & FilePayload::kAccessWriteEnable) != 0 ? (Byte) (record.data[2] & ~kFileLocked) : (Byte) (record.data[2] | kFileLocked);
    }

    if (attributes.hasCreated)
    {
        PutLong (record.data, 44, ToMacDate (attributes.createdDate, attributes.createdTime));
    }

    if (attributes.hasModified)
    {
        PutLong (record.data, 48, ToMacDate (attributes.modifiedDate, attributes.modifiedTime));
    }

    hr = ReplaceRecord (state, state.catalog, record);
    CHR (hr);

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
    std::vector<HfsBTree::Record>  records;



    outImage.clear();

    CBREx (sectors >= 800 && sectors <= 4194304, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));
    valid = EncodeName (name, encoded) && encoded.size() <= kMaxVolumeNameLength;
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
    state.extents.bytes    = treeBlocks * state.allocationBytes;
    state.catalog.bytes    = treeBlocks * state.allocationBytes;
    state.extents.file[0]  = Extent { 0, (Word) treeBlocks };
    state.catalog.file[0]  = Extent { (Word) treeBlocks, (Word) treeBlocks };

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

    records.push_back (HfsBTree::Record { MakeCatalogKey (kRootParentId, encoded), root });
    records.push_back (HfsBTree::Record { MakeCatalogKey (kRootId, {}), thread });

    hr = HfsBTree::Build (HfsBTree::Kind::Extents, {}, state.extents.bytes / (uint32_t) HfsBTree::kNodeBytes, state.extents.nodes);
    CHR (hr);

    hr = HfsBTree::Build (HfsBTree::Kind::Catalog, records, state.catalog.bytes / (uint32_t) HfsBTree::kNodeBytes, state.catalog.nodes);
    CHR (hr);

    state.extents.editor  = std::make_unique<HfsBTreeEditor> (HfsBTree::Kind::Extents, state.extents.nodes);
    state.catalog.editor  = std::make_unique<HfsBTreeEditor> (HfsBTree::Kind::Catalog, state.catalog.nodes);
    state.extents.isMoved = true;
    state.catalog.isMoved = true;

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
