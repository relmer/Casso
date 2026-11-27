#include "Pch.h"

#include "Ui/DiskInspector/Components/EditingPart.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32HostDialogs.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/Views/SectorsView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::EditingPart
//
////////////////////////////////////////////////////////////////////////////////

EditingPart::EditingPart (InspectorSession & session, IInspectorComponentHost & host, SectorsView & byteView) :
    m_session  (session),
    m_host     (host),
    m_sectorsView (byteView)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::Create
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::Create()
{
    m_editBar    = m_host.GetControlParent().CreateChild<EditBar>();
    m_editButton = m_host.GetControlParent().CreateChild<DxuiButton> (L"Edit");

    m_editButton->SetOnClick ([this] () { ToggleEditing(); });

    //  A double-click on a byte starts editing with the caret on it.
    m_sectorsView.SetOnOpen ([this] (int offset)
    {
        if (!m_sectorsView.IsEditing())
        {
            ToggleEditing();
        }

        if (m_sectorsView.IsEditing())
        {
            m_sectorsView.GoToByte (offset);
        }
    });

    //  The bar goes at the right of the Sector data tab's first header line
    //  when its controls fit there, and otherwise on a separate row between
    //  the header and the bytes.
    m_sectorsView.SetEditRow ([this] (const RECT & inlinePx, const RECT & rowPx, int & outLeft)
    {
        int   inlineW = m_editBar->GetInlineWidthPx (m_session.scaler);
        bool  isIn    = inlineW >= 0 && inlineW <= inlinePx.right - inlinePx.left;
        int   height  = isIn ? 0 : m_editBar->GetHeightPx (rowPx.right - rowPx.left, m_session.scaler);
        RECT  bounds  = isIn ? RECT { inlinePx.right - inlineW, inlinePx.top, inlinePx.right, inlinePx.bottom }
                             : RECT { rowPx.left, rowPx.top, rowPx.right, rowPx.top + height };
        RECT  shown   = m_editBar->GetBounds();

        if (!EqualRect (&bounds, &shown))
        {
            m_editBar->Layout (bounds, m_session.scaler);
        }

        outLeft = isIn ? bounds.left : inlinePx.right;
        return height;
    });

    m_editBar->SetOnChecksum      ([this] (bool isOn) { m_session.edits.SetChecksumMode (GetSelectedEdit(), isOn ? ChecksumMode::Recompute : ChecksumMode::KeepStored); });

    m_sectorsView.SetCopySector ([this] (int toSide) { return CanCopySector (toSide); },
                                 [this] (int toSide) { if (toSide == 0) { UseBBytes(); } else { UseABytes(); } });
    m_editBar->SetOnPauseApply    ([this] () { PauseApply(); });
    m_editBar->SetOnCancelApply   ([this] () { CancelApply(); });
    m_editBar->SetOnUndoApplied   ([this] () { UndoApplied(); });
    m_editBar->SetOnRedoApplied   ([this] () { RedoApplied(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::Layout
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    m_editBar->Layout (area, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int EditingPart::GetHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    return m_editBar->GetHeightPx (widthPx, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::SetShown
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::SetShown (bool isShown)
{
    m_editBar->SetVisible    (isShown);
    m_editButton->SetVisible (isShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::LayoutEditButton
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::LayoutEditButton (const RECT & rectPx, const DxuiDpiScaler & scaler)
{
    m_editButton->Layout (rectPx, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::Update
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::Update()
{
    UpdateEditBar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditingPart::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    return m_editBar->IsVisible() ? m_editBar->GetTip (pointPx, outAnchorPx) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::OnKey
//
//  While editing, the Sector data tab takes typing, Ctrl+Z, Ctrl+Y and
//  Ctrl+V, and Escape stops editing (FR-097).
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::OnKey (const DxuiKeyEvent & ev)
{
    return m_session.keyTarget == KeyTarget::SectorData && m_sectorsView.IsEditing() && HandleEditKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::TakeReply
//
//  The host's reply to an apply or a saved copy; false for any other.
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::TakeReply (const InspectorReply & reply)
{
    bool  isTaken = false;



    if (reply.kind == InspectorRequestKind::Apply && reply.requestId == m_applyRequest)
    {
        TakeApplyReply (reply);
        isTaken = true;
    }
    else if (reply.requestId == m_saveCopyRequest)
    {
        TakeSaveCopyReply (reply);
        isTaken = true;
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::OnDiskStarted
//
//  Pending edits are discarded with the disk they were made on, or when the
//  disk is ejected. A remount of the same file, as Reset, Power Cycle and a
//  machine change do, is a reload: the edits follow their fields once each
//  track is analyzed again (FR-108). A new disk, or a file read again after
//  it changed outside the window, clears the apply history (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::OnDiskStarted (const DiskChange & change)
{
    bool  isNewDisk = change.isNewDisk;
    bool  isReload  = change.isReload;



    if (!change.hasDisk)
    {
        OnEjected();
    }
    else if (isNewDisk && !isReload && !m_session.edits.IsEmpty())
    {
        m_session.editNote = L"Unsaved changes were discarded: the disk in the drive changed";
    }

    if (change.hasDisk && isNewDisk && !isReload)
    {
        m_session.edits = EditController();
        m_sectorsView.SetEditing (false);
    }

    if (change.hasDisk && (isNewDisk || change.isReread))
    {
        m_history.clear();
        m_historyAt = 0;
        m_session.appliedSlots.clear();
    }

    //  The result of an apply, undo or redo is about the file as it was.
    if (change.isReread)
    {
        m_session.editNote.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::OnAnalysisChanged
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::OnAnalysisChanged()
{
    FollowEdits();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::OnEjected
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::OnEjected()
{
    if (!m_session.edits.IsEmpty())
    {
        m_session.editNote = L"Unsaved changes were discarded: the disk was ejected";
        m_host.SetStatusNote (m_session.editNote);
    }

    m_session.edits = EditController();
    m_sectorsView.SetEditing (false);
    m_history.clear();
    m_historyAt = 0;
    m_session.appliedSlots.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetSelectedEdit
//
//  The selected sector's pending edit, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int EditingPart::GetSelectedEdit() const
{
    const TrackAnalysis *  track  = nullptr;
    int                    sector = -1;
    int                    side   = 0;



    return GetEditTarget (track, sector, side) ? m_session.edits.Find (*track, sector, side) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetEditTarget
//
//  The sector edits go to: while comparing, B's paired sector once B's view
//  was clicked, and otherwise A's selected sector.
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::GetEditTarget (const TrackAnalysis *& outTrack, int & outSector, int & outSide) const
{
    outSide   = m_session.comparison.IsComparing() ? m_sectorsView.GetActiveSide() : 0;
    outTrack  = (outSide == 0) ? m_session.model.GetTrack() : m_session.context.trackB;
    outSector = (outSide == 0) ? m_session.model.GetSectorIndex() : m_sectorsView.GetSelectedSectorB();

    return outTrack != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetBEditBlock
//
//  Why B's sectors cannot be edited, or empty when they can: B is saved to
//  its file, so it must be an image file Casso can write.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditingPart::GetBEditBlock() const
{
    const DiskCopy *  copy = m_session.comparison.GetB().copy.get();



    return (copy == nullptr)                                                             ? std::wstring (L"No second image")
         : (m_session.comparison.GetSourceB().kind != ComparisonSourceKind::ImageFile)   ? std::wstring (L"The second image is a drive's disk; edit it in its own inspector")
         : copy->isReadOnly                                                              ? std::wstring (L"The second image's file is read-only")
                                                                                         : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::BeginTarget
//
//  Begins an edit on the sector edits go to, or finds the one pending.
//  Returns -1 with the reason when that sector cannot be edited.
//
////////////////////////////////////////////////////////////////////////////////

int EditingPart::BeginTarget (std::wstring & outReason)
{
    const TrackAnalysis *  track  = nullptr;
    int                    sector = -1;
    int                    side   = 0;
    int                    edit   = -1;



    outReason = GetEditTarget (track, sector, side) ? std::wstring() : std::wstring (L"No track");
    outReason = (outReason.empty() && side == 1) ? GetBEditBlock() : outReason;
    outReason = (outReason.empty() && side == 1 && sector < 0) ? std::wstring (L"The second image has no sector to pair with this one") : outReason;

    if (outReason.empty())
    {
        edit = m_session.edits.Begin (*track, m_session.model.GetQuarterTrack(), sector, outReason, side);
    }

    if (edit >= 0 && side == 1)
    {
        m_session.editsBMedia = m_session.comparison.GetB().mediaId;
    }

    return edit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::DropStaleBEdits
//
//  B's pending edits go with B's disk: when comparing ends or B is another
//  image, they are discarded.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::DropStaleBEdits()
{
    bool  hasB = std::any_of (m_session.edits.GetEdits().begin(), m_session.edits.GetEdits().end(), [] (const PendingEdit & edit) { return edit.side == 1; });



    if (hasB && (!m_session.comparison.IsComparing() || m_session.comparison.GetB().mediaId != m_session.editsBMedia))
    {
        m_session.edits.RemoveSide (1);
        m_session.editNote = L"Unsaved changes to the second image were discarded: it is no longer compared";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::ToggleEditing
//
//  Starting to edit begins an edit on the selected sector, selects its
//  first byte when no byte is selected, and moves the keys to the Sector
//  data tab.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::ToggleEditing()
{
    const TrackAnalysis *  track = m_session.model.GetTrack();
    std::wstring           reason;
    int                    edit  = -1;



    if (m_sectorsView.IsEditing())
    {
        m_sectorsView.SetEditing (false);
    }
    else if (track != nullptr && IsLiveEditConfirmed())
    {
        edit               = BeginTarget (reason);
        m_session.editNote = reason;

        if (edit >= 0)
        {
            m_sectorsView.SetEditing (true);
            m_session.keyTarget = KeyTarget::SectorData;

            if (m_session.model.GetFirstByte() < 0)
            {
                m_session.model.SelectBytes (0, 1, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::HandleEditKey
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::HandleEditKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = true;



    m_session.editNote.clear();

    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'Z')
    {
        Undo();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'Y' || (ev.shift && ev.vk == 'Z')))
    {
        Redo();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'V')
    {
        PasteIntoEdit();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE)
    {
        ToggleEditing();
    }
    else
    {
        isHandled = m_sectorsView.OnEditKey (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::PasteIntoEdit
//
//  From the cursor, in the column it is in; the bytes past $FF are counted
//  in the note.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::PasteIntoEdit()
{
    std::wstring  text;
    vector<Byte>  values;
    int           overflow = 0;
    int           written  = 0;



    if (GetSelectedEdit() >= 0 && m_session.model.GetFirstByte() >= 0 && m_session.clipboard.GetText (m_host.GetOwnerHwnd(), text))
    {
        written            = m_session.edits.Paste (GetSelectedEdit(), m_session.model.GetFirstByte(), text, m_session.model.IsTextColumn(), overflow);
        m_session.editNote = (written == 0)  ? (m_session.model.IsTextColumn() ? std::wstring (L"Nothing to paste: the clipboard has no printable text")
                                                                               : InspectorClipboard::GetHexPasteNote (InspectorClipboard::ParseHex (text, values)))
                           : (overflow > 0)  ? InspectorText::FormatCount (overflow, L"byte", L"bytes") + L" did not fit before the end of the sector"
                                             : std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::FollowEdits
//
//  After re-analysis, each pending edit moves with its field; the window
//  lists the edits whose sectors can no longer be edited (FR-108).
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::FollowEdits()
{
    vector<PendingEdit>  discarded;
    std::wstring         list;



    m_session.edits.Follow (m_session.analysis, discarded);

    for (const PendingEdit & edit : discarded)
    {
        list += (list.empty() ? L"" : L", ") + InspectorFormat::FormatSector (edit.sectorNumber) + L" on track " + InspectorFormat::FormatQuarterTrack (edit.quarterTrack);
    }

    if (!list.empty())
    {
        m_session.editNote = L"Edits discarded, the sectors can no longer be edited: " + list;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::UpdateEditBar
//
//  While editing, moving to another editable sector begins an edit there,
//  so typing always goes to the selected sector.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::UpdateEditBar()
{
    const TrackAnalysis *  track    = nullptr;
    int                    sector   = -1;
    int                    side     = 0;
    bool                   hasTrack = GetEditTarget (track, sector, side);
    std::wstring           reason;
    int                    edit     = -1;
    EditBarState           state;
    EditController::Counts counts;
    const DiskCopy *       copyB    = m_session.comparison.GetB().copy.get();
    bool                   isDskB   = copyB != nullptr && (copyB->format == DiskFormat::Dsk || copyB->format == DiskFormat::Do || copyB->format == DiskFormat::Po);
    int                    width    = 0;
    int                    height   = 0;



    DropStaleBEdits();

    reason = !hasTrack ? std::wstring (L"No track") : (side == 1) ? GetBEditBlock() : std::wstring();
    reason = (!reason.empty() || !hasTrack) ? reason : (side == 1 && sector < 0) ? std::wstring (L"The second image has no sector to pair with this one")
                                                                                  : EditController::GetUneditableReason (*track, sector);
    edit   = GetSelectedEdit();
    counts = m_session.edits.GetCounts();

    if (m_sectorsView.IsEditing() && edit < 0 && reason.empty())
    {
        edit = BeginTarget (reason);
    }

    //  The Tracks tab counts the edited sectors of each track.
    m_session.isTablesDirty = m_session.isTablesDirty || (static_cast<int64_t> (counts.bytes) << 16 | counts.sectors) != m_editSig;
    m_editSig               = static_cast<int64_t> (counts.bytes) << 16 | counts.sectors;

    state.canEdit          = m_session.context.hasDisk && reason.empty();
    state.isEditing        = m_sectorsView.IsEditing();
    state.hasSectorEdit    = edit >= 0;
    state.isChecksumLocked = (side == 1) ? isDskB : m_session.IsSectorImage();
    state.isChecksumOn     = state.isChecksumLocked || edit < 0 || m_session.edits.GetEdit (edit).checksumMode == ChecksumMode::Recompute;
    state.bytes            = counts.bytes;
    state.sectors          = counts.sectors;
    state.applyBlocked     = GetApplyBlock (m_session);
    state.canApply         = state.applyBlocked.empty() && m_applyRequest == 0;
    state.isWaiting        = m_isApplyWaiting;
    state.isMachinePaused  = m_isApplyPaused;
    state.canUndoApplied   = m_historyAt > 0 && m_applyRequest == 0 && state.applyBlocked.empty() && m_session.edits.IsEmpty() &&
                             (IsHistoryCurrent (m_history[m_historyAt - 1].countsAfter) || m_history[m_historyAt - 1].edits.IsCurrentIn (m_session.analysis));
    state.canRedoApplied   = m_historyAt < static_cast<int> (m_history.size()) && m_applyRequest == 0 && state.applyBlocked.empty() &&
                             m_session.edits.HasSameChanges (m_history[m_historyAt].edits) &&
                             (IsHistoryCurrent (m_history[m_historyAt].countsUndone) || m_history[m_historyAt].edits.MakeOverCurrent (true).IsCurrentIn (m_session.analysis));
    state.note             = !m_session.editNote.empty() ? m_session.editNote : (m_session.model.GetSector() != nullptr && !reason.empty()) ? L"Cannot edit: " + reason : std::wstring();

    //  A change of the bar's height moves the bytes under it. The height, not
    //  the row count, is compared: an empty bar has one row and no height.
    width  = m_editBar->GetBounds().right - m_editBar->GetBounds().left;
    height = m_editBar->GetHeightPx (width, m_session.scaler);
    m_editBar->SetState (state);
    m_state = state;

    //  As in block mode: "Edit", or "Done" in the accent while editing.
    m_editButton->SetLabel   (state.isEditing ? L"Done" : L"Edit");
    m_editButton->SetVariant (state.isEditing ? DxuiButton::Variant::Primary : DxuiButton::Variant::Default);
    m_editButton->SetEnabled (state.canEdit || state.isEditing);
    m_session.isLayoutStale = m_session.isLayoutStale || (width > 0 && m_editBar->GetHeightPx (width, m_session.scaler) != height);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetApplyBlock
//
//  Why "Save", "Undo save" and "Redo save" cannot change
//  the disk now, with every write-protect cause listed, or empty
//  (FR-102). In a host with no drives, the disk is the window's file, and
//  the causes are the file's as it was read.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditingPart::GetApplyBlock (const InspectorSession & session)
{
    const DriveHeadState  * state  = session.headState.get();
    const DiskCopy        * copy   = session.analysis.copy.get();
    bool                    isFile = session.host->GetDriveCount() == 0;
    std::wstring            causes;



    if (!session.IsWindowDiskA() || (isFile && copy == nullptr) || (!isFile && (state == nullptr || !state->hasDisk)))
    {
        causes = isFile ? L"Only the image this window opened can be changed" : L"Only the disk in the drive can be changed";
    }
    else
    {
        causes = isFile ? InspectorText::FormatWriteProtect (copy->writeProtect, L"Casso Explorer") : InspectorText::FormatWriteProtect (state->writeProtect);
        causes = causes.empty() ? causes : L"Write-protected: " + causes;
    }

    return causes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::IsHistoryCurrent
//
//  An applied edit can be undone, or an undone one redone, only while the
//  guest has not written to any record it changed since (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::IsHistoryCurrent (const vector<RecordWriteCount> & counts) const
{
    return std::all_of (counts.begin(), counts.end(), [this] (const RecordWriteCount & count)
                        { return count.slot >= 0 && count.slot < static_cast<int> (m_session.writeCounts.size()) && m_session.writeCounts[count.slot] == count.guestWriteCount; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::ApplyEdits
//
//  "Save..." writes at once, unless a warning applies to an edit: then the
//  user confirms the warnings first (FR-100). The edits go to the
//  emulation thread, which saves them at once when the drive is not
//  turning, or waits for it to stop (FR-103).
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::ApplyEdits()
{
    InspectorRequest       request;
    vector<std::wstring>   lines;
    std::wstring           records;
    std::wstring           warnings;
    EditController::Counts counts  = m_session.edits.GetCounts (0);
    EditController::Counts countsB = m_session.edits.GetCounts (1);
    vector<SectorWrite>    writesB = m_session.edits.BuildWrites (m_session.comparison.GetB(), 1);
    std::wstring           lineB   = L"Save " + InspectorText::FormatCount (countsB.bytes, L"byte", L"bytes") + L" in " + InspectorText::FormatCount (countsB.sectors, L"sector", L"sectors")
                                   + L" to " + m_session.context.GetSideName (1) + L"?";
    bool                   isWoz1  = m_session.analysis.copy != nullptr && m_session.analysis.copy->format == DiskFormat::Woz && m_session.analysis.copy->woz.info.version < 2;
    bool                   isNib   = m_session.analysis.copy != nullptr && m_session.analysis.copy->format == DiskFormat::Nib;
    HRESULT                hr      = S_OK;



    request.kind      = InspectorRequestKind::Apply;
    request.drive     = m_session.drive;
    request.mediaId   = m_session.analysis.mediaId;
    request.applyKind = ApplyKind::Apply;
    request.applyMode = ApplyMode::WaitForStop;
    request.writes    = m_session.edits.BuildWrites (m_session.analysis);

    //  With changes only in B, B is saved to its file at once.
    if (request.writes.empty())
    {
        if (!writesB.empty())
        {
            (void) SaveBEdits();
        }

        BAIL_OUT_IF (true, S_OK);
    }

    lines.push_back (L"Save " + EditBar::FormatCount (counts.bytes, counts.sectors).substr (0, EditBar::FormatCount (counts.bytes, counts.sectors).size() - std::wstring (L" changed").size())
                     + L" to " + m_session.GetFileName() + L"?");

    for (const PendingEdit & edit : m_session.edits.GetEdits())
    {
        const TrackAnalysis *  track = (edit.slot >= 0 && edit.slot < static_cast<int> (m_session.analysis.tracks.size())) ? m_session.analysis.tracks[edit.slot].get() : nullptr;
        vector<int>            readers;

        if (edit.CountChanged() == 0 || track == nullptr)
        {
            continue;
        }

        for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            if (m_session.analysis.entries[qt].slot == edit.slot)
            {
                readers.push_back (qt);
            }
        }

        warnings  = edit.conflicts.any() ? L": the guest also wrote " + InspectorText::FormatCount (static_cast<int> (edit.conflicts.count()), L"byte you changed", L"bytes you changed") + L", and your values are saved"
                  : edit.isOutOfDate     ? L": changed since editing began, so only the bytes you changed are written"
                                         : L"";
        warnings += m_session.analysis.entries[readers.empty() ? 0 : readers.front()].isFromFluxMap ? L"; only the flux record changes, and the bit record is left as it was" : L"";
        warnings += (isNib && m_session.writtenSlots.contains (edit.slot)) ? L"; the guest also wrote this track, so Casso rebuilds it when saving" : L"";

        if (!warnings.empty())
        {
            records = L"Sector " + InspectorFormat::FormatSector (edit.sectorNumber) + L", read at track " + InspectorFormat::FormatQuarterTrackList (readers);
            lines.push_back (records + warnings);
        }

        request.expectedCounts.push_back ({ edit.slot, track->guestWriteCount });

        for (const LocatedField & field : track->fields)
        {
            if (field.startCell == edit.fieldStartCell && track->framed.cellCount > 0)
            {
                request.spans.push_back ({ edit.slot, static_cast<double> (field.startCell) / track->framed.cellCount, static_cast<double> (field.endCell) / track->framed.cellCount });
            }
        }
    }

    if (isWoz1)
    {
        lines.push_back (L"This disk is a WOZ 1 image. Casso saves it as WOZ 2.");
    }

    if (!writesB.empty())
    {
        lines.push_back (lineB);
    }

    if (lines.size() == 1 || m_host.Confirm (L"Save", lines, L"Save"))
    {
        if (!writesB.empty())
        {
            (void) SaveBEdits();
        }

        PostApply (request);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::SaveBEdits
//
//  B's pending edits written to B's file, whole or not at all through
//  DurableCommit; then B is read again and its edits are dropped.
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::SaveBEdits()
{
    HRESULT               hr      = S_OK;
    Win32DiskFileIo       fileIo;
    CommitPlan::Progress  progress;
    std::string           path    = TextEncoding::WideToUtf8 (m_session.comparison.GetSourceB().path);
    std::wstring          name    = m_session.context.GetSideName (1);
    std::wstring          reason  = GetBEditBlock();
    vector<SectorWrite>   writes  = m_session.edits.BuildWrites (m_session.comparison.GetB(), 1);
    vector<Byte>          bytes;
    vector<Byte>          edited;
    bool                  canSave = reason.empty();



    CBR (canSave);

    reason = L"the file could not be read";
    hr     = DiskFileCommit::ReadFileBytes (path, bytes);
    CHR (hr);

    hr = InspectorImageLoader::ApplyWrites (bytes, path, writes, edited, reason);
    CHR (hr);

    reason = L"the file could not be written";
    hr     = DurableCommit::Commit (fileIo, path, edited, GetTickCount64(), CommitMode::Replace, progress);
    CHR (hr);

    m_session.edits.RemoveSide (1);
    m_session.comparison.Reload (ComparisonSession::kSideB, *m_session.host);
    m_session.editNote = L"Saved the changes to " + name;

Error:
    if (FAILED (hr))
    {
        m_session.editNote = L"Not saved to " + name + L": " + reason;
    }

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::PostApply
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::PostApply (const InspectorRequest & request)
{
    m_lastApply        = request;
    m_applyRequest     = m_session.host->PostInspectorRequest (request);
    m_isApplyWaiting   = false;
    m_session.editNote = L"Saving...";
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::PauseApply
//
//  The waiting save again, made at the next safe point with the machine
//  running on, or for a paused machine with it paused again afterward.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::PauseApply()
{
    InspectorRequest  cancel;
    InspectorRequest  request = m_lastApply;



    cancel.kind     = InspectorRequestKind::CancelApply;
    cancel.drive    = m_session.drive;
    request.applyMode = m_isApplyPaused ? ApplyMode::RunToSafePointStayPaused : ApplyMode::PauseAtSafePoint;

    m_applyRequest = 0;
    (void) m_session.host->PostInspectorRequest (cancel);
    PostApply (request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::CancelApply
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::CancelApply()
{
    InspectorRequest  cancel;



    cancel.kind  = InspectorRequestKind::CancelApply;
    cancel.drive = m_session.drive;
    (void) m_session.host->PostInspectorRequest (cancel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::Undo
//
//  Undoes the last unsaved edit, in or out of editing, as block mode does.
//  With no unsaved edit to undo, undoes the last save.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::Undo()
{
    int  edit   = -1;
    int  offset = 0;



    m_session.editNote.clear();

    if (!m_session.edits.CanUndo())
    {
        if (m_state.canUndoApplied && !m_history[m_historyAt - 1].edits.GetEdits().empty())
        {
            ShowEdit (m_history[m_historyAt - 1].edits.GetEdits().front(), 0);
        }

        UndoApplied();
        return;
    }

    if (m_session.edits.GetUndoAt (edit, offset))
    {
        ShowEdit (m_session.edits.GetEdit (edit), offset);
    }

    (void) m_session.edits.Undo();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::Redo
//
//  Redoes the last undone edit, in or out of editing. With no edit to redo,
//  redoes the last undone save.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::Redo()
{
    int  edit   = -1;
    int  offset = 0;



    m_session.editNote.clear();

    if (!m_session.edits.CanRedo())
    {
        if (m_state.canRedoApplied && !m_history[m_historyAt].edits.GetEdits().empty())
        {
            ShowEdit (m_history[m_historyAt].edits.GetEdits().front(), 0);
        }

        RedoApplied();
        return;
    }

    if (m_session.edits.GetRedoAt (edit, offset))
    {
        ShowEdit (m_session.edits.GetEdit (edit), offset);
    }

    (void) m_session.edits.Redo();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::ShowEdit
//
//  Selects an edit's sector on the Sector data tab with the caret on a
//  byte, so an undo or a redo is seen as it happens.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::ShowEdit (const PendingEdit & edit, int offset)
{
    const TrackAnalysis *  track = nullptr;



    if (edit.quarterTrack != m_session.model.GetQuarterTrack())
    {
        m_session.model.SelectQuarterTrack (edit.quarterTrack);
    }

    track = m_session.model.GetTrack();

    //  B's edit is on B's track: show the track's Sector data tab, and leave
    //  B's view where it is.
    if (edit.side == 1)
    {
        m_host.ShowTrackTab (kTabSectorData);
        track = nullptr;
    }

    for (int k = 0; track != nullptr && k < static_cast<int> (track->sectors.size()); k++)
    {
        if (track->sectors[k].dataField >= 0 && track->fields[track->sectors[k].dataField].startCell == edit.fieldStartCell)
        {
            if (k != m_session.model.GetSectorIndex())
            {
                m_session.model.SelectSector (edit.quarterTrack, k);
            }

            m_host.ShowTrackTab (kTabSectorData);
            m_sectorsView.GoToByte (offset);
            m_host.OnSelection();
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::UndoApplied
//
//  Undo puts the tracks back as they were before the last save, and its
//  edits become unsaved changes again; redo saves those edits again
//  (FR-107). After the guest wrote one of the tracks, only the bytes the
//  save changed are written back, over the sectors' current bytes.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::UndoApplied()
{
    InspectorRequest  request;
    HRESULT           hr = S_OK;



    BAIL_OUT_IF (!m_state.canUndoApplied, S_OK);

    request.kind           = InspectorRequestKind::Apply;
    request.drive          = m_session.drive;
    request.mediaId        = m_session.analysis.mediaId;
    request.applyKind = ApplyKind::Undo;

    if (IsHistoryCurrent (m_history[m_historyAt - 1].countsAfter))
    {
        request.restore        = m_history[m_historyAt - 1].before;
        request.expectedCounts = m_history[m_historyAt - 1].countsAfter;
    }
    else
    {
        request.writes         = m_history[m_historyAt - 1].edits.MakeOverCurrent (true).BuildWrites (m_session.analysis);
        request.expectedCounts = GetCurrentCounts (request.writes);
    }

    PostApply (request);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::RedoApplied
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::RedoApplied()
{
    InspectorRequest  request;
    HRESULT           hr = S_OK;



    BAIL_OUT_IF (!m_state.canRedoApplied, S_OK);

    request.kind           = InspectorRequestKind::Apply;
    request.drive          = m_session.drive;
    request.mediaId        = m_session.analysis.mediaId;
    request.applyKind = ApplyKind::Redo;

    if (IsHistoryCurrent (m_history[m_historyAt].countsUndone))
    {
        request.restore        = m_history[m_historyAt].after;
        request.expectedCounts = m_history[m_historyAt].countsUndone;
    }
    else
    {
        request.writes         = m_history[m_historyAt].edits.MakeOverCurrent (false).BuildWrites (m_session.analysis);
        request.expectedCounts = GetCurrentCounts (request.writes);
    }

    PostApply (request);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::TakeApplyReply
//
//  The window marks an edit as applied only once the file has it (FR-100);
//  the changed tracks then go into the window's copy, so only they are
//  analyzed again. A new apply clears the redo history (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::TakeApplyReply (const InspectorReply & reply)
{
    InspectorReply  tracks;
    bool            isApplied = reply.applyOutcome == ApplyOutcome::Applied || reply.applyOutcome == ApplyOutcome::AppliedToPreservedCopy;



    m_isApplyWaiting = reply.applyOutcome == ApplyOutcome::Waiting;
    m_isApplyPaused  = reply.isMachinePaused;

    switch (reply.applyOutcome)
    {
        case ApplyOutcome::Waiting:   m_session.editNote = m_isApplyPaused ? L"The machine is paused with the head over an edited field" : L"Waiting for the drive to stop"; break;
        case ApplyOutcome::Cancelled: m_session.editNote = L"Save canceled. The edits are pending.";                                                               break;
        case ApplyOutcome::WaitEnded: m_session.editNote = L"The guest wrote to a track these edits change. Check the edits and save again.";                           break;
        case ApplyOutcome::NotApplied: m_session.editNote = reply.reason;                                                                                               break;
        default:                       break;
    }

    if (!m_isApplyWaiting)
    {
        m_applyRequest = 0;
    }

    //  The action chosen to run after the save is due once the save
    //  succeeds, and is dropped when the save fails or is canceled.
    if (m_afterSave != nullptr && !m_isApplyWaiting)
    {
        m_dueAction = (isApplied && reply.applyKind == ApplyKind::Apply) ? std::move (m_afterSave) : nullptr;
        m_afterSave = nullptr;
    }

    if (isApplied)
    {
        if (reply.applyKind == ApplyKind::Apply)
        {
            m_history.resize (m_historyAt);
            m_history.push_back ({ reply.before, reply.after, reply.countsAfter, {}, reply.savedPath, m_session.edits });
            m_historyAt        = static_cast<int> (m_history.size());
            m_session.edits    = EditController();
            m_sectorsView.SetEditing (false);
        }
        else if (reply.applyKind == ApplyKind::Undo)
        {
            m_historyAt--;
            m_history[m_historyAt].countsUndone = reply.countsAfter;
            m_session.edits                     = m_history[m_historyAt].edits;

            for (const RecordWriteCount & count : reply.countsAfter)
            {
                m_session.edits.SetWriteCount (count.slot, count.guestWriteCount);
            }
        }
        else
        {
            m_history[m_historyAt].countsAfter = reply.countsAfter;
            m_historyAt++;
            m_session.edits = EditController();
            m_sectorsView.SetEditing (false);
        }

        for (const std::shared_ptr<const TrackCopy> & copy : reply.after)
        {
            m_session.appliedSlots.insert (copy->slot);
        }

        m_session.isTablesDirty = true;

        m_session.editNote = (reply.applyOutcome == ApplyOutcome::AppliedToPreservedCopy)
                         ? L"Saved to a preserved copy, since the file changed outside Casso: " + std::filesystem::path (TextEncoding::Utf8ToWide (reply.savedPath)).filename().wstring()
                         : std::wstring ((reply.applyKind == ApplyKind::Undo) ? L"Undid the save to " : L"Saved to ")
                           + std::filesystem::path (TextEncoding::Utf8ToWide (reply.savedPath)).filename().wstring();

        tracks.mediaId = reply.mediaId;
        tracks.tracks  = reply.after;
        m_host.TakeTracks (tracks);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetBSectorBytes
//
//  B's bytes for the sector paired with A's selected one, paired as the
//  Sectors tab pairs them, while A is the window's disk as it is now
//  (FR-123).
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::GetBSectorBytes (PendingEdit::Sector & outBytes) const
{
    const TrackAnalysis *  trackA  = m_session.model.GetTrack();
    const TrackAnalysis *  trackB  = m_session.context.trackB;
    std::span<const Byte>  bytes;
    bool                   isLoose = false;
    bool                   isFound = false;



    if (m_session.comparison.IsComparing() && m_session.sourceA == ComparisonSource { ComparisonSourceKind::DriveNow, m_session.drive } && trackA != nullptr && trackB != nullptr &&
        m_session.model.GetSectorIndex() >= 0)
    {
        bytes   = m_sectorsView.GetPairedBytes (*trackA, m_session.model.GetSectorIndex(), isLoose);
        isFound = !bytes.empty();

        if (isFound)
        {
            outBytes.fill (0);
            std::copy_n (bytes.begin(), std::min (bytes.size(), outBytes.size()), outBytes.begin());
        }
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::UseBBytes
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::UseBBytes()
{
    const TrackAnalysis *  track = m_session.model.GetTrack();
    PendingEdit::Sector    bytes = {};
    std::wstring           reason;
    int                    edit  = -1;



    if (track != nullptr && GetBSectorBytes (bytes))
    {
        edit               = m_session.edits.Begin (*track, m_session.model.GetQuarterTrack(), m_session.model.GetSectorIndex(), reason);
        m_session.editNote = reason;

        if (edit >= 0)
        {
            (void) m_session.edits.SetSector (edit, bytes);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetASectorBytes
//
//  The bytes of A's selected sector in the Sectors tab: its pending edit's
//  bytes, or its decoded bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::GetASectorBytes (PendingEdit::Sector & outBytes) const
{
    const TrackAnalysis *  track  = m_session.model.GetTrack();
    int                    sector = m_session.model.GetSectorIndex();
    int                    edit   = (track != nullptr) ? m_session.edits.Find (*track, sector) : -1;
    bool                   isData = track != nullptr && sector >= 0 && sector < static_cast<int> (track->sectors.size()) && track->sectors[sector].dataField >= 0;



    if (edit >= 0)
    {
        outBytes = m_session.edits.GetEdit (edit).bytes;
    }
    else if (isData)
    {
        outBytes = track->fields[track->sectors[sector].dataField].data.bytes;
    }

    return edit >= 0 || isData;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::UseABytes
//
//  "Copy sector from" A in B's view: B's paired sector takes A's bytes.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::UseABytes()
{
    const TrackAnalysis *  trackB  = m_session.context.trackB;
    int                    sectorB = m_sectorsView.GetSelectedSectorB();
    PendingEdit::Sector    bytes   = {};
    std::wstring           reason  = GetBEditBlock();
    int                    edit    = -1;



    if (reason.empty() && trackB != nullptr && sectorB >= 0 && GetASectorBytes (bytes))
    {
        edit = m_session.edits.Begin (*trackB, m_session.model.GetQuarterTrack(), sectorB, reason, 1);

        if (edit >= 0)
        {
            m_session.editsBMedia = m_session.comparison.GetB().mediaId;
            (void) m_session.edits.SetSector (edit, bytes);
        }
    }

    m_session.editNote = reason;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::CanCopySector
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::CanCopySector (int toSide) const
{
    const TrackAnalysis *  track   = m_session.model.GetTrack();
    const TrackAnalysis *  trackB  = m_session.context.trackB;
    int                    sectorB = m_sectorsView.GetSelectedSectorB();
    PendingEdit::Sector    bytes   = {};



    if (toSide == 0)
    {
        return track != nullptr && EditController::GetUneditableReason (*track, m_session.model.GetSectorIndex()).empty() && GetBSectorBytes (bytes);
    }

    return trackB != nullptr && sectorB >= 0 && GetBEditBlock().empty() && EditController::GetUneditableReason (*trackB, sectorB).empty() && GetASectorBytes (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::SaveEditedCopy
//
//  "Save as..." (FR-101): a copy of each image with pending edits. For A,
//  the emulation thread builds the copy's bytes from the drive's disk and
//  the pending edits, and the reply opens the Save dialog. B's copy is
//  saved first.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::SaveEditedCopy()
{
    InspectorRequest  request;



    if (m_session.edits.GetCounts (1).sectors > 0)
    {
        SaveBEditedCopy();
    }

    if (m_session.edits.GetCounts (0).sectors > 0)
    {
        request.kind       = InspectorRequestKind::SaveCopy;
        request.drive      = m_session.drive;
        request.mediaId    = m_session.analysis.mediaId;
        request.writes     = m_session.edits.BuildWrites (m_session.analysis);
        m_saveCopyRequest  = m_session.host->PostInspectorRequest (request);
        m_session.editNote = L"Preparing the copy...";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::SaveBEditedCopy
//
//  A copy of B's file with B's pending edits, saved to a new file the user
//  chooses, never B's file or A's, whole or not at all through
//  DurableCommit. The edits stay pending and B's file is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::SaveBEditedCopy()
{
    HRESULT                hr        = S_OK;
    Win32HostDialogs       dialogs;
    Win32DiskFileIo        fileIo;
    FileDialogSpec         spec;
    CommitPlan::Progress   progress;
    std::filesystem::path  image     = m_session.comparison.GetSourceB().path;
    std::filesystem::path  imageA    = std::filesystem::path (TextEncoding::Utf8ToWide (m_session.analysis.copy != nullptr ? m_session.analysis.copy->fileName : std::string()));
    std::filesystem::path  target;
    std::wstring           name      = m_session.context.GetSideName (1);
    std::wstring           extension = image.has_extension() ? image.extension().wstring().substr (1) : std::wstring (L"dsk");
    std::wstring           reason    = L"the second image is a drive's disk";
    vector<Byte>           bytes;
    vector<Byte>           edited;
    bool                   isFile    = m_session.comparison.GetSourceB().kind == ComparisonSourceKind::ImageFile;
    bool                   isPicked  = false;
    bool                   isTaken   = true;



    CBR (isFile);

    reason = L"the file could not be read";
    hr     = DiskFileCommit::ReadFileBytes (TextEncoding::WideToUtf8 (image.wstring()), bytes);
    CHR (hr);

    hr = InspectorImageLoader::ApplyWrites (bytes, TextEncoding::WideToUtf8 (image.wstring()), m_session.edits.BuildWrites (m_session.comparison.GetB(), 1), edited, reason);
    CHR (hr);

    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.content          = FileDialogContent::Images;
    spec.defaultExtension = extension;
    spec.defaultFileName  = image.stem().wstring() + L" edited." + extension;
    spec.initialFolder    = image.parent_path();
    spec.title            = L"Save a copy of " + name;

    //  Open the Save dialog again until the file is neither image's file.
    while (isTaken)
    {
        hr = dialogs.PickFileToSave (m_host.GetOwnerHwnd(), spec, target, isPicked);
        CHR (hr);
        BAIL_OUT_IF (!isPicked, S_OK);

        isTaken = IsSameFile (target, image) || IsSameFile (target, imageA);

        if (isTaken)
        {
            (void) DxuiMessageBox (m_host.GetOwnerHwnd(), m_session.theme, L"That file is one of the images being compared. Choose another file for the copy.", L"Save as", MB_OK | MB_ICONWARNING);
        }
    }

    reason = L"the copy could not be written to " + target.filename().wstring();
    hr     = DurableCommit::Commit (fileIo, TextEncoding::WideToUtf8 (target.wstring()), edited, GetTickCount64(),
                                    std::filesystem::exists (target) ? CommitMode::Replace : CommitMode::CreateNew, progress);
    CHR (hr);

    m_session.editNote = L"Saved a copy of " + name + L" with its pending edits to " + target.filename().wstring();

Error:
    if (FAILED (hr) || !isFile)
    {
        m_session.editNote = L"Not saved: " + reason;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::TakeSaveCopyReply
//
//  The copy goes to a new file the user chooses, never the disk's file
//  or one in a drive, whole or not at all through DurableCommit. The
//  pending edits stay pending, and the drive's disk and its file are left
//  as they were.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::TakeSaveCopyReply (const InspectorReply & reply)
{
    HRESULT                hr        = S_OK;
    Win32HostDialogs       dialogs;
    Win32DiskFileIo        fileIo;
    FileDialogSpec         spec;
    CommitPlan::Progress   progress;
    std::filesystem::path  image     = std::filesystem::path (TextEncoding::Utf8ToWide (m_session.analysis.copy != nullptr ? m_session.analysis.copy->fileName : std::string()));
    std::filesystem::path  target;
    std::wstring           extension = image.has_extension() ? image.extension().wstring().substr (1) : std::wstring (L"dsk");
    bool                   isPicked  = false;
    bool                   isTaken   = true;
    bool                   isDamaged = m_session.analysis.copy != nullptr && (!m_session.analysis.copy->damagedTracks.empty() || !m_session.analysis.copy->damagedQuarterTracks.empty());



    m_saveCopyRequest = 0;
    m_session.editNote.clear();

    if (reply.status != InspectorReplyStatus::Done || reply.sourceBytes == nullptr)
    {
        m_session.editNote = L"Not saved: " + (reply.reason.empty() ? std::wstring (L"the disk in the drive changed") : reply.reason);
        BAIL_OUT_IF (true, S_OK);
    }

    if (isDamaged)
    {
        BAIL_OUT_IF (!m_host.Confirm (L"Save as", { L"This image is damaged. The copy gets a new checksum, and each damaged track record is saved as Casso read it.",
                                                         L"Save the copy?" }, L"Save"), S_OK);
    }

    extension             = (m_session.analysis.copy != nullptr && m_session.analysis.copy->format == DiskFormat::Woz) ? std::wstring (L"woz") : extension;
    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.content = FileDialogContent::Images;
    spec.defaultExtension = extension;
    spec.defaultFileName  = image.stem().wstring() + L" edited." + extension;
    spec.initialFolder    = image.parent_path();
    spec.title            = m_session.comparison.IsComparing() ? L"Save a copy of " + m_session.context.GetSideName (0) : std::wstring();

    //  Open the Save dialog again until the file is neither the disk's file
    //  nor one in a drive.
    while (isTaken)
    {
        hr = dialogs.PickFileToSave (m_host.GetOwnerHwnd(), spec, target, isPicked);
        CHR (hr);
        BAIL_OUT_IF (!isPicked, S_OK);

        isTaken = IsSameFile (target, image);

        for (const std::string & mounted : reply.mountedPaths)
        {
            isTaken = isTaken || IsSameFile (target, std::filesystem::path (TextEncoding::Utf8ToWide (mounted)));
        }

        if (isTaken)
        {
            (void) DxuiMessageBox (m_host.GetOwnerHwnd(), m_session.theme, L"That file is a disk in a drive. Choose another file for the copy.", L"Save as", MB_OK | MB_ICONWARNING);
        }
    }

    hr = DurableCommit::Commit (fileIo, TextEncoding::WideToUtf8 (target.wstring()), *reply.sourceBytes, GetTickCount64(),
                                std::filesystem::exists (target) ? CommitMode::Replace : CommitMode::CreateNew, progress);

    m_session.editNote = SUCCEEDED (hr) ? L"Saved a copy with the pending edits to " + target.filename().wstring()
                                : L"Not saved: the copy could not be written to " + target.filename().wstring();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::IsSameFile
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::IsSameFile (const std::filesystem::path & a, const std::filesystem::path & b)
{
    std::error_code  error;
    bool             isSame = std::filesystem::equivalent (a, b, error);



    return (!error && isSame) || _wcsicmp (std::filesystem::absolute (a, error).wstring().c_str(), std::filesystem::absolute (b, error).wstring().c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::ConfirmDiscardEdits
//
//  With unsaved changes, the user picks Save, Discard or Cancel. Save is
//  offered when there is an action to run after it and the disk can be
//  saved to. The save can wait for the drive to stop, so `afterSave` runs
//  once the save succeeds, and not at all when it fails or is canceled.
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::ConfirmDiscardEdits (const std::wstring & action, std::function<void()> afterSave)
{
    static constexpr int  kCancel  = 0;
    static constexpr int  kSave    = 1;
    static constexpr int  kDiscard = 2;



    EditController::Counts         counts    = m_session.edits.GetCounts();
    bool                           canSave   = afterSave != nullptr && (m_state.canApply || m_applyRequest != 0);
    bool                           isGoingOn = counts.sectors == 0;
    int                            choice    = kCancel;
    vector<std::wstring>           lines;
    vector<MessageDialog::Button>  buttons;



    if (!isGoingOn)
    {
        lines.push_back (InspectorText::FormatUnsavedChanges (counts.sectors, L"sector", L"sectors", m_session.GetFileName()));

        if (canSave)
        {
            buttons.push_back ({ L"Save", kSave, true, false });
        }
        else
        {
            lines.push_back (action + L" discards them.");
        }

        buttons.push_back ({ L"Discard", kDiscard, !canSave, false });
        buttons.push_back ({ L"Cancel", kCancel, false, true });

        choice = InspectorDialogs::ShowChoice (m_host.GetOwnerHwnd(), m_session.theme, L"Unsaved changes", lines, buttons);

        if (choice == kSave)
        {
            if (m_applyRequest == 0)
            {
                ApplyEdits();
            }

            m_afterSave = (m_applyRequest != 0) ? std::move (afterSave) : nullptr;
        }

        if (choice == kDiscard)
        {
            m_session.edits = EditController();
            m_sectorsView.SetEditing (false);
        }

        isGoingOn = choice == kDiscard;
    }

    return isGoingOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::TakeDueAction
//
//  The action chosen to run after a save, once that save succeeds; empty
//  otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::function<void()> EditingPart::TakeDueAction()
{
    return std::exchange (m_dueAction, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetCurrentCounts
//
//  The guest write count the window has for each record the writes change,
//  so a guest write before they are made stops them.
//
////////////////////////////////////////////////////////////////////////////////

vector<RecordWriteCount> EditingPart::GetCurrentCounts (const vector<SectorWrite> & writes) const
{
    vector<RecordWriteCount>  counts;



    for (const SectorWrite & write : writes)
    {
        if (write.slot >= 0 && write.slot < static_cast<int> (m_session.writeCounts.size()))
        {
            counts.push_back ({ write.slot, m_session.writeCounts[write.slot] });
        }
    }

    return counts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::IsLiveEditConfirmed
//
//  A disk in a drive of a running machine is edited only after the user
//  accepts the risks, once for each window.
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::IsLiveEditConfirmed()
{
    if (!m_isLiveEditConfirmed && m_session.host->GetDriveCount() > 0)
    {
        m_isLiveEditConfirmed = m_host.Confirm (L"Edit a disk in use",
                                                { L"This disk is in a drive of a running machine. The program running there can write to the disk at any time, "
                                                  L"over your changes, and may not expect the disk to change under it. "
                                                  L"A wrong byte in a catalog, directory or bitmap can damage the disk's files.",
                                                  L"Edit it anyway?" }, L"Edit");
    }

    return m_isLiveEditConfirmed || m_session.host->GetDriveCount() == 0;
}
