#include "Pch.h"

#include "CassoLink/FullSend/FullSendStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::FullSendStatus
//
////////////////////////////////////////////////////////////////////////////////

FullSendStatus::FullSendStatus (SecondsFn seconds, TimeFn time) :
    m_seconds (std::move (seconds)),
    m_time    (std::move (time))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::Begin
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::Begin()
{
    std::lock_guard  lock (m_lock);
    uint64_t         revision = m_snapshot.revision;



    m_snapshot          = FullSendSnapshot();
    m_snapshot.activity = FullSendActivity::Starting;
    m_snapshot.revision = revision + 1;
    m_resume            = FullSendActivity::None;
    m_resumeDrive       = 0;
    m_began             = m_seconds();
    m_ended             = -1.0;
    m_sentSeconds       = 0.0;
    m_baseCount         = { 0, 0 };
    m_lastMark          = { m_began, m_began };

    m_samples.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::End
//
//  The totals' time stops, and each drive's disk in progress is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::End (const std::string & reason)
{
    std::lock_guard  lock (m_lock);



    m_ended             = m_seconds();
    m_snapshot.activity = FullSendActivity::Ended;
    m_snapshot.drive    = 0;
    m_snapshot.reason   = reason;

    for (FullSendDriveStatus & drive : m_snapshot.drives)
    {
        bool  busy = drive.state == FullSendDiskState::Checking || drive.state == FullSendDiskState::Sending || drive.state == FullSendDiskState::Capturing;

        drive.state = busy ? FullSendDiskState::None : drive.state;
    }

    AddTimedLine (reason);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::SetActivity
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::SetActivity (FullSendActivity activity, int drive)
{
    std::lock_guard  lock (m_lock);



    m_snapshot.activity = activity;
    m_snapshot.drive    = drive;
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::SetWaiting
//
//  A wait for the Apple's reply begins over the activity it interrupts, and
//  ends with that activity again and a line with the wait's length.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::SetWaiting (bool waiting)
{
    std::lock_guard  lock (m_lock);
    bool             isWaiting = m_snapshot.activity == FullSendActivity::WaitingForApple;



    if (waiting && !isWaiting)
    {
        m_resume            = m_snapshot.activity;
        m_resumeDrive       = m_snapshot.drive;
        m_waitBegan         = m_seconds();
        m_snapshot.activity = FullSendActivity::WaitingForApple;
        m_snapshot.revision++;
    }

    if (!waiting && isWaiting)
    {
        m_snapshot.activity = m_resume;
        m_snapshot.drive    = m_resumeDrive;

        AddTimedLine (std::format ("The Apple replied after {:.0f} seconds.", m_seconds() - m_waitBegan));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::BeginDisk
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::BeginDisk (int drive, bool verified, std::span<const int> haveTracks)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);
    FullSendTrackMark      mark   = verified ? FullSendTrackMark::Verified : FullSendTrackMark::Read;
    int                    count  = 0;



    status          = FullSendDriveStatus();
    status.state    = FullSendDiskState::Sending;
    status.verified = verified;
    status.started  = m_seconds();

    for (int track : haveTracks)
    {
        if (track >= 0 && track < FullSendDriveStatus::kTracks && status.tracks[(size_t) track] == FullSendTrackMark::Unread)
        {
            status.tracks[(size_t) track] = mark;
            count++;
        }
    }

    m_baseCount[GetIndex (drive)] = count;
    m_lastMark[GetIndex (drive)]  = status.started;
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::SetTitle (int drive, const std::string & title, const std::string & fileSystem)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);



    status.title      = title;
    status.fileSystem = fileSystem;
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::MarkTrack
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::MarkTrack (int drive, int track, FullSendTrackMark mark)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);



    if (track < 0 || track >= FullSendDriveStatus::kTracks)
    {
        return;
    }

    status.tracks[(size_t) track] = mark;
    status.percent                = CountRead (status) * 100 / FullSendDriveStatus::kTracks;
    m_lastMark[GetIndex (drive)]  = m_seconds();
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::BeginCapture
//
//  The drive's disk, read as sectors until now, is captured raw: its title
//  and start time stay, and its progress begins again.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::BeginCapture (int drive)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);



    status.state       = FullSendDiskState::Capturing;
    status.positions.assign (FullSendDriveStatus::kPositions, FullSendPositionMark::Unread);
    status.pass        = 1;
    status.position    = 0;
    status.read        = 0;
    status.reads       = 0;
    status.percent     = 0;
    status.secondsLeft = -1.0;

    m_lastMark[GetIndex (drive)] = m_seconds();
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::SetCapture
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::SetCapture (int drive, int pass, int position, int read, int reads, int percent, double secondsLeft)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);



    status.pass        = pass;
    status.position    = position;
    status.read        = read;
    status.reads       = reads;
    status.percent     = percent;
    status.secondsLeft = secondsLeft;

    if (position >= 0 && (size_t) position < status.positions.size() && status.positions[(size_t) position] == FullSendPositionMark::Unread)
    {
        status.positions[(size_t) position] = FullSendPositionMark::Read;
    }

    m_lastMark[GetIndex (drive)] = m_seconds();
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::MarkPosition
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::MarkPosition (int drive, int position, FullSendPositionMark mark)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);



    if (position >= 0 && (size_t) position < status.positions.size())
    {
        status.positions[(size_t) position] = mark;
        m_snapshot.revision++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::SetCaptureTime
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::SetCaptureTime (int drive, double diskSeconds, double lineSeconds)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status = GetDrive (drive);



    status.diskSeconds = diskSeconds;
    status.lineSeconds = lineSeconds;
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::EndDisk
//
//  The disk goes to the top of the list of disks done and into the totals.
//  A sector image's time is a sample for the next disk's time left.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::EndDisk (int drive, const FullSendRow & row, uint64_t bytes)
{
    std::lock_guard        lock (m_lock);
    FullSendDriveStatus  & status      = GetDrive (drive);
    FullSendTotals       & totals      = m_snapshot.totals;
    bool                   isDuplicate = row.status == FullSendManifest::kStatusDuplicate;
    bool                   isRaw       = row.status == FullSendManifest::kStatusRawCapture;
    FullSendDoneDisk       done;



    done.row        = row;
    done.fileSystem = status.fileSystem;
    done.seconds    = m_seconds() - status.started;

    m_snapshot.disks.insert (m_snapshot.disks.begin(), done);

    totals.duplicates  += isDuplicate ? 1 : 0;
    totals.sent        += isDuplicate ? 0 : 1;
    totals.rawCaptures += isRaw ? 1 : 0;
    totals.badSectors  += (row.unreadable != 0 || row.unreliable != 0) ? 1 : 0;
    totals.bytes       += bytes;

    if (!isDuplicate)
    {
        m_sentSeconds += done.seconds;
    }

    if (!isDuplicate && !isRaw)
    {
        m_samples.push_back ({ status.verified, done.seconds });
    }

    status.state       = isDuplicate ? FullSendDiskState::Duplicate : FullSendDiskState::Sent;
    status.percent     = 100;
    status.secondsLeft = -1.0;
    status.elapsed     = done.seconds;
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::ClearDisk
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::ClearDisk (int drive, FullSendDiskState state)
{
    std::lock_guard  lock (m_lock);



    GetDrive (drive)       = FullSendDriveStatus();
    GetDrive (drive).state = state;
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::AddLine
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::AddLine (const std::string & text)
{
    std::lock_guard  lock (m_lock);



    AddTimedLine (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::AddTimedLine
//
//  Called with the lock taken.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendStatus::AddTimedLine (const std::string & text)
{
    m_snapshot.lines.push_back ({ m_time(), text });
    m_snapshot.revision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::GetSnapshot
//
//  A copy with the times as of now: each disk in progress has its elapsed
//  time and time left, and the totals their elapsed time and average.
//
////////////////////////////////////////////////////////////////////////////////

FullSendSnapshot FullSendStatus::GetSnapshot() const
{
    std::lock_guard   lock (m_lock);
    FullSendSnapshot  snapshot = m_snapshot;
    double            now      = m_seconds();



    for (size_t index = 0; index < snapshot.drives.size(); index++)
    {
        FullSendDriveStatus  & drive = snapshot.drives[index];
        bool                   busy  = drive.state == FullSendDiskState::Sending || drive.state == FullSendDiskState::Capturing;

        if (busy)
        {
            drive.elapsed     = now - drive.started;
            drive.secondsLeft = EstimateLeft (index, now);
        }
    }

    snapshot.waited         = snapshot.activity == FullSendActivity::WaitingForApple ? now - m_waitBegan : 0.0;
    snapshot.totals.elapsed = (m_ended >= 0.0 ? m_ended : now) - m_began;
    snapshot.totals.average = snapshot.totals.sent != 0 ? m_sentSeconds / snapshot.totals.sent : 0.0;

    return snapshot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::EstimateLeft
//
//  A raw capture's time left is the capture's, less the time since it was
//  set. A sector image's is the tracks left at the rate of the tracks read
//  since the disk began, once kRateTracks are, and before that the average
//  of the sector images read the same way, less the time so far. Negative
//  with neither.
//
////////////////////////////////////////////////////////////////////////////////

double FullSendStatus::EstimateLeft (size_t index, double now) const
{
    const FullSendDriveStatus  & drive   = m_snapshot.drives[index];
    double                       since   = now - m_lastMark[index];
    int                          read    = CountRead (drive);
    int                          counted = read - m_baseCount[index];
    double                       total   = 0.0;
    int                          samples = 0;



    if (drive.state == FullSendDiskState::Capturing)
    {
        return drive.secondsLeft < 0.0 ? -1.0 : std::max (0.0, drive.secondsLeft - since);
    }

    if (counted >= kRateTracks)
    {
        double  rate = (m_lastMark[index] - drive.started) / counted;

        return std::max (0.0, rate * (FullSendDriveStatus::kTracks - read) - since);
    }

    for (const Sample & sample : m_samples)
    {
        if (sample.verified == drive.verified)
        {
            total += sample.seconds;
            samples++;
        }
    }

    return samples == 0 ? -1.0 : std::max (0.0, total / samples - (now - drive.started));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus::CountRead
//
////////////////////////////////////////////////////////////////////////////////

int FullSendStatus::CountRead (const FullSendDriveStatus & drive)
{
    return (int) std::count_if (drive.tracks.begin(), drive.tracks.end(), [] (FullSendTrackMark mark) { return mark != FullSendTrackMark::Unread; });
}
