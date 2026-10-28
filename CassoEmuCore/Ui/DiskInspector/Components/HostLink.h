#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/FollowHead.h"

class DiskCopy;
class IInspectorComponentHost;
class InspectorSession;
struct RecordResult;





////////////////////////////////////////////////////////////////////////////////
//
//  HostLink
//
//  The inspector's link to its host: the copy of the drive's disk and the
//  replies to its requests, the analysis results as they arrive, the
//  records the guest writes, and the drive's head and Follow head (FR-062
//  to FR-070).
//
////////////////////////////////////////////////////////////////////////////////

class HostLink : public IInspectorComponent
{
public:
    HostLink (InspectorSession & session, IInspectorComponentHost & host);

    void  Create     () override {}
    void  GetPrefs   (DiskInspectorPrefs & inOut) const override;
    void  ApplyPrefs (const DiskInspectorPrefs & prefs) override;

    void  RequestCopy       ();
    void  TakeReplies       ();
    void  StartDisk         (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason);
    void  TakeResults       ();
    void  TickHead          ();
    void  UpdateRings       ();
    void  TakeWrittenTracks (const InspectorReply & reply);
    void  SetFollowHead     (bool isOn);
    void  GoToHead          ();
    bool  IsHeadShown       () const;
    bool  IsHeadSelecting   () const { return m_isHeadSelecting; }
    bool  IsFollowingHead   () const { return m_followHead.IsOn(); }
    void  OnHandSelection   ()       { m_followHead.OnHandSelection(); }

private:
    void           RequestWrittenTracks ();
    void           SelectHeadTrack      (int quarterTrack);
    std::set<int>  GetQuarterTracksOf   (const vector<RecordResult> & results) const;

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    FollowHead                 m_followHead;
    bool                       m_isHeadSelecting  = false;
    uint64_t                   m_countsGeneration = 0;
    uint64_t                   m_trackRequest     = 0;
    int64_t                    m_trackRequestMs   = 0;
};
