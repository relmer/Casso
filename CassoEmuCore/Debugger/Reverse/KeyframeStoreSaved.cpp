#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Export
//
//  The active path, with the input journal's entries from the first on,
//  then each branch left behind with its entries. Every keyframe's stored
//  bytes are copied end to end, so the file has none of the arena's free
//  space.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Export (
    const InputJournal  & journal,
    SavedKeyframes      & outSaved)
{
    HRESULT               hr       = S_OK;
    bool                  isCopied = false;
    size_t                i        = 0;
    SavedKeyframeSegment  segment;



    outSaved = SavedKeyframes();

    hr = WaitForPending();
    CHR (hr);

    CBRA (m_count > 0);

    outSaved.settings     = m_settings;
    outSaved.nextBranchId = m_nextBranchId;
    outSaved.arena.reserve (m_storedBytes + m_branchBytes);

    segment.firstJournalIndex = journal.GetBeginIndex();

    isCopied = journal.TryCopyRecords (segment.firstJournalIndex, UINT64_MAX, segment.records);
    CBRA (isCopied);

    for (i = 0; i < m_count; i++)
    {
        ExportEntry (GetEntry (i), segment, outSaved.arena);
    }

    outSaved.segments.push_back (std::move (segment));

    for (const Branch & branch : m_branches)
    {
        segment                   = SavedKeyframeSegment();
        segment.info              = branch.info;
        segment.firstJournalIndex = branch.firstJournalIndex;
        segment.records           = branch.records;

        for (const Entry & entry : branch.entries)
        {
            ExportEntry (entry, segment, outSaved.arena);
        }

        outSaved.segments.push_back (std::move (segment));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExportEntry
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ExportEntry (
    const Entry           & entry,
    SavedKeyframeSegment  & segment,
    std::vector<Byte>     & arena) const
{
    const Byte  * start = m_arena.get() + entry.offset;



    segment.infos.push_back   (entry.info);
    segment.offsets.push_back (arena.size());

    arena.insert (arena.end(), start, start + entry.info.storedBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Import
//
//  The store is emptied, then laid out as saved: the saved arena becomes
//  the arena, the active path the table and the rest the branches, and the
//  journal is loaded with the active path's entries. Every saved setting is
//  used but the budget. The store is then moved into a table and an arena
//  sized from the budget, and the oldest groups are dropped until the
//  history fits it. When the saved history is not consistent, or its newest
//  whole snapshot does not unpack, the store and the journal are emptied.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Import (
    const SavedKeyframes  & saved,
    InputJournal          & journal)
{
    HRESULT   hr         = S_OK;
    size_t    budget     = m_settings.budgetBytes;
    size_t    slotCount  = 0;
    size_t    arenaBytes = 0;
    size_t    offset     = 0;
    size_t    i          = 0;
    size_t    s          = 0;
    bool      isValid    = IsValidSaved (saved);
    bool      isPlaced   = false;
    Byte    * arena      = nullptr;
    Entry     entry;
    Branch    branch;



    hr = WaitForPending();
    CHR (hr);

    Clear();

    CBREx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_settings             = saved.settings;
    m_settings.budgetBytes = budget;

    arena = new (std::nothrow) Byte[saved.arena.size()];
    CPRA (arena);

    m_arena.reset (arena);
    memcpy (m_arena.get(), saved.arena.data(), saved.arena.size());

    m_arenaBytes = saved.arena.size();

    m_entries.assign (saved.segments[0].infos.size(), Entry());
    m_first = 0;

    for (i = 0; i < saved.segments[0].infos.size(); i++)
    {
        entry.info   = saved.segments[0].infos[i];
        entry.offset = saved.segments[0].offsets[i];

        m_entries[i]   = entry;
        m_storedBytes += entry.info.storedBytes;
        m_sideBytes   += entry.info.sideBytes;
    }

    m_count = saved.segments[0].infos.size();

    for (s = 1; s < saved.segments.size(); s++)
    {
        const SavedKeyframeSegment  & segment = saved.segments[s];



        branch                    = Branch();
        branch.info               = segment.info;
        branch.info.keyframeCount = segment.infos.size();
        branch.firstJournalIndex  = segment.firstJournalIndex;
        branch.records            = segment.records;

        for (i = 0; i < segment.infos.size(); i++)
        {
            entry.info   = segment.infos[i];
            entry.offset = segment.offsets[i];

            branch.entries.push_back (entry);
            m_branchBytes += entry.info.storedBytes;
        }

        m_branches.push_back (std::move (branch));
    }

    m_nextBranchId = saved.nextBranchId;

    journal.LoadRecords (saved.segments[0].firstJournalIndex, saved.segments[0].records);

    m_extents.Reset (m_arenaBytes, GetExtentRoom());

    isPlaced = m_extents.TryAllocate (m_arenaBytes, offset);
    CBRA (isPlaced && offset == 0);

    hr = ReloadLatestWhole();
    CHR (hr);

    ComputeLayout (m_wholeBytes, slotCount, arenaBytes);

    hr = Relayout (slotCount, arenaBytes);
    CHR (hr);

    ScheduleAfter (GetEntry (m_count - 1).info.cycle);

Error:
    if (FAILED (hr))
    {
        Clear();
        journal.Clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsValidSaved
//
//  The active path begins with a whole snapshot, every segment has
//  keyframes in rising cycle order, each branch's id is unique and below
//  the next id, the branch forks from the active path or a branch present,
//  and the
//  keyframes' bytes lie end to end in segment order, filling the arena
//  exactly.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::IsValidSaved (const SavedKeyframes & saved)
{
    constexpr size_t       kMaxStateBytes = 512ull * 1024 * 1024;
    size_t                 end            = 0;
    size_t                 s              = 0;
    size_t                 i              = 0;
    bool                   isValid        = false;
    std::vector<uint32_t>  ids;



    isValid = !saved.segments.empty() && !saved.segments[0].infos.empty() && saved.segments[0].infos[0].isWhole && saved.settings.intervalCycles > 0;

    for (s = 0; isValid && s < saved.segments.size(); s++)
    {
        const SavedKeyframeSegment  & segment = saved.segments[s];



        isValid = !segment.infos.empty() && segment.infos.size() == segment.offsets.size();

        if (isValid && s > 0)
        {
            isValid = segment.info.id != 0 && segment.info.id < saved.nextBranchId && std::find (ids.begin(), ids.end(), segment.info.id) == ids.end();

            ids.push_back (segment.info.id);
        }

        for (i = 0; isValid && i < segment.infos.size(); i++)
        {
            const KeyframeInfo  & info = segment.infos[i];



            isValid = segment.offsets[i] == end && info.sideBytes < info.storedBytes && info.storedBytes <= saved.arena.size() - end && info.stateBytes <= kMaxStateBytes;
            isValid = isValid && (i == 0 || info.cycle > segment.infos[i - 1].cycle);

            end += info.storedBytes;
        }
    }

    for (s = 1; isValid && s < saved.segments.size(); s++)
    {
        isValid = saved.segments[s].info.parent == 0 || std::find (ids.begin(), ids.end(), saved.segments[s].info.parent) != ids.end();
    }

    return isValid && end == saved.arena.size();
}
