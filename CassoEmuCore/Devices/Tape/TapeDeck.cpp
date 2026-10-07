#include "Pch.h"

#include "Devices/Tape/TapeDeck.h"

#include "Devices/Tape/TapeRecordScanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::SetCpuClock
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::SetCpuClock (double cpuClockHz)
{
    if (cpuClockHz > 0.0)
    {
        m_cpuClockHz = cpuClockHz;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Insert
//
//  A newly inserted tape is stopped at its start with record released.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Insert (TapeImage && image)
{
    m_image         = std::move (image);
    m_hasImage      = true;
    m_transport     = TapeTransport::Stopped;
    m_startSample   = 0.0;
    m_startCycle    = 0;
    m_cursor        = 0;
    m_cursorSample  = 0.0;
    m_isRecordArmed = false;
    m_capture             = RecordingCapture();
    m_hasPendingRecording = false;

    FindLeaders();
    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::ReplaceImage
//
//  Swaps in the rewritten tape after a recording, leaving the transport and
//  the position where they are, so the tape stands just past what was
//  recorded, as it would on a real deck.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::ReplaceImage (TapeImage && image)
{
    m_image        = std::move (image);
    m_hasImage     = true;
    m_cursor       = 0;
    m_cursorSample = 0.0;

    FindLeaders();
    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::TakeRecording
//
//  Hands over the stopped recording for writing; the deck no longer holds it.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::TakeRecording (RecordingCapture & capture)
{
    capture               = std::move (m_capture);
    m_capture             = RecordingCapture();
    m_hasPendingRecording = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Eject
//
//  Any recording must be committed by the caller before this; the capture is
//  discarded with the tape.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Eject (uint64_t nowCycle)
{
    Halt (nowCycle);

    m_image         = TapeImage();
    m_hasImage      = false;
    m_transport     = TapeTransport::Empty;
    m_startSample   = 0.0;
    m_cursor        = 0;
    m_cursorSample  = 0.0;
    m_isRecordArmed = false;
    m_capture             = RecordingCapture();
    m_hasPendingRecording = false;

    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Play
//
//  Starts the tape from where it stands. With record armed on a writable tape
//  it records instead, and the capture starts empty.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Play (uint64_t nowCycle)
{
    if (m_transport != TapeTransport::Stopped)
    {
        return;
    }

    m_startCycle    = nowCycle;
    m_readSincePlay = false;

    if (m_isRecordArmed && m_image.isWritable)
    {
        m_transport         = TapeTransport::Recording;
        m_capture             = RecordingCapture();
        m_capture.startSample = m_startSample;
        m_capture.startCycle  = nowCycle;
        m_hasPendingRecording = false;
    }
    else
    {
        m_transport = TapeTransport::Playing;
    }

    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Stop
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Stop (uint64_t nowCycle)
{
    Halt (nowCycle);
    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Rewind
//
//  Rewinds toward the start, as the key does: first stops whatever the tape
//  was doing, then stops again at the start or when Stop is pressed.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Rewind (uint64_t nowCycle)
{
    StartWinding (nowCycle, TapeTransport::Rewinding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::FastForward
//
//  Fast-forwards toward the end, stopping there or when Stop is pressed.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::FastForward (uint64_t nowCycle)
{
    StartWinding (nowCycle, TapeTransport::FastForwarding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Record
//
//  Starts recording from where the tape stands, on a writable tape that is
//  stopped. One key does it: a person clicking cannot press Record and Play
//  together the way a hand does on the real deck.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Record (uint64_t nowCycle)
{
    if (m_transport != TapeTransport::Stopped || !m_image.isWritable)
    {
        return;
    }

    m_isRecordArmed = true;
    Play (nowCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::StartWinding
//
//  Rewinding at the start or fast-forwarding at the end leaves the tape
//  stopped.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::StartWinding (uint64_t nowCycle, TapeTransport direction)
{
    bool  atStart = false;
    bool  atEnd   = false;



    if (!m_hasImage || m_transport == direction)
    {
        return;
    }

    Halt (nowCycle);

    atStart = m_startSample <= 0.0;
    atEnd   = m_startSample >= (double) m_image.signal.lengthSamples;

    if (!(direction == TapeTransport::Rewinding ? atStart : atEnd))
    {
        m_transport  = direction;
        m_startCycle = nowCycle;
    }

    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Seek
//
//  Moves to a position, in seconds from the start, and stops there, as
//  fast-forward or rewind to a counter reading would. A position past the end
//  stops at the end. With no tape inserted this does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Seek (uint64_t nowCycle, double seconds)
{
    const std::vector<double> &  transitions = m_image.signal.transitions;
    double                       sample      = 0.0;



    if (!m_hasImage)
    {
        return;
    }

    Halt (nowCycle);

    sample = clamp (seconds * (double) m_image.signal.sampleRate, 0.0, (double) m_image.signal.lengthSamples);

    m_startSample  = sample;
    m_cursor       = (size_t) (upper_bound (transitions.begin(), transitions.end(), sample) - transitions.begin());
    m_cursorSample = sample;

    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::SetRecordArmed
//
//  Record latches only on a writable tape that is not moving, like the record
//  key on a deck whose tab is intact.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::SetRecordArmed (bool isArmed)
{
    bool  canArm = m_hasImage && m_image.isWritable && !IsMoving();



    m_isRecordArmed = isArmed && canArm;
    PublishSnapshot();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Update
//
//  Called once per emulation slice: stops playback that has run off the end
//  and refreshes the snapshot the UI reads. A recording may run past the end; the tape
//  is extended when it is committed.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Update (uint64_t nowCycle)
{
    double  sample    = GetSampleAtCycle (nowCycle);
    bool    isPastEnd = m_transport == TapeTransport::Playing && m_isAutoStop.load (std::memory_order_relaxed) &&
                        sample >= (double) m_image.signal.lengthSamples;
    bool    isWound   = (m_transport == TapeTransport::Rewinding      && sample <= 0.0) ||
                        (m_transport == TapeTransport::FastForwarding && sample >= (double) m_image.signal.lengthSamples);
    bool    isIdle    = m_transport == TapeTransport::Playing && m_readSincePlay &&
                        m_isIdleStop.load (std::memory_order_relaxed) &&
                        (double) (nowCycle - m_lastAccessCycle) > kIdleStopSeconds * m_cpuClockHz;



    if (isPastEnd || isWound || isIdle)
    {
        Halt (nowCycle);
        PublishSnapshot();
    }

    m_shownPosition.store (GetSampleAtCycle (nowCycle), std::memory_order_release);
    m_shownTransport.store (m_transport, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::ReadInputLevel
//
//  The level at a bus cycle. The cursor only walks forward during playback,
//  so each read costs a step or two; it re-seeks by binary search only when
//  the position has gone backward. A stopped or recording deck reads low.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeck::ReadInputLevel (uint64_t busCycle)
{
    const std::vector<double>  & transitions = m_image.signal.transitions;
    double                       sample      = 0.0;



    if (m_transport != TapeTransport::Playing)
    {
        return false;
    }

    m_lastAccessCycle = busCycle;
    m_hasBeenAccessed = true;
    m_readSincePlay   = true;
    sample            = GetSampleAtCycle (busCycle);

    if (sample < m_cursorSample)
    {
        m_cursor = (size_t) (upper_bound (transitions.begin(), transitions.end(), sample) - transitions.begin());
    }

    while (m_cursor < transitions.size() && transitions[m_cursor] <= sample)
    {
        m_cursor++;
    }

    m_cursorSample = sample;

    return m_image.signal.initialLevel != ((m_cursor & 1) != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::PeekLevel
//
//  The level at a bus cycle without counting as a guest access or moving the
//  cursor, for the tape's own sound. Low unless the tape is playing.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeck::PeekLevel (uint64_t busCycle) const
{
    const std::vector<double>  & transitions = m_image.signal.transitions;
    size_t                       flips       = 0;



    if (m_transport != TapeTransport::Playing)
    {
        return false;
    }

    flips = (size_t) (upper_bound (transitions.begin(), transitions.end(), GetSampleAtCycle (busCycle)) - transitions.begin());

    return m_image.signal.initialLevel != ((flips & 1) != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::OnOutputToggle
//
//  Captured only while recording. With record not armed the output goes
//  nowhere, as with a recorder that is not recording.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::OnOutputToggle (uint64_t busCycle)
{
    if (m_transport != TapeTransport::Recording)
    {
        return;
    }

    m_capture.toggleCycles.push_back (busCycle);
    m_lastAccessCycle = busCycle;
    m_hasBeenAccessed = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::GetPositionSamples
//
////////////////////////////////////////////////////////////////////////////////

double TapeDeck::GetPositionSamples (uint64_t nowCycle) const
{
    return GetSampleAtCycle (nowCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::IsOnLeader
//
//  Whether the head is over a record's leader right now: the stretch a
//  loader only waits through.
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeck::IsOnLeader (uint64_t nowCycle) const
{
    double  at = GetSampleAtCycle (nowCycle);



    return std::any_of (m_leaders.begin(), m_leaders.end(),
                        [at] (const std::pair<double, double> & leader) { return at >= leader.first && at < leader.second; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::FindLeaders
//
//  Finds the inserted tape's leaders, once per image, with the same scan the
//  decoder uses to check records. Nonstandard records have no leader and are
//  never sped through.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::FindLeaders()
{
    std::vector<TapeRecord>  records;



    m_leaders.clear();

    TapeRecordScanner::Scan (m_image.signal.transitions, m_image.signal.sampleRate, records);

    for (const TapeRecord & record : records)
    {
        m_leaders.emplace_back (record.leaderStart, record.dataStart);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::GetSnapshot
//
//  Safe from any thread.
//
////////////////////////////////////////////////////////////////////////////////

TapeDeck::Snapshot TapeDeck::GetSnapshot() const
{
    Snapshot  snapshot;



    snapshot.transport       = m_shownTransport.load (std::memory_order_acquire);
    snapshot.positionSamples = m_shownPosition.load  (std::memory_order_acquire);
    snapshot.lengthSamples   = m_shownLength.load    (std::memory_order_acquire);
    snapshot.sampleRate      = m_shownRate.load      (std::memory_order_acquire);
    snapshot.isRecordArmed   = m_shownArmed.load     (std::memory_order_acquire);
    snapshot.isWritable      = m_shownWritable.load  (std::memory_order_acquire);

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::IsMoving
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeck::IsMoving() const
{
    return m_transport == TapeTransport::Playing || m_transport == TapeTransport::Recording || IsWinding();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::IsWinding
//
////////////////////////////////////////////////////////////////////////////////

bool TapeDeck::IsWinding() const
{
    return m_transport == TapeTransport::FastForwarding || m_transport == TapeTransport::Rewinding;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::GetSampleAtCycle
//
////////////////////////////////////////////////////////////////////////////////

double TapeDeck::GetSampleAtCycle (uint64_t cycle) const
{
    double  elapsedCycles = 0.0;
    double  travel        = 0.0;



    if (!IsMoving() || cycle < m_startCycle)
    {
        return m_startSample;
    }

    elapsedCycles = (double) (cycle - m_startCycle);
    travel        = elapsedCycles * m_image.signal.sampleRate / m_cpuClockHz;

    // Fast-forward and rewind run at many times the playing speed and are
    // clamped to the tape's length.
    if (IsWinding())
    {
        travel *= (m_transport == TapeTransport::Rewinding) ? -kWindSpeed : kWindSpeed;

        return clamp (m_startSample + travel, 0.0, (double) m_image.signal.lengthSamples);
    }

    return m_startSample + travel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::Halt
//
//  Freezes the position where the tape stands and stops it. A stopped tape
//  never sits past its end except after a recording, which extends it.
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::Halt (uint64_t nowCycle)
{
    if (!IsMoving())
    {
        return;
    }

    m_startSample = GetSampleAtCycle (nowCycle);

    // Recording ends with the record key released, as Stop leaves it.
    if (m_transport == TapeTransport::Recording)
    {
        m_capture.endCycle    = nowCycle;
        m_hasPendingRecording = true;
        m_isRecordArmed       = false;
    }

    if (m_transport == TapeTransport::Playing)
    {
        m_startSample = min (m_startSample, (double) m_image.signal.lengthSamples);
    }

    m_transport = TapeTransport::Stopped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeck::PublishSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void TapeDeck::PublishSnapshot()
{
    m_shownTransport.store (m_transport,                     std::memory_order_release);
    m_shownPosition.store  (m_startSample,                   std::memory_order_release);
    m_shownLength.store    (m_image.signal.lengthSamples,    std::memory_order_release);
    m_shownRate.store      (m_image.signal.sampleRate,       std::memory_order_release);
    m_shownArmed.store     (m_isRecordArmed,                 std::memory_order_release);
    m_shownWritable.store  (m_hasImage && m_image.isWritable, std::memory_order_release);
}
