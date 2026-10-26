#include "Pch.h"

#include "Ui/DiskInspector/EditController.h"





static constexpr Byte  s_kHighBit = 0x80;
static constexpr Byte  s_kLowBits = 0x7F;





////////////////////////////////////////////////////////////////////////////////
//
//  GetHexValue
//
//  A hex digit's value, or -1.
//
////////////////////////////////////////////////////////////////////////////////

static int GetHexValue (wchar_t ch)
{
    int  value = -1;



    if (ch >= L'0' && ch <= L'9')
    {
        value = ch - L'0';
    }
    else if (ch >= L'A' && ch <= L'F')
    {
        value = ch - L'A' + 10;
    }
    else if (ch >= L'a' && ch <= L'f')
    {
        value = ch - L'a' + 10;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PendingEdit::CountChanged
//
////////////////////////////////////////////////////////////////////////////////

int PendingEdit::CountChanged() const
{
    int  count = 0;



    for (int i = 0; i < DiskFieldFormat::kSectorBytes; i++)
    {
        count += IsChanged (i) ? 1 : 0;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::GetUneditableReason
//
//  In the order a user would fix them: a sector with no data field first,
//  then its address field, then what is wrong inside the data field, and a
//  damaged record last, since nothing on it can be read reliably.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditController::GetUneditableReason (const TrackAnalysis & track, int sectorIndex)
{
    std::wstring  reason;



    if (sectorIndex < 0 || sectorIndex >= static_cast<int> (track.sectors.size()))
    {
        reason = L"No sector selected";
    }
    else
    {
        const AnalyzedSector &  sector = track.sectors[sectorIndex];

        if (sector.dataField < 0)
        {
            reason = L"No data field";
        }
        else if (sector.addressField >= 0 && sector.isAddressCheck && !sector.isAddressGood)
        {
            reason = L"Address checksum failed";
        }
        else if (track.fields[sector.dataField].hasNoise)
        {
            reason = L"Noise inside the data field";
        }
        else if (!track.fields[sector.dataField].data.invalidNibbles.empty())
        {
            reason = L"Nibbles outside the translate table";
        }
        else if (track.context.isDamaged)
        {
            reason = L"Damaged track record";
        }
    }

    return reason;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::IsChecksumGoodAfter
//
//  A recomputed checksum always passes. In KeepStored mode, the result
//  passes when the new bytes encode to the stored checksum.
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::IsChecksumGoodAfter (const PendingEdit & edit, const TrackAnalysis & track)
{
    int                field    = FindField (track, edit.sectorNumber, edit.fieldKind, edit.fieldStartCell);
    vector<Byte>       body;
    Byte               checksum = 0;
    bool               isGood   = edit.checksumMode == ChecksumMode::Recompute;



    if (!isGood && field >= 0)
    {
        const DataFieldDecode &  decode = track.fields[field].data;

        DiskFieldFormat::EncodeData (edit.fieldKind, edit.bytes, body, checksum);
        isGood = decode.isStoredValid && DiskFieldFormat::TranslateNibble (edit.fieldKind, checksum) == decode.storedChecksum;
    }

    return isGood;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Begin
//
////////////////////////////////////////////////////////////////////////////////

int EditController::Begin (const TrackAnalysis & track, int quarterTrack, int sectorIndex, std::wstring & outReason)
{
    int  edit = Find (track, sectorIndex);



    outReason = GetUneditableReason (track, sectorIndex);

    if (edit < 0 && outReason.empty())
    {
        const AnalyzedSector &  sector = track.sectors[sectorIndex];
        const LocatedField   &  data   = track.fields[sector.dataField];
        PendingEdit             pending;

        pending.slot              = track.slot;
        pending.quarterTrack      = quarterTrack;
        pending.fieldStartCell    = data.startCell;
        pending.sectorNumber      = sector.sector;
        pending.fieldKind         = data.kind;
        pending.bytesAtStart      = data.data.bytes;
        pending.bytes             = data.data.bytes;
        pending.writeCountAtStart = track.guestWriteCount;

        m_edits.push_back (pending);
        edit = static_cast<int> (m_edits.size()) - 1;
    }

    return outReason.empty() ? edit : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Find
//
////////////////////////////////////////////////////////////////////////////////

int EditController::Find (int slot, uint32_t fieldStartCell) const
{
    int  found = -1;



    for (size_t i = 0; found < 0 && i < m_edits.size(); i++)
    {
        found = (m_edits[i].slot == slot && m_edits[i].fieldStartCell == fieldStartCell) ? static_cast<int> (i) : -1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Find
//
////////////////////////////////////////////////////////////////////////////////

int EditController::Find (const TrackAnalysis & track, int sectorIndex) const
{
    int  found = -1;



    if (sectorIndex >= 0 && sectorIndex < static_cast<int> (track.sectors.size()) && track.sectors[sectorIndex].dataField >= 0)
    {
        found = Find (track.slot, track.fields[track.sectors[sectorIndex].dataField].startCell);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::TypeHexDigit
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::TypeHexDigit (int edit, int offset, bool isHighNibble, wchar_t digit)
{
    int   value   = GetHexValue (digit);
    bool  isTyped = false;



    if (value >= 0 && edit >= 0 && edit < static_cast<int> (m_edits.size()) && offset >= 0 && offset < DiskFieldFormat::kSectorBytes)
    {
        Byte  old = m_edits[edit].bytes[offset];

        isTyped = SetBytes (edit, offset, { static_cast<Byte> (isHighNibble ? ((value << 4) | (old & 0x0F)) : ((old & 0xF0) | value)) });
    }

    return isTyped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::TypeCharacter
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::TypeCharacter (int edit, int offset, wchar_t ch)
{
    bool  isTyped = false;



    if (ch >= 0x20 && ch < 0x7F && edit >= 0 && edit < static_cast<int> (m_edits.size()) && offset >= 0 && offset < DiskFieldFormat::kSectorBytes)
    {
        isTyped = SetBytes (edit, offset, { static_cast<Byte> ((m_edits[edit].bytes[offset] & s_kHighBit) | (ch & s_kLowBits)) });
    }

    return isTyped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Paste
//
//  Hex text is read as pairs of digits, ignoring spaces. Each pasted
//  character takes the high bit of the byte it replaces, as typing does.
//
////////////////////////////////////////////////////////////////////////////////

int EditController::Paste (int edit, int offset, const std::wstring & text, bool isTextColumn, int & outOverflow)
{
    vector<Byte>  values;
    int           high    = -1;
    int           digit   = 0;
    bool          isValid = edit >= 0 && edit < static_cast<int> (m_edits.size()) && offset >= 0 && offset < DiskFieldFormat::kSectorBytes;
    int           room    = DiskFieldFormat::kSectorBytes - offset;
    int           written = 0;



    outOverflow = 0;

    for (wchar_t ch : isValid ? text : std::wstring())
    {
        if (isTextColumn)
        {
            if (ch >= 0x20 && ch < 0x7F)
            {
                values.push_back (static_cast<Byte> (ch & s_kLowBits));
            }
        }
        else if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n')
        {
            digit   = GetHexValue (ch);
            isValid = isValid && digit >= 0;

            if (high < 0)
            {
                high = digit;
            }
            else
            {
                values.push_back (static_cast<Byte> ((high << 4) | digit));
                high = -1;
            }
        }
    }

    isValid = isValid && high < 0 && !values.empty();

    if (isValid)
    {
        outOverflow = std::max (0, static_cast<int> (values.size()) - room);
        values.resize (std::min (static_cast<int> (values.size()), room));

        for (size_t i = 0; isTextColumn && i < values.size(); i++)
        {
            values[i] = static_cast<Byte> (values[i] | (m_edits[edit].bytes[offset + i] & s_kHighBit));
        }

        written = SetBytes (edit, offset, values) ? static_cast<int> (values.size()) : 0;
    }

    return written;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Undo
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::Undo()
{
    bool  isDone = CanUndo();



    if (isDone)
    {
        m_undoAt--;
        Restore (m_actions[m_undoAt], true);
    }

    return isDone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Redo
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::Redo()
{
    bool  isDone = CanRedo();



    if (isDone)
    {
        Restore (m_actions[m_undoAt], false);
        m_undoAt++;
    }

    return isDone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::DiscardAll
//
//  Each is one action, so Ctrl+Z brings the edits back.
//
////////////////////////////////////////////////////////////////////////////////

void EditController::DiscardAll()
{
    Action  action;



    if (!m_edits.empty())
    {
        action.isWholeSet = true;
        action.setBefore  = m_edits;
        m_edits.clear();
        Record (std::move (action));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::DiscardSector
//
////////////////////////////////////////////////////////////////////////////////

void EditController::DiscardSector (int edit)
{
    Action  action;



    if (edit >= 0 && edit < static_cast<int> (m_edits.size()))
    {
        action.isWholeSet = true;
        action.setBefore  = m_edits;
        m_edits.erase (m_edits.begin() + edit);
        action.setAfter   = m_edits;
        Record (std::move (action));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::SetChecksumMode
//
////////////////////////////////////////////////////////////////////////////////

void EditController::SetChecksumMode (int edit, ChecksumMode mode)
{
    if (edit >= 0 && edit < static_cast<int> (m_edits.size()))
    {
        m_edits[edit].checksumMode = mode;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::SetSector
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::SetSector (int edit, const PendingEdit::Sector & bytes)
{
    bool  isValid = edit >= 0 && edit < static_cast<int> (m_edits.size());



    return isValid && SetBytes (edit, 0, vector<Byte> (bytes.begin(), bytes.end()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Follow
//
//  The undo stack refers to edits by index, so it is cleared when an edit
//  is dropped; the edits themselves move with their fields.
//
////////////////////////////////////////////////////////////////////////////////

void EditController::Follow (const DiskAnalysis & analysis, vector<PendingEdit> & outDiscarded)
{
    vector<PendingEdit>  remaining;
    int                  field  = -1;
    int                  sector = -1;



    for (PendingEdit & edit : m_edits)
    {
        const TrackAnalysis *  track = (edit.slot >= 0 && edit.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[edit.slot].get() : nullptr;

        field  = (track != nullptr) ? FindField (*track, edit.sectorNumber, edit.fieldKind, edit.fieldStartCell) : -1;
        sector = -1;

        for (size_t s = 0; field >= 0 && sector < 0 && s < track->sectors.size(); s++)
        {
            sector = (track->sectors[s].dataField == field) ? static_cast<int> (s) : -1;
        }

        if (sector < 0 || !GetUneditableReason (*track, sector).empty())
        {
            outDiscarded.push_back (edit);
            continue;
        }

        edit.fieldStartCell = track->fields[field].startCell;
        edit.isOutOfDate    = edit.isOutOfDate || track->fields[field].data.bytes != edit.bytesAtStart;
        remaining.push_back (edit);
    }

    if (remaining.size() != m_edits.size())
    {
        m_actions.clear();
        m_undoAt = 0;
    }

    m_edits = std::move (remaining);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::BuildWrites
//
////////////////////////////////////////////////////////////////////////////////

vector<SectorWrite> EditController::BuildWrites (const DiskAnalysis & analysis) const
{
    vector<SectorWrite>  writes;
    int                  field = -1;



    for (const PendingEdit & edit : m_edits)
    {
        const TrackAnalysis *  track = (edit.slot >= 0 && edit.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[edit.slot].get() : nullptr;
        SectorWrite            write;

        if (edit.CountChanged() == 0)
        {
            continue;
        }

        field = (track != nullptr) ? FindField (*track, edit.sectorNumber, edit.fieldKind, edit.fieldStartCell) : -1;

        write.slot          = edit.slot;
        write.dataFieldCell = static_cast<int> (edit.fieldStartCell);
        write.sector        = edit.sectorNumber;
        write.checksumMode  = edit.checksumMode;
        write.bytes         = (edit.isOutOfDate && field >= 0) ? track->fields[field].data.bytes : edit.bytes;

        for (int i = 0; edit.isOutOfDate && i < DiskFieldFormat::kSectorBytes; i++)
        {
            write.bytes[i] = edit.IsChanged (i) ? edit.bytes[i] : write.bytes[i];
        }

        writes.push_back (write);
    }

    return writes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::HasChanges
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::HasChanges (int slot, Byte sectorNumber) const
{
    return std::any_of (m_edits.begin(), m_edits.end(),
                        [&] (const PendingEdit & edit) { return edit.slot == slot && edit.sectorNumber == sectorNumber && edit.CountChanged() > 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::CountChangedSectors
//
////////////////////////////////////////////////////////////////////////////////

int EditController::CountChangedSectors (int slot) const
{
    return static_cast<int> (std::count_if (m_edits.begin(), m_edits.end(), [slot] (const PendingEdit & edit) { return edit.slot == slot && edit.CountChanged() > 0; }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::GetCounts
//
////////////////////////////////////////////////////////////////////////////////

EditController::Counts EditController::GetCounts() const
{
    Counts  counts;
    int     changed = 0;



    for (const PendingEdit & edit : m_edits)
    {
        changed         = edit.CountChanged();
        counts.bytes   += changed;
        counts.sectors += (changed > 0) ? 1 : 0;
    }

    return counts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Record
//
//  A new action drops the actions undone before it.
//
////////////////////////////////////////////////////////////////////////////////

void EditController::Record (Action action)
{
    m_actions.resize (m_undoAt);
    m_actions.push_back (std::move (action));
    m_undoAt = static_cast<int> (m_actions.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::Restore
//
////////////////////////////////////////////////////////////////////////////////

void EditController::Restore (const Action & action, bool isUndo)
{
    const vector<Byte> &  values = isUndo ? action.before : action.after;



    if (action.isWholeSet)
    {
        m_edits = isUndo ? action.setBefore : action.setAfter;
    }
    else if (action.edit >= 0 && action.edit < static_cast<int> (m_edits.size()))
    {
        std::copy (values.begin(), values.end(), m_edits[action.edit].bytes.begin() + action.offset);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::SetBytes
//
//  One action for the run of bytes; a run that changes nothing is not an
//  action.
//
////////////////////////////////////////////////////////////////////////////////

bool EditController::SetBytes (int edit, int offset, const vector<Byte> & values)
{
    PendingEdit &  target = m_edits[edit];
    Action         action;



    action.edit   = edit;
    action.offset = offset;
    action.before.assign (target.bytes.begin() + offset, target.bytes.begin() + offset + values.size());
    action.after  = values;

    if (action.before != action.after)
    {
        std::copy (values.begin(), values.end(), target.bytes.begin() + offset);
        Record (std::move (action));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditController::FindField
//
//  The data field for a sector number and encoding whose start is nearest
//  a cell, around the index too.
//
////////////////////////////////////////////////////////////////////////////////

int EditController::FindField (const TrackAnalysis & track, Byte sectorNumber, DiskFieldKind kind, uint32_t nearCell)
{
    uint32_t  length   = std::max<uint32_t> (track.framed.cellCount, 1);
    uint32_t  distance = 0;
    uint32_t  best     = UINT32_MAX;
    int       found    = -1;



    for (const AnalyzedSector & sector : track.sectors)
    {
        if (sector.dataField < 0 || sector.sector != sectorNumber || track.fields[sector.dataField].kind != kind)
        {
            continue;
        }

        distance = (track.fields[sector.dataField].startCell + length - nearCell % length) % length;
        distance = std::min (distance, length - distance);

        if (distance < best)
        {
            best  = distance;
            found = sector.dataField;
        }
    }

    return found;
}
