#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Devices/Disk/SectorWrite.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PendingEdit
//
//  The new bytes of one sector, not yet applied (FR-097). The sector is the
//  data field that starts at `fieldStartCell` on record `slot`, so an edit
//  changes the field it was made on, not the first with the same number.
//  `bytesAtStart` has the sector as decoded when editing began; a byte is
//  changed where `bytes` differs from it. `isOutOfDate` is set when the
//  sector's decoded bytes differ from `bytesAtStart` (FR-108). When the
//  guest writes the sector, the bytes the user didn't change take the
//  guest's values, and `conflicts` marks each changed byte the guest also
//  wrote.
//
////////////////////////////////////////////////////////////////////////////////

struct PendingEdit
{
    using Sector = std::array<Byte, DiskFieldFormat::kSectorBytes>;

    int            slot              = -1;
    int            quarterTrack      = -1;
    uint32_t       fieldStartCell    = 0;
    Byte           sectorNumber      = 0;
    DiskFieldKind  fieldKind         = DiskFieldKind::Sixteen;
    Sector         bytesAtStart      = {};
    Sector         bytes             = {};
    uint64_t       writeCountAtStart = 0;
    ChecksumMode   checksumMode      = ChecksumMode::Recompute;
    bool           isOutOfDate       = false;

    std::bitset<DiskFieldFormat::kSectorBytes>  conflicts;

    bool  IsChanged    (int offset) const { return bytes[offset] != bytesAtStart[offset]; }
    bool  IsConflict   (int offset) const { return conflicts.test (static_cast<size_t> (offset)); }
    int   CountChanged () const;
};





////////////////////////////////////////////////////////////////////////////////
//
//  EditController
//
//  Every pending edit on a disk and the undo stack over them (FR-096 to
//  FR-098, FR-108). Typing overwrites in place, so a sector always has 256
//  bytes; a typed character takes the high bit of the byte it replaces.
//  Each typing, paste or discard is one action for Ctrl+Z and Ctrl+Y, which
//  act on pending edits only. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class EditController
{
public:
    struct Counts
    {
        int  bytes   = 0;
        int  sectors = 0;
    };

    //  Why a sector cannot be edited, or empty when it can (FR-096).
    static std::wstring  GetUneditableReason (const TrackAnalysis & track, int sectorIndex);

    //  The data checksum result the sector gets once applied (FR-098).
    static bool          IsChecksumGoodAfter (const PendingEdit & edit, const TrackAnalysis & track);

    //  Starts editing a sector, or finds the edit already pending on it.
    //  Returns its index, or -1 with the reason the sector cannot be edited.
    int   Begin (const TrackAnalysis & track, int quarterTrack, int sectorIndex, std::wstring & outReason);

    //  The pending edit on a sector, or -1.
    int   Find  (int slot, uint32_t fieldStartCell) const;
    int   Find  (const TrackAnalysis & track, int sectorIndex) const;

    //  Sets the hex digit at `offset`, high nibble when `isHighNibble`.
    bool  TypeHexDigit (int edit, int offset, bool isHighNibble, wchar_t digit);

    //  Sets the byte at `offset` from a character, keeping its high bit.
    bool  TypeCharacter (int edit, int offset, wchar_t ch);

    //  Overwrites from `offset` with pasted hex or text, stopping at $FF.
    //  Returns the bytes written; `outOverflow` has the bytes that did not
    //  fit. Hex text that does not read as pairs of hex digits writes nothing.
    int   Paste (int edit, int offset, const std::wstring & text, bool isTextColumn, int & outOverflow);

    bool  Undo    ();
    bool  Redo    ();
    bool  CanUndo () const { return m_undoAt > 0; }
    bool  CanRedo () const { return m_undoAt < static_cast<int> (m_actions.size()); }

    void  DiscardAll    ();
    void  DiscardSector (int edit);

    void  SetChecksumMode (int edit, ChecksumMode mode);

    //  All 256 bytes of a sector at once, as one action, for "Use B's
    //  bytes" (FR-123).
    bool  SetSector (int edit, const PendingEdit::Sector & bytes);

    //  Moves each edit to the field with its sector number and encoding
    //  nearest its old cell on its record, after a guest write, undo, redo
    //  or reload. An edit whose sector's bytes changed is marked out of date;
    //  one whose sector cannot be found or edited is dropped, and its sector
    //  number is added to `outDiscarded`.
    void  Follow (const DiskAnalysis & analysis, vector<PendingEdit> & outDiscarded);

    //  The writer's requests for every edit with changes. An out-of-date
    //  edit writes only the bytes the user changed over the sector's current
    //  contents (FR-108).
    vector<SectorWrite>  BuildWrites (const DiskAnalysis & analysis) const;

    //  Edits that write these edits' changed bytes over each sector's current
    //  bytes, or with `isReverse` the bytes they replaced, for undoing or
    //  redoing a save after the guest wrote the track.
    EditController       MakeOverCurrent (bool isReverse) const;

    //  Whether each changed byte has its edited value in the sectors as now
    //  decoded.
    bool                 IsCurrentIn (const DiskAnalysis & analysis) const;

    //  The guest write count of each edit on a record, from the count a save
    //  or undo left.
    void                 SetWriteCount (int slot, uint64_t count);

    //  Whether a sector of a record has changed bytes pending, and how many
    //  of a record's sectors do, for the marks in other views (FR-097).
    bool  HasChanges          (int slot, Byte sectorNumber) const;
    int   CountChangedSectors (int slot) const;

    Counts                       GetCounts      () const;
    bool                         IsEmpty        () const { return GetCounts().sectors == 0; }
    bool                         HasSameChanges (const EditController & other) const;
    const vector<PendingEdit>  & GetEdits       () const { return m_edits; }
    const PendingEdit          & GetEdit        (int edit) const { return m_edits[edit]; }

private:
    struct Action
    {
        int                  edit       = -1;
        int                  offset     = 0;
        vector<Byte>         before;
        vector<Byte>         after;
        bool                 isWholeSet = false;
        vector<PendingEdit>  setBefore;
        vector<PendingEdit>  setAfter;
    };

    void        Record    (Action action);
    void        Restore   (const Action & action, bool isUndo);
    bool        SetBytes  (int edit, int offset, const vector<Byte> & values);
    static int  FindField (const TrackAnalysis & track, Byte sectorNumber, DiskFieldKind kind, uint32_t nearCell);

    vector<PendingEdit>  m_edits;
    vector<Action>       m_actions;
    int                  m_undoAt = 0;
};
