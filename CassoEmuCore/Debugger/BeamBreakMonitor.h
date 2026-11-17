#pragma once

class IDebugTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  BeamBreakMonitor
//
//  The stops a debug session takes at a place in the video frame: BPV's,
//  when the beam enters a range of scanlines, BPBEAM's, when the beam
//  reaches a cycle within the frame, and FRAME's, a beam break for one run.
//  Poll follows the beam at each instruction boundary.
//
////////////////////////////////////////////////////////////////////////////////

class BeamBreakMonitor
{
public:
    explicit BeamBreakMonitor (IDebugTarget & target) : m_target (target) {}

    void   SetVideoBreak   (uint32_t first, uint32_t last);
    void   ClearVideoBreak ()       { m_videoBreak.reset(); }
    bool   HasVideoBreak   () const { return m_videoBreak.has_value(); }

    std::optional<std::pair<uint32_t, uint32_t>>  GetVideoBreak () const;

    void   SetBeamBreak    (uint32_t cycleInFrame, uint32_t passes, bool isForOneRun);
    void   ClearBeamBreak  (bool isForOneRun);
    bool   HasBeamBreak    () const { return m_beamBreak.has_value() || m_frameBreak.has_value(); }

    std::optional<uint32_t>  GetBeamBreak () const;

    //  Any of the three is set, so the session sees every instruction.
    bool   IsArmed         () const { return m_videoBreak.has_value() || HasBeamBreak(); }

    //  At an instruction boundary: true when a break is reached.
    bool   Poll            ();

    //  A stop takes away the breaks that made it, and FRAME's whatever made it.
    void   OnStopped       ();
    void   ClearAll        ();

private:
    struct VideoBreak
    {
        uint32_t  first        = 0;
        uint32_t  last         = 0;
        uint32_t  lastScanline = 0;
    };

    struct BeamBreak
    {
        uint32_t  target    = 0;
        uint32_t  passes    = 1;
        uint32_t  lastCycle = 0;
        bool      isOneRun  = false;
    };

    bool                 HasEnteredVideoBreak ();
    bool                 HasReachedBeamBreak  (std::optional<BeamBreak> & beamBreak);
    uint32_t             GetBeamCycle         () const;

    IDebugTarget                & m_target;
    std::optional<VideoBreak>     m_videoBreak;
    bool                          m_videoBreakHit = false;
    std::optional<BeamBreak>      m_beamBreak;
    bool                          m_beamBreakHit  = false;
    std::optional<BeamBreak>      m_frameBreak;                 // FRAME's, for one run beside BPBEAM's
    bool                          m_frameBreakHit = false;
};
