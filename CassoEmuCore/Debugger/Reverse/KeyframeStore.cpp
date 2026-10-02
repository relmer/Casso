#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Core/StateWriter.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Configure
//
//  Takes new settings and empties the store, since keyframes taken on the old
//  interval do not fall on the new one's boundaries.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Configure (const KeyframeSettings & settings)
{
    m_settings = settings;

    Clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Clear()
{
    m_entries.clear();
    m_latestWhole.clear();
    m_latestWhole.shrink_to_fit();

    m_storedBytes  = 0;
    m_groupLength  = 0;
    m_nextDueCycle = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Add
//
//  Stores one machine state taken at the given position and cycle, which
//  must be later than the newest keyframe's. It is stored whole when it
//  opens a group (the store is empty, the group holds wholeEvery keyframes
//  already, or the state's size changed) and as a difference otherwise.
//  Then the next keyframe is scheduled and the budget enforced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Add (
    uint64_t                   position,
    uint64_t                   cycle,
    const std::vector<Byte>  & state)
{
    HRESULT       hr          = S_OK;
    KeyframeInfo  info;
    bool          isEmpty     = m_entries.empty();
    bool          isLater     = isEmpty || cycle > m_entries.back().info.cycle;
    bool          isSameSize  = state.size() == m_latestWhole.size();
    bool          isGroupFull = m_groupLength >= m_settings.wholeEvery;



    CBRAEx (isLater, E_INVALIDARG);

    info.position   = position;
    info.cycle      = cycle;
    info.checksum   = ComputeChecksum (state.data(), state.size());
    info.stateBytes = state.size();

    if (isEmpty || !isSameSize || isGroupFull)
    {
        hr = AddWhole (info, state);
        CHR (hr);
    }
    else
    {
        hr = AddDifference (info, state);
        CHR (hr);
    }

    ScheduleAfter (cycle);
    DropOldestGroups();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Capture
//
//  Saves the machine's whole state and adds it at the CPU's current cycle.
//  Call it on the thread that runs the machine, between instructions.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Capture (
    const MachineHost  & machine,
    uint64_t             position)
{
    HRESULT         hr  = S_OK;
    StateWriter     writer;
    const EmuCpu  * cpu = machine.GetCpu();



    CBRA (cpu);

    hr = machine.SaveState (writer);
    CHR (hr);

    hr = Add (position, cpu->GetTotalCycles(), writer.GetBytes());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddWhole
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::AddWhole (
    const KeyframeInfo       & info,
    const std::vector<Byte>  & state)
{
    HRESULT  hr    = S_OK;
    Entry    entry;



    hr = m_compressor.Compress (state.data(), state.size(), entry.packed);
    CHR (hr);

    entry.info             = info;
    entry.info.isWhole     = true;
    entry.info.storedBytes = entry.packed.size();

    m_storedBytes += entry.info.storedBytes;
    m_entries.push_back (std::move (entry));

    m_latestWhole = state;
    m_groupLength = 1;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDifference
//
//  XORs the state against the group's whole snapshot, so bytes that did not
//  change since it become zero and pack to almost nothing, then packs that.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::AddDifference (
    const KeyframeInfo       & info,
    const std::vector<Byte>  & state)
{
    HRESULT  hr         = S_OK;
    Entry    entry;
    size_t   i          = 0;
    bool     isSameSize = state.size() == m_latestWhole.size();



    CBRA (isSameSize);

    m_scratch.resize (state.size());

    for (i = 0; i < state.size(); i++)
    {
        m_scratch[i] = static_cast<Byte> (state[i] ^ m_latestWhole[i]);
    }

    hr = m_compressor.Compress (m_scratch.data(), m_scratch.size(), entry.packed);
    CHR (hr);

    entry.info             = info;
    entry.info.isWhole     = false;
    entry.info.storedBytes = entry.packed.size();

    m_storedBytes += entry.info.storedBytes;
    m_entries.push_back (std::move (entry));

    m_groupLength++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Restore
//
//  Unpacks keyframe index into outState: the whole snapshot of its group
//  (already unpacked when it is the newest group), and for a difference, the
//  difference XORed onto it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Restore (
    size_t               index,
    std::vector<Byte>  & outState)
{
    HRESULT              hr          = S_OK;
    size_t               count       = m_entries.size();
    size_t               groupStart  = 0;
    bool                 isNewest    = false;
    std::vector<Byte>    olderWhole;
    const Entry        * entry       = nullptr;
    const Entry        * whole       = nullptr;
    const Byte         * base        = nullptr;
    size_t               i           = 0;



    CBRAEx (index < count, E_INVALIDARG);

    entry      = &m_entries[index];
    groupStart = FindGroupStart (index);
    whole      = &m_entries[groupStart];
    isNewest   = groupStart == FindGroupStart (count - 1);

    if (!isNewest)
    {
        hr = m_compressor.Decompress (whole->packed, whole->info.stateBytes, olderWhole);
        CHR (hr);
    }

    base = isNewest ? m_latestWhole.data() : olderWhole.data();

    if (entry->info.isWhole)
    {
        outState.assign (base, base + entry->info.stateBytes);
    }
    else
    {
        CBRA (entry->info.stateBytes == whole->info.stateBytes);

        hr = m_compressor.Decompress (entry->packed, entry->info.stateBytes, outState);
        CHR (hr);

        for (i = 0; i < outState.size(); i++)
        {
            outState[i] ^= base[i];
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindAtOrBefore
//
//  The newest keyframe taken at or before cycle; false when every keyframe
//  is later, or there are none.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::TryFindAtOrBefore (
    uint64_t   cycle,
    size_t   & outIndex) const
{
    std::deque<Entry>::const_iterator  after = std::upper_bound (m_entries.begin(),
                                                                 m_entries.end(),
                                                                 cycle,
                                                                 [] (uint64_t target, const Entry & entry) { return target < entry.info.cycle; });
    bool                               found = after != m_entries.begin();



    if (found)
    {
        outIndex = static_cast<size_t> (std::distance (m_entries.begin(), after)) - 1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoesStateMatch
//
//  Whether state, saved from a machine that replayed to keyframe index, has
//  that keyframe's checksum. A mismatch means the replay diverged.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::DoesStateMatch (
    size_t                     index,
    const std::vector<Byte>  & state) const
{
    bool  isValid = index < m_entries.size();



    return isValid
        && state.size() == m_entries[index].info.stateBytes
        && ComputeChecksum (state.data(), state.size()) == m_entries[index].info.checksum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateAfter
//
//  Drops every keyframe taken after cycle: the recorded future once the user
//  changes something, or history past the last keyframe a replay matched.
//  The next keyframe falls due on the first boundary after the newest one
//  left, or at once when none is left.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::TruncateAfter (uint64_t cycle)
{
    HRESULT  hr      = S_OK;
    bool     dropped = false;
    bool     isEmpty = false;



    while (!m_entries.empty() && m_entries.back().info.cycle > cycle)
    {
        m_storedBytes -= m_entries.back().info.storedBytes;
        m_entries.pop_back();
        dropped = true;
    }

    BAIL_OUT_IF (!dropped, S_OK);

    isEmpty = m_entries.empty();

    if (isEmpty)
    {
        Clear();
    }
    else
    {
        hr = ReloadLatestWhole();
        CHR (hr);

        ScheduleAfter (m_entries.back().info.cycle);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadLatestWhole
//
//  Unpacks the newest group's whole snapshot into m_latestWhole after a
//  truncation, and recounts that group.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::ReloadLatestWhole()
{
    HRESULT        hr         = S_OK;
    size_t         count      = m_entries.size();
    size_t         groupStart = 0;
    const Entry  * whole      = nullptr;



    CBRA (count > 0);

    groupStart = FindGroupStart (count - 1);
    whole      = &m_entries[groupStart];

    hr = m_compressor.Decompress (whole->packed, whole->info.stateBytes, m_latestWhole);
    CHR (hr);

    m_groupLength = count - groupStart;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindGroupStart
//
//  The index of the whole snapshot keyframe index belongs to. The oldest
//  entry is always whole, because groups are dropped whole.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::FindGroupStart (size_t index) const
{
    while (index > 0 && !m_entries[index].info.isWhole)
    {
        index--;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropOldestGroups
//
//  While over the budget, drops the oldest group: its whole snapshot and
//  every difference against it. The newest group stays even over budget.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::DropOldestGroups()
{
    size_t  nextGroup = 0;



    while (GetByteCount() > m_settings.budgetBytes)
    {
        nextGroup = 1;

        while (nextGroup < m_entries.size() && !m_entries[nextGroup].info.isWhole)
        {
            nextGroup++;
        }

        if (nextGroup >= m_entries.size())
        {
            break;
        }

        for (; nextGroup > 0; nextGroup--)
        {
            m_storedBytes -= m_entries.front().info.storedBytes;
            m_entries.pop_front();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleAfter
//
//  The next keyframe falls due on the first multiple of the interval after
//  cycle, so keyframes land on the same boundaries however late within an
//  instruction each one was taken.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ScheduleAfter (uint64_t cycle)
{
    uint64_t  interval = std::max<uint64_t> (m_settings.intervalCycles, 1);



    m_nextDueCycle = (cycle / interval + 1) * interval;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeChecksum
//
//  64-bit FNV-1a over the state eight bytes at a time, folding the high half
//  down after each word so a change in any bit reaches every later bit, then
//  the tail a byte at a time. Not cryptographic: it detects a replay that diverged, not
//  tampering.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t KeyframeStore::ComputeChecksum (
    const Byte  * data,
    size_t        size)
{
    constexpr uint64_t  kOffset = 0xCBF29CE484222325ULL;
    constexpr uint64_t  kPrime  = 0x00000100000001B3ULL;
    constexpr int       kFold   = 32;
    uint64_t            hash    = kOffset;
    uint64_t            word    = 0;
    size_t              i       = 0;



    for (i = 0; i + sizeof (word) <= size; i += sizeof (word))
    {
        memcpy (&word, data + i, sizeof (word));
        hash  = (hash ^ word) * kPrime;
        hash ^= hash >> kFold;
    }

    for (; i < size; i++)
    {
        hash = (hash ^ data[i]) * kPrime;
    }

    return hash;
}
