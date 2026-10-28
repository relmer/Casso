#include "Pch.h"

#include "Ui/DiskInspector/Components/HostLink.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterView.h"





//  The written records are copied again at most twice a second (FR-069).
static constexpr int64_t  s_kRecopyMs = 500;





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::HostLink
//
////////////////////////////////////////////////////////////////////////////////

HostLink::HostLink (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::GetPrefs (DiskInspectorPrefs & inOut) const
{
    inOut.isFollowHead = m_followHead.IsOn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::ApplyPrefs
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::ApplyPrefs (const DiskInspectorPrefs & prefs)
{
    SetFollowHead (prefs.isFollowHead);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::RequestCopy
//
//  A copy of whatever disk is in the drive; the reply indicates which disk it is.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::RequestCopy()
{
    InspectorRequest  request;



    request.kind             = InspectorRequestKind::CopyDisk;
    request.drive            = m_session.drive;
    m_session.pendingRequest = m_session.host->PostInspectorRequest (request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::TakeReplies
//
//  Only the reply to the latest request counts. Its disk starts the view
//  over unless it is the same disk; every record waits for its analysis.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::TakeReplies()
{
    vector<InspectorReply>  replies;



    m_session.host->TakeInspectorReplies (replies);

    for (InspectorReply & reply : replies)
    {
        if (m_session.comparison.OfferReply (reply))
        {
            continue;
        }

        if (reply.requestId == m_session.pendingRequest)
        {
            m_session.pendingRequest = 0;
            StartDisk (reply.disk, (reply.status == InspectorReplyStatus::Unopenable) ? reply.reason : std::wstring());
        }
        else if (reply.requestId == m_trackRequest)
        {
            m_trackRequest = 0;
            TakeWrittenTracks (reply);
        }
        else
        {
            (void) m_host.OfferReply (reply);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::StartDisk
//
//  A new disk for side A starts the view over; every record waits for its
//  analysis. With no disk, the session has the reason it could not be opened.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::StartDisk (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason)
{
    DecodeSettings  settings  = m_session.comparison.IsComparing() ? m_session.analysis.settings : DecodeSettings::MakeStandard();
    int             slot      = 0;
    uint64_t        lastMedia = m_session.analysis.mediaId;
    std::string     lastFile  = (m_session.analysis.copy != nullptr) ? m_session.analysis.copy->fileName : std::string();
    bool            isReload  = copy != nullptr && !lastFile.empty() && copy->fileName == lastFile;
    DiskChange      change;



    change.hasDisk     = copy != nullptr;
    change.isNewDisk   = copy == nullptr || copy->mediaId != lastMedia;
    change.isReload    = isReload;
    change.lastMediaId = lastMedia;

    if (copy != nullptr && copy->mediaId != lastMedia)
    {
        m_session.statusNote.clear();
    }

    m_host.NotifyDiskStarting (change);

    m_session.context.hasDisk = (copy != nullptr);
    m_session.openError       = reason;
    m_session.changedSlots.clear();
    m_session.levels.fill (nullptr);
    m_session.timingLevels.fill (nullptr);

    if (m_session.context.hasDisk)
    {
        m_session.model.SetDisk (copy->mediaId);

        m_session.analysis           = DiskAnalysis();
        m_session.analysis.mediaId   = copy->mediaId;
        m_session.analysis.copy      = copy;
        m_session.analysis.settings  = settings;
        m_session.analysis.headLimit = Disk2Controller::kMaxQuarterTrack;
        m_session.analysis.tracks.resize (copy->tracks.size());
        DiskAnalyzer::Assemble (m_session.analysis);
        m_session.scheduler->Restart (copy, m_session.analysis.settings);

        for (slot = 0; slot < static_cast<int> (copy->tracks.size()); slot++)
        {
            m_session.changedSlots.insert (slot);
        }


        //  The guest's writes are counted from the copy's write counts.
        m_session.writeCounts.clear();
        m_session.writtenSlots.clear();
        m_trackRequest = 0;

        for (const std::shared_ptr<const TrackCopy> & track : copy->tracks)
        {
            m_session.writeCounts.push_back (track != nullptr ? track->guestWriteCount : 0);
        }
    }
    else
    {
        m_session.model.SetDisk (0);
        m_session.analysis = DiskAnalysis();
    }

    m_session.model.SetAnalysis (m_session.context.hasDisk ? &m_session.analysis : nullptr);
    m_session.comparison.MarkAChanged();
    m_session.isTablesDirty = true;

    m_host.NotifyDiskStarted (change);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::TakeResults
//
//  Every result that arrived since the last frame is stored, then the disk
//  is rebuilt once.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::TakeResults()
{
    vector<RecordResult>  results;



    m_session.scheduler->TakeResults (results);

    for (RecordResult & result : results)
    {
        if (result.mediaId == m_session.analysis.mediaId)
        {
            DiskAnalyzer::Accept (result.copy, result.slot, std::move (result.analysis), m_session.analysis);
            m_session.changedSlots.insert (result.slot);
            m_session.levels[result.slot]       = nullptr;
            m_session.timingLevels[result.slot] = nullptr;
        }
    }

    if (!results.empty())
    {
        DiskAnalyzer::Assemble (m_session.analysis);
        m_session.model.SetAnalysis (&m_session.analysis);
        m_session.comparison.MarkAChanged (GetQuarterTracksOf (results));
        m_session.isTablesDirty = true;
        m_host.NotifyAnalysisChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::GetQuarterTracksOf
//
//  Every quarter track mapped to one of the analyzed tracks' slots.
//
////////////////////////////////////////////////////////////////////////////////

std::set<int> HostLink::GetQuarterTracksOf (const vector<RecordResult> & results) const
{
    std::set<int>  slots;
    std::set<int>  quarterTracks;



    for (const RecordResult & result : results)
    {
        slots.insert (result.slot);
    }

    for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        if (slots.contains (m_session.analysis.entries[qt].slot))
        {
            quarterTracks.insert (qt);
        }
    }

    return quarterTracks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::TickHead
//
//  Once a frame: the drive's head as last published, and with Follow head
//  on, the head's track once the head has settled on it (FR-067).
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::TickHead()
{
    int            qt       = -1;
    bool           isOnDisk = false;
    InspectorHead  head;



    m_session.headState = m_session.host->GetHeadState (m_session.drive);

    //  A disk inserted, swapped or ejected has a new media id; a copy of it is
    //  requested once, and the reply replaces the analysis on screen.
    if (m_session.headState != nullptr && m_session.IsWindowDiskA() && m_session.pendingRequest == 0 && m_session.headState->mediaId != m_session.analysis.mediaId &&
        m_session.headState->mediaId != m_session.requestedMediaId)
    {
        m_session.requestedMediaId = m_session.headState->mediaId;
        RequestCopy();
    }

    RequestWrittenTracks();

    isOnDisk = m_session.headState != nullptr && m_session.headState->hasDisk && m_session.headState->mediaId == m_session.analysis.mediaId && m_session.IsWindowDiskA();

    if (isOnDisk)
    {
        qt = m_followHead.Tick (m_session.headState->headQuarterTrack, static_cast<int64_t> (GetTickCount64()));

        head.isShown      = true;
        head.quarterTrack = m_session.headState->headQuarterTrack;
        head.turn         = m_session.headState->angle;
        head.activity     = m_session.headState->activity;
    }

    m_session.context.head  = head;
    m_session.context.drive = isOnDisk ? m_session.headState.get() : nullptr;

    //  While the drive turns, the platter turns under the head at 12
    //  o'clock, or at the near edge when tilted (FR-152). Zoomed in, the
    //  platter does not turn and the head moves around it (FR-064).
    if (isOnDisk && m_session.headState->isTurning && m_session.model.IsAtFit())
    {
        m_session.model.SetRotation ((m_session.model.IsTilted() ? 0.5 : 0.0) - m_session.headState->angle);
    }

    if (qt >= 0)
    {
        SelectHeadTrack (qt);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::SetFollowHead
//
//  Turning Follow head on selects the head's track at once.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::SetFollowHead (bool isOn)
{
    int  head = (m_session.headState != nullptr) ? m_session.headState->headQuarterTrack : m_session.model.GetQuarterTrack();
    int  qt   = m_followHead.SetOn (isOn, head, static_cast<int64_t> (GetTickCount64()));



    if (qt >= 0)
    {
        SelectHeadTrack (qt);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::GoToHead
//
//  "Go to head": the head's track, once, leaving Follow head as it is.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::GoToHead()
{
    if (m_session.headState != nullptr && m_session.headState->hasDisk)
    {
        SelectHeadTrack (m_session.headState->headQuarterTrack);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::SelectHeadTrack
//
//  A selection made for the head rather than by hand, so Follow head stays.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::SelectHeadTrack (int quarterTrack)
{
    m_isHeadSelecting = true;
    m_session.model.SelectQuarterTrack (quarterTrack);
    m_host.OnSelection();
    m_isHeadSelecting = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::RequestWrittenTracks
//
//  The records the guest has written since the window's copy, from the
//  published write counts, copied again at most twice a second so a track
//  being written continuously is redrawn within half a second (FR-069).
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::RequestWrittenTracks()
{
    const DriveHeadState *  state = m_session.headState.get();
    int64_t                 now   = static_cast<int64_t> (GetTickCount64());
    InspectorRequest        request;
    size_t                  slot  = 0;



    if (state != nullptr && m_session.analysis.copy != nullptr && state->mediaId == m_session.analysis.mediaId && m_session.IsWindowDiskA() && state->countsGeneration != m_countsGeneration)
    {
        for (slot = 0; slot < state->recordWriteCounts.size() && slot < m_session.analysis.copy->tracks.size(); slot++)
        {
            if (slot >= m_session.writeCounts.size() || state->recordWriteCounts[slot] != m_session.writeCounts[slot])
            {
                m_session.writtenSlots.insert (static_cast<int> (slot));
            }
        }

        m_session.writeCounts   = state->recordWriteCounts;
        m_countsGeneration      = state->countsGeneration;
        m_session.isTablesDirty = true;
    }

    if (!m_session.writtenSlots.empty() && m_trackRequest == 0 && now - m_trackRequestMs >= s_kRecopyMs)
    {
        request.kind     = InspectorRequestKind::CopyTracks;
        request.drive    = m_session.drive;
        request.mediaId  = m_session.analysis.mediaId;
        request.slots.assign (m_session.writtenSlots.begin(), m_session.writtenSlots.end());
        m_trackRequest   = m_session.host->PostInspectorRequest (request);
        m_trackRequestMs = now;
        m_session.writtenSlots.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::TakeWrittenTracks
//
//  The written records go into a new copy of the disk, sharing every other
//  record with the old one, and only they are analyzed again. A reply for a
//  disk no longer in the drive starts the window over on the new one.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::TakeWrittenTracks (const InspectorReply & reply)
{
    vector<int>  slots;



    if (reply.status == InspectorReplyStatus::DiskChanged || reply.status == InspectorReplyStatus::NoDisk)
    {
        RequestCopy();
    }
    else if (m_session.analysis.copy != nullptr && reply.mediaId == m_session.analysis.mediaId && !reply.tracks.empty())
    {
        auto  copy = std::make_shared<DiskCopy> (*m_session.analysis.copy);

        for (const std::shared_ptr<const TrackCopy> & track : reply.tracks)
        {
            if (track != nullptr && track->slot >= 0 && track->slot < static_cast<int> (copy->tracks.size()))
            {
                copy->tracks[track->slot] = track;
                slots.push_back (track->slot);
            }
        }

        m_session.analysis.copy = copy;
        m_session.scheduler->Update (copy, slots);
        m_session.isTablesDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::IsHeadShown
//
//  "Follow head" and "Go to head" are offered while Casso publishes the
//  head of the window's drive.
//
////////////////////////////////////////////////////////////////////////////////

bool HostLink::IsHeadShown() const
{
    return m_session.context.hasDisk && m_session.headState != nullptr && m_session.IsWindowDiskA();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink::UpdateRings
//
//  The platter's rings for every record whose analysis or state changed. A
//  record's cells are built once and shared by the quarter tracks that play
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void HostLink::UpdateRings()
{
    const DiskCopy *   copy     = m_session.analysis.copy.get();
    PlatterRenderer &  renderer = m_session.views.platter->GetRenderer();
    int                qt       = 0;
    int                slot     = 0;
    bool               damaged  = false;
    vector<Byte>       cells;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        slot    = (copy != nullptr) ? copy->playedSlot[qt] : -1;
        damaged = copy != nullptr && slot < 0 && copy->mappedSlot[qt] >= 0 && copy->IsSlotDamaged (copy->mappedSlot[qt]);

        if (slot < 0)
        {
            renderer.SetRing (qt, damaged ? PlatterRingState::Damaged : PlatterRingState::Nothing, nullptr);
        }
        else if (m_session.scheduler->IsPending (slot) || slot >= static_cast<int> (m_session.analysis.tracks.size()) || m_session.analysis.tracks[slot] == nullptr)
        {
            renderer.SetRing (qt, PlatterRingState::Pending, nullptr);
        }
        else if (m_session.changedSlots.contains (slot) || m_session.levels[slot] == nullptr)
        {
            if (m_session.levels[slot] == nullptr)
            {
                auto  levels = std::make_shared<PlatterRenderer::Levels>();

                PlatterCells::BuildCells  (*m_session.analysis.tracks[slot], cells);
                PlatterCells::BuildLevels (cells, *levels);
                m_session.levels[slot] = levels;

                if (m_session.analysis.tracks[slot]->framed.isFlux)
                {
                    auto  timing = std::make_shared<PlatterRenderer::Levels>();

                    FluxTiming::BuildDeviations (*m_session.analysis.tracks[slot], cells);
                    FluxTiming::BuildLevels     (cells, *timing);
                    m_session.timingLevels[slot] = timing;
                }
            }

            renderer.SetRing (qt, PlatterRingState::Data, m_session.levels[slot], m_session.timingLevels[slot]);
        }
    }

    m_session.changedSlots.clear();
}
