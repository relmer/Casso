#pragma once

#include "Pch.h"

#include "Debugger/Reverse/KeyframeCodec.h"
#include "Debugger/Reverse/KeyframeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena
//
//  The keyframes a store has: a table of their facts used as a ring, oldest
//  first, and one arena their packed snapshots are laid into end to end,
//  wrapping around, each followed by its side bytes. The newest entries can
//  be listed before their bytes arrive (List); PlaceNext lays the bytes in
//  for the oldest of those, in the order they were listed. A group is a
//  whole snapshot and the differences against it, and is dropped whole,
//  oldest first, since its differences cannot be restored without it. The
//  newest group is never dropped.
//
//  Touched by the caller's thread only.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeArena
{
public:
    //  Called before the oldest or the newest keyframe is dropped, or before
    //  every one is, while its side bytes can be read.
    using DropCallback = std::function<void (KeyframeDrop drop)>;

    KeyframeArena();
    ~KeyframeArena();

    KeyframeArena (const KeyframeArena &)             = delete;
    KeyframeArena & operator= (const KeyframeArena &) = delete;

    void     SetDropCallback (DropCallback callback) { m_onDrop = std::move (callback); }

    //  The table and arena, taken once; IsAllocated is false until then.
    HRESULT  Allocate    (size_t slotCount, size_t arenaBytes);
    bool     IsAllocated () const { return !m_entries.empty(); }

    //  Moves the keyframes into a table and arena of the sizes given, the
    //  oldest groups dropped first until what is stored fits them and the
    //  budget. With nothing unplaced.
    HRESULT  Relayout (size_t slotCount, size_t arenaBytes, size_t budgetBytes, uint64_t keepPosition = UINT64_MAX);

    //  Lists the next keyframe, its bytes to come. A whole one opens the
    //  newest group.
    KeyframeInfo  & List (const KeyframeInfo & info);

    //  Lays the packed bytes and side bytes in for the oldest unplaced
    //  keyframe, dropping the oldest groups for room. The side bytes are left
    //  out when nothing more can be dropped for them, so a keyframe is never
    //  lost to them.
    HRESULT  PlaceNext (const std::vector<Byte> & packed, const std::vector<Byte> & side, uint64_t checksum, bool & outSideDropped);

    //  The unplaced keyframes go, after their packing failed.
    void     DropUnplaced();

    bool     TryDropOldestGroup (uint64_t keepPosition = UINT64_MAX);
    void     DropOldestGroups   (size_t budgetBytes);
    bool     DropNewerThan      (bool isByPosition, uint64_t limit);

    //  The newest group's counts, worked out again after a drop of the
    //  newest keyframes.
    void     RecountNewestGroup();

    void     Clear  ();
    void     Free   ();

    size_t               GetCount         () const { return m_count; }
    size_t               GetUnplacedCount () const { return m_unplaced; }
    size_t               GetPlacedCount   () const { return m_count - m_unplaced; }
    const KeyframeInfo & GetInfo          (size_t index) const { return GetEntry (index).info; }
    KeyframeInfo       & GetNewestInfo    ()                   { return GetEntry (m_count - 1).info; }

    //  A placed keyframe's packed snapshot and side bytes.
    KeyframeCodec::Packed  GetPacked (size_t index) const;
    void                   GetSide   (size_t index, const Byte *& outData, size_t & outSize) const;

    bool     TryFindAtOrBefore (uint64_t cycle, size_t & outIndex) const;
    bool     TryFindByPosition (uint64_t position, size_t & outIndex) const;
    size_t   FindGroupStart    (size_t index) const;

    size_t   GetSideByteCount  () const { return m_sideBytes; }
    size_t   GetStoredBytes    () const { return m_storedBytes; }
    size_t   GetByteCount      () const { return m_storedBytes + m_wholeBytes; }
    size_t   GetUsedBytes      (size_t budgetBytes) const;
    bool     IsFull            () const { return m_isFull; }
    void     SetHasRoom        ()       { m_isFull = false; }
    size_t   GetCapacity       () const { return m_entries.size(); }
    size_t   GetTableBytes     () const { return m_entries.capacity() * sizeof (Entry); }
    size_t   GetArenaBytes     () const { return m_arenaBytes; }

    //  The newest group: its keyframes, the unpacked size of its whole
    //  snapshot, and the packed sizes of its whole snapshot and of the
    //  differences placed against it.
    size_t   GetGroupLength    () const { return m_groupLength; }
    size_t   GetWholeBytes     () const { return m_wholeBytes; }
    size_t   GetGroupWhole     () const { return m_groupWhole; }
    size_t   GetGroupDiffs     () const { return m_groupDiffs; }

    static constexpr size_t  GetEntryBytes() { return sizeof (Entry); }

private:
    struct ArenaFree
    {
        void  operator() (Byte * arena) const noexcept { VirtualFree (arena, 0, MEM_RELEASE); }
    };

    using ArenaPtr = std::unique_ptr<Byte[], ArenaFree>;

    struct Entry
    {
        KeyframeInfo  info;
        size_t        offset = 0;      // where its packed bytes start in the arena
    };

    Entry        & GetEntry (size_t index)       { return m_entries[(m_first + index) % m_entries.size()]; }
    const Entry  & GetEntry (size_t index) const { return m_entries[(m_first + index) % m_entries.size()]; }

    HRESULT  Place       (const std::vector<Byte> & packed, const std::vector<Byte> & side, size_t & outOffset);
    bool     TryFindRoom (size_t size, size_t & outOffset) const;
    void     PopOldest   ();
    void     NotifyDrop  (KeyframeDrop drop);

    static size_t    GetPackedBytes (const KeyframeInfo & info) { return info.storedBytes - info.sideBytes; }
    static ArenaPtr  AllocateArena  (size_t bytes);

    std::vector<Entry>  m_entries;              // the table, used as a ring
    size_t              m_first       = 0;      // slot of the oldest keyframe
    size_t              m_count       = 0;
    size_t              m_unplaced    = 0;      // the newest keyframes, listed without their bytes
    ArenaPtr            m_arena;
    size_t              m_arenaBytes  = 0;
    size_t              m_arenaEnd    = 0;      // just past the newest packed snapshot
    size_t              m_storedBytes = 0;
    size_t              m_sideBytes   = 0;      // of m_storedBytes, the side bytes
    size_t              m_groupLength = 0;
    size_t              m_wholeBytes  = 0;
    size_t              m_groupWhole  = 0;
    size_t              m_groupDiffs  = 0;
    bool                m_isFull      = false;  // the oldest were dropped for room since the arena last had room to spare
    DropCallback        m_onDrop;
};
