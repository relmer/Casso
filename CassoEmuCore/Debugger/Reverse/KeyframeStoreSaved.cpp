#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/InputJournal.h"





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
    HRESULT  hr       = S_OK;
    bool     isCopied = false;
    bool     hasAny   = false;



    outSaved = SavedKeyframes();

    hr = WaitForPending();
    CHR (hr);

    hasAny = m_arena.GetCount() > 0;
    CBRA (hasAny);

    outSaved.settings = m_settings;

    m_arena.Export (outSaved);

    outSaved.segments[0].firstJournalIndex = journal.GetBeginIndex();

    isCopied = journal.TryCopyRecords (outSaved.segments[0].firstJournalIndex, UINT64_MAX, outSaved.segments[0].records);
    CBRA (isCopied);

Error:
    return hr;
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
    HRESULT  hr         = S_OK;
    size_t   budget     = m_settings.budgetBytes;
    size_t   slotCount  = 0;
    size_t   arenaBytes = 0;
    bool     isValid    = IsValidSaved (saved);



    hr = WaitForPending();
    CHR (hr);

    Clear();

    CBREx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_settings             = saved.settings;
    m_settings.budgetBytes = budget;

    hr = m_arena.Import (saved);
    CHR (hr);

    journal.LoadRecords (saved.segments[0].firstJournalIndex, saved.segments[0].records);

    hr = ReloadLatestWhole();
    CHR (hr);

    m_packQueue.Reserve (m_arena.GetWholeBytes());

    ComputeLayout (m_arena.GetWholeBytes(), slotCount, arenaBytes);

    hr = m_arena.Relayout (slotCount, arenaBytes, m_settings.budgetBytes);
    CHR (hr);

    ScheduleAfter (m_arena.GetNewestInfo().cycle);

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
        isValid = IsRootedSaved (saved, s);
    }

    return isValid && end == saved.arena.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRootedSaved
//
//  Whether the chain of parents from branch segment s ends at the active
//  path, parent 0, in fewer steps than there are branches. A parent missing
//  from the saved branches, or a chain that loops, fails.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::IsRootedSaved (
    const SavedKeyframes  & saved,
    size_t                  s)
{
    size_t    steps    = 0;
    size_t    j        = 0;
    uint32_t  parent   = saved.segments[s].info.parent;
    bool      isFound  = false;
    bool      isRooted = true;



    while (isRooted && parent != 0)
    {
        isFound = false;

        for (j = 1; !isFound && j < saved.segments.size(); j++)
        {
            if (saved.segments[j].info.id == parent)
            {
                isFound = true;
                parent  = saved.segments[j].info.parent;
            }
        }

        steps++;
        isRooted = isFound && steps < saved.segments.size();
    }

    return isRooted;
}
