#include "Pch.h"

#include "Ui/DiskInspector/Components/EditingPart.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32HostDialogs.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/SectorByteView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::EditingPart
//
////////////////////////////////////////////////////////////////////////////////

EditingPart::EditingPart (InspectorSession & session, IInspectorComponentHost & host, SectorByteView & byteView) :
    m_session  (session),
    m_host     (host),
    m_byteView (byteView)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::Create
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::Create()
{
    m_editBar = m_host.GetControlParent().CreateChild<EditBar>();

    m_editBar->SetOnEdit          ([this] () { ToggleEditing(); });
    m_editBar->SetOnChecksum      ([this] (bool isOn) { m_session.edits.SetChecksumMode (GetSelectedEdit(), isOn ? ChecksumMode::Recompute : ChecksumMode::KeepStored); });
    m_editBar->SetOnDiscardSector ([this] () { m_session.edits.DiscardSector (GetSelectedEdit()); });
    m_editBar->SetOnDiscard       ([this] () { m_session.edits.DiscardAll(); });
    m_editBar->SetOnApply         ([this] () { ApplyEdits(); });
    m_editBar->SetOnSaveCopy      ([this] () { SaveEditedCopy(); });
    m_editBar->SetOnUseB          ([this] () { UseBBytes(); });
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
    m_editBar->SetVisible (isShown);
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
    return m_session.keyTarget == KeyTarget::SectorData && m_byteView.IsEditing() && HandleEditKey (ev);
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
//  track is analyzed again (FR-108). A new disk clears the apply history.
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
        m_session.editNote = L"Pending edits were discarded: the disk in the drive changed";
    }

    if (change.hasDisk && isNewDisk && !isReload)
    {
        m_session.edits = EditController();
        m_byteView.SetEditing (false);
    }

    if (change.hasDisk && isNewDisk)
    {
        m_history.clear();
        m_historyAt = 0;
        m_session.appliedSlots.clear();
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
        m_session.editNote = L"Pending edits were discarded: the disk was ejected";
        m_host.SetStatusNote (m_session.editNote);
    }

    m_session.edits = EditController();
    m_byteView.SetEditing (false);
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
    const TrackAnalysis *  track = m_session.model.GetTrack();



    return (track != nullptr) ? m_session.edits.Find (*track, m_session.model.GetSectorIndex()) : -1;
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



    if (m_byteView.IsEditing())
    {
        m_byteView.SetEditing (false);
    }
    else if (track != nullptr)
    {
        edit               = m_session.edits.Begin (*track, m_session.model.GetQuarterTrack(), m_session.model.GetSectorIndex(), reason);
        m_session.editNote = reason;

        if (edit >= 0)
        {
            m_byteView.SetEditing (true);
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

    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'Z')
    {
        (void) m_session.edits.Undo();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'Y')
    {
        (void) m_session.edits.Redo();
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
        isHandled = m_byteView.OnEditKey (ev);
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
    int           overflow = 0;
    int           written  = 0;



    if (GetSelectedEdit() >= 0 && m_session.model.GetFirstByte() >= 0 && m_session.clipboard.GetText (m_host.GetOwnerHwnd(), text))
    {
        written            = m_session.edits.Paste (GetSelectedEdit(), m_session.model.GetFirstByte(), text, m_session.model.IsTextColumn(), overflow);
        m_session.editNote = (written == 0)  ? std::wstring (m_session.model.IsTextColumn() ? L"Nothing to paste: the clipboard has no printable text"
                                                                            : L"Nothing to paste: the clipboard has no pairs of hex digits")
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
    const TrackAnalysis *  track  = m_session.model.GetTrack();
    std::wstring           reason = (track != nullptr) ? EditController::GetUneditableReason (*track, m_session.model.GetSectorIndex()) : std::wstring (L"No track");
    int                    edit   = GetSelectedEdit();
    EditBarState           state;
    EditController::Counts counts = m_session.edits.GetCounts();
    int                    width  = 0;
    int                    rows   = 0;
    PendingEdit::Sector    paired = {};



    if (m_byteView.IsEditing() && edit < 0 && reason.empty())
    {
        edit = m_session.edits.Begin (*track, m_session.model.GetQuarterTrack(), m_session.model.GetSectorIndex(), reason);
    }

    //  The Tracks tab counts the edited sectors of each track.
    m_session.isTablesDirty = m_session.isTablesDirty || (static_cast<int64_t> (counts.bytes) << 16 | counts.sectors) != m_editSig;
    m_editSig               = static_cast<int64_t> (counts.bytes) << 16 | counts.sectors;

    state.canEdit          = m_session.context.hasDisk && reason.empty();
    state.isEditing        = m_byteView.IsEditing();
    state.hasSectorEdit    = edit >= 0;
    state.isChecksumOn     = m_session.IsSectorImage() || edit < 0 || m_session.edits.GetEdit (edit).checksumMode == ChecksumMode::Recompute;
    state.isChecksumLocked = m_session.IsSectorImage();
    state.bytes            = counts.bytes;
    state.sectors          = counts.sectors;
    state.applyBlocked     = GetApplyBlock();
    state.canApply         = state.applyBlocked.empty() && m_applyRequest == 0;
    state.isWaiting        = m_isApplyWaiting;
    state.isMachinePaused  = m_isApplyPaused;
    state.canUseB          = reason.empty() && GetBSectorBytes (paired);
    state.canUndoApplied   = m_historyAt > 0 && m_applyRequest == 0 && state.applyBlocked.empty() && IsHistoryCurrent (m_history[m_historyAt - 1].countsAfter);
    state.canRedoApplied   = m_historyAt < static_cast<int> (m_history.size()) && m_applyRequest == 0 && state.applyBlocked.empty() &&
                             IsHistoryCurrent (m_history[m_historyAt].countsUndone);
    state.note             = !m_session.editNote.empty() ? m_session.editNote : (m_session.model.GetSector() != nullptr && !reason.empty()) ? L"Cannot edit: " + reason : std::wstring();

    //  A change of the bar's rows moves the bytes under it.
    width = m_editBar->GetBounds().right - m_editBar->GetBounds().left;
    rows  = m_editBar->GetRowCount (width, m_session.scaler);
    m_editBar->SetState (state);
    m_session.isLayoutStale = m_session.isLayoutStale || (width > 0 && m_editBar->GetRowCount (width, m_session.scaler) != rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::GetApplyBlock
//
//  Why "Apply", "Undo applied edit" and "Redo applied edit" cannot change
//  the disk now, with every write-protect cause listed, or empty
//  (FR-102).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EditingPart::GetApplyBlock() const
{
    const DriveHeadState *  state  = m_session.headState.get();
    std::wstring            causes;



    if (!m_session.IsWindowDiskA() || state == nullptr || !state->hasDisk)
    {
        causes = L"Only the disk in the drive can be changed";
    }
    else
    {
        causes = InspectorText::FormatWriteProtect (state->writeProtect);
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
//  Nothing changes until the user confirms the list of changes (FR-100).
//  The edits then go to the emulation thread, which applies them at once
//  when the drive is not turning, or waits for it to stop (FR-103).
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::ApplyEdits()
{
    InspectorRequest       request;
    vector<std::wstring>   lines;
    std::wstring           records;
    EditController::Counts counts  = m_session.edits.GetCounts();
    bool                   isWoz1  = m_session.analysis.copy != nullptr && m_session.analysis.copy->format == DiskFormat::Woz && m_session.analysis.copy->woz.info.version < 2;
    bool                   isNib   = m_session.analysis.copy != nullptr && m_session.analysis.copy->format == DiskFormat::Nib;
    HRESULT                hr      = S_OK;



    request.kind      = InspectorRequestKind::Apply;
    request.drive     = m_session.drive;
    request.mediaId   = m_session.analysis.mediaId;
    request.applyKind = ApplyKind::Apply;
    request.applyMode = ApplyMode::WaitForStop;
    request.writes    = m_session.edits.BuildWrites (m_session.analysis);

    BAIL_OUT_IF (request.writes.empty(), S_OK);

    lines.push_back (L"Apply " + EditBar::FormatCount (counts.bytes, counts.sectors).substr (0, EditBar::FormatCount (counts.bytes, counts.sectors).size() - std::wstring (L" changed").size())
                     + L" and save " + m_session.GetFileName() + L"?");

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

        records = L"Sector " + InspectorFormat::FormatSector (edit.sectorNumber) + L", read at track " + InspectorFormat::FormatQuarterTrackList (readers);
        records += edit.isOutOfDate ? L": changed since editing began, so only the bytes you changed are written" : L"";
        records += m_session.analysis.entries[readers.empty() ? 0 : readers.front()].isFromFluxMap ? L"; only the flux record changes, and the bit record is left as it was" : L"";
        records += (isNib && m_session.writtenSlots.contains (edit.slot)) ? L"; the guest also wrote this track, so Casso rebuilds it when saving" : L"";
        lines.push_back (records);

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

    if (!m_isFirstApplyDone)
    {
        lines.push_back (L"DOS or ProDOS may have one of these sectors in memory and write its own copy back over the edit.");
    }

    if (m_host.Confirm (L"Apply edits", lines, L"Apply"))
    {
        PostApply (request);
    }

Error:
    return;
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
    m_session.editNote = L"Applying...";
}





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart::PauseApply
//
//  The waiting apply again, made at the next safe point with the machine
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
//  EditingPart::UndoApplied
//
//  Undo and redo each restore tracks exactly as the history has them, once the
//  user confirms (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::UndoApplied()
{
    InspectorRequest  request;
    HRESULT           hr = S_OK;



    BAIL_OUT_IF (m_historyAt <= 0, S_OK);

    request.kind           = InspectorRequestKind::Apply;
    request.drive          = m_session.drive;
    request.mediaId        = m_session.analysis.mediaId;
    request.applyKind      = ApplyKind::Undo;
    request.restore        = m_history[m_historyAt - 1].before;
    request.expectedCounts = m_history[m_historyAt - 1].countsAfter;

    if (m_host.Confirm (L"Undo applied edit", { L"Put back the sectors the last apply changed, and save " + m_session.GetFileName() + L"?" }, L"Undo"))
    {
        PostApply (request);
    }

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



    BAIL_OUT_IF (m_historyAt >= static_cast<int> (m_history.size()), S_OK);

    request.kind           = InspectorRequestKind::Apply;
    request.drive          = m_session.drive;
    request.mediaId        = m_session.analysis.mediaId;
    request.applyKind      = ApplyKind::Redo;
    request.restore        = m_history[m_historyAt].after;
    request.expectedCounts = m_history[m_historyAt].countsUndone;

    if (m_host.Confirm (L"Redo applied edit", { L"Apply the undone edit again, and save " + m_session.GetFileName() + L"?" }, L"Redo"))
    {
        PostApply (request);
    }

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
        case ApplyOutcome::Cancelled: m_session.editNote = L"Apply canceled; the edits are still pending";                                                               break;
        case ApplyOutcome::WaitEnded: m_session.editNote = L"The guest wrote to a track these edits change. Check the edits and apply again.";                           break;
        case ApplyOutcome::NotApplied: m_session.editNote = reply.reason;                                                                                               break;
        default:                       break;
    }

    if (!m_isApplyWaiting)
    {
        m_applyRequest = 0;
    }

    if (isApplied)
    {
        if (reply.applyKind == ApplyKind::Apply)
        {
            m_history.resize (m_historyAt);
            m_history.push_back ({ reply.before, reply.after, reply.countsAfter, {}, reply.savedPath });
            m_historyAt        = static_cast<int> (m_history.size());
            m_session.edits    = EditController();
            m_isFirstApplyDone = true;
            m_byteView.SetEditing (false);
        }
        else if (reply.applyKind == ApplyKind::Undo)
        {
            m_historyAt--;
            m_history[m_historyAt].countsUndone = reply.countsAfter;
        }
        else
        {
            m_history[m_historyAt].countsAfter = reply.countsAfter;
            m_historyAt++;
        }

        for (const std::shared_ptr<const TrackCopy> & copy : reply.after)
        {
            m_session.appliedSlots.insert (copy->slot);
        }

        m_session.isTablesDirty = true;

        m_session.editNote = (reply.applyOutcome == ApplyOutcome::AppliedToPreservedCopy)
                         ? L"Saved to a preserved copy, since the file changed outside Casso: " + std::filesystem::path (TextEncoding::Utf8ToWide (reply.savedPath)).filename().wstring()
                         : std::wstring ((reply.applyKind == ApplyKind::Undo) ? L"Undone" : (reply.applyKind == ApplyKind::Redo) ? L"Redone" : L"Applied") + L" and saved to "
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
//  B's bytes for the sector paired with A's selected one, while A is the
//  window's disk as it is now (FR-123).
//
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::GetBSectorBytes (PendingEdit::Sector & outBytes) const
{
    const TrackAnalysis *  trackA  = m_session.model.GetTrack();
    const TrackAnalysis *  trackB  = m_session.context.trackB;
    int                    paired  = -1;
    bool                   isFound = false;



    if (m_session.comparison.IsComparing() && m_session.sourceA == ComparisonSource { ComparisonSourceKind::DriveNow, m_session.drive } && trackA != nullptr && trackB != nullptr &&
        m_session.model.GetSectorIndex() >= 0)
    {
        paired  = DiskComparer::FindPairedSector (*trackA, m_session.model.GetSectorIndex(), *trackB);
        isFound = paired >= 0 && trackB->sectors[paired].dataField >= 0;

        if (isFound)
        {
            outBytes = trackB->fields[trackB->sectors[paired].dataField].data.bytes;
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
//  EditingPart::SaveEditedCopy
//
//  "Save edited copy..." (FR-101): the emulation thread builds the copy's
//  bytes from the drive's disk and the pending edits, and the reply opens
//  the Save dialog.
//
////////////////////////////////////////////////////////////////////////////////

void EditingPart::SaveEditedCopy()
{
    InspectorRequest  request;



    request.kind       = InspectorRequestKind::SaveCopy;
    request.drive      = m_session.drive;
    request.mediaId    = m_session.analysis.mediaId;
    request.writes     = m_session.edits.BuildWrites (m_session.analysis);
    m_saveCopyRequest  = m_session.host->PostInspectorRequest (request);
    m_session.editNote = L"Preparing the copy...";
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
        BAIL_OUT_IF (!m_host.Confirm (L"Save edited copy", { L"This image is damaged. The copy gets a new checksum, and each damaged track record is saved as Casso read it.",
                                                         L"Save the copy?" }, L"Save"), S_OK);
    }

    extension             = (m_session.analysis.copy != nullptr && m_session.analysis.copy->format == DiskFormat::Woz) ? std::wstring (L"woz") : extension;
    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = image.stem().wstring() + L" edited." + extension;
    spec.initialFolder    = image.parent_path();

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
            (void) DxuiMessageBox (m_host.GetOwnerHwnd(), m_session.theme, L"That file is a disk in a drive. Choose another file for the copy.", L"Save edited copy", MB_OK | MB_ICONWARNING);
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
////////////////////////////////////////////////////////////////////////////////

bool EditingPart::ConfirmDiscardEdits (const std::wstring & action)
{
    EditController::Counts  counts    = m_session.edits.GetCounts();
    bool                    isGoingOn = counts.sectors == 0;



    if (!isGoingOn)
    {
        isGoingOn = m_host.Confirm (L"Pending edits", { EditBar::FormatCount (counts.bytes, counts.sectors) + L" and not applied.",
                                                     action + L" discards these edits." }, L"Discard edits");
    }

    if (isGoingOn && counts.sectors > 0)
    {
        m_session.edits      = EditController();
        m_session.editNote   = action + L" discarded the pending edits.";
        m_host.SetStatusNote (m_session.editNote);
        m_byteView.SetEditing (false);
    }

    return isGoingOn;
}
