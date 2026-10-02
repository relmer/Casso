#pragma once

#include "Devices/Tape/ITapeDeckPort.h"
#include "Devices/Tape/RecordingCapture.h"
#include "Devices/Tape/TapeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTransport
//
////////////////////////////////////////////////////////////////////////////////

enum class TapeTransport
{
    Empty,
    Stopped,
    Playing,
    Recording,
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck
//
//  The recorder: transport, position and the inserted tape. Position is in
//  samples of the tape's own rate and advances only with emulated bus cycles,
//  so a paused machine holds it and any emulation speed loads identically.
//
//  Every mutator and the port calls run on the CPU thread. The UI reads the
//  atomic snapshot (GetSnapshot) and never touches the image.
//
////////////////////////////////////////////////////////////////////////////////

class TapeDeck : public ITapeDeckPort
{
public:
    struct Snapshot
    {
        TapeTransport  transport       = TapeTransport::Empty;
        double         positionSamples = 0.0;
        uint64_t       lengthSamples   = 0;
        uint32_t       sampleRate      = 0;
        bool           isRecordArmed   = false;
        bool           isWritable      = false;
    };

    void  SetCpuClock    (double cpuClockHz);
    void  Insert         (TapeImage && image);
    void  ReplaceImage   (TapeImage && image);
    void  Eject          (uint64_t nowCycle);
    void  Play           (uint64_t nowCycle);
    void  Stop           (uint64_t nowCycle);
    void  Rewind         (uint64_t nowCycle);
    void  SetRecordArmed (bool isArmed);
    void  SetAutoStop    (bool isOn) { m_isAutoStop.store (isOn, std::memory_order_relaxed); }
    void  Update         (uint64_t nowCycle);

    bool  ReadInputLevel (uint64_t busCycle) override;
    bool  PeekLevel      (uint64_t busCycle) const;
    void  OnOutputToggle (uint64_t busCycle) override;

    TapeTransport      GetTransport       () const { return m_transport; }
    double             GetPositionSamples (uint64_t nowCycle) const;
    uint64_t           GetLastAccessCycle () const { return m_lastAccessCycle; }
    bool               HasBeenAccessed    () const { return m_hasBeenAccessed; }
    const TapeImage  * GetImage           () const { return m_hasImage ? &m_image : nullptr; }
    Snapshot           GetSnapshot        () const;

    const std::vector<uint64_t> & GetCapturedToggles   () const { return m_capture.toggleCycles; }
    double                        GetRecordStartSample () const { return m_capture.startSample; }
    uint64_t                      GetRecordStartCycle  () const { return m_capture.startCycle; }
    double                        GetCpuClock          () const { return m_cpuClockHz; }

    //  A recording that has stopped, by any path, and not yet been written.
    bool  HasPendingRecording () const { return m_hasPendingRecording; }
    void  TakeRecording       (RecordingCapture & capture);

private:
    static constexpr double  kDefaultCpuClockHz = 1020484.0;   // NTSC Apple II; replaced by the machine's own rate

    bool    IsMoving         () const;
    double  GetSampleAtCycle (uint64_t cycle) const;
    void    Halt             (uint64_t nowCycle);
    void    PublishSnapshot  ();

    TapeImage         m_image;
    bool              m_hasImage            = false;
    TapeTransport     m_transport           = TapeTransport::Empty;
    double            m_cpuClockHz          = kDefaultCpuClockHz;
    double            m_startSample         = 0.0;
    uint64_t          m_startCycle          = 0;
    size_t            m_cursor              = 0;
    double            m_cursorSample        = 0.0;
    bool              m_isRecordArmed       = false;
    uint64_t          m_lastAccessCycle     = 0;
    bool              m_hasBeenAccessed     = false;
    RecordingCapture  m_capture;
    bool              m_hasPendingRecording = false;

    std::atomic<bool>           m_isAutoStop      { true };
    std::atomic<TapeTransport>  m_shownTransport  { TapeTransport::Empty };
    std::atomic<double>         m_shownPosition   { 0.0 };
    std::atomic<uint64_t>       m_shownLength     { 0 };
    std::atomic<uint32_t>       m_shownRate       { 0 };
    std::atomic<bool>           m_shownArmed      { false };
    std::atomic<bool>           m_shownWritable   { false };
};
