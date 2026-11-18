#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Drive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::Disk2Drive
//
////////////////////////////////////////////////////////////////////////////////

Disk2Drive::Disk2Drive()
{
    m_active = &m_disk;
    m_engine.SetDiskImage (m_active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::MoveHead
//
//  Moves the head to where the energized phases pull it, clamped to the
//  legal range, and puts the engine on that quarter track.
//
//  THE HEAD FOLLOWS THE MAGNETS THAT ARE ON. Phase p's magnets sit at the
//  quarter tracks where qt mod 8 is 2p, so each energized phase pulls the
//  head toward its nearest magnet, up to three quarter tracks either way,
//  and the head comes to rest at the average of the pulls (GetPull). With
//  one phase on, the head rests on that phase's track; with two adjacent
//  phases on, it rests on the quarter track between them, the only way to a
//  quarter track. A phase directly opposite the head pulls neither way, and
//  with none on, or with two opposite ones, the head does not move. A phase
//  turned off can move the head as well as one turned on: a DOS seek turns
//  the next phase on before it turns the last one off, so the head stops on
//  the quarter track between, and the last one going off finishes the
//  half-track step.
//
//  A pull past a travel stop is a bump: the head stays at the stop. Each
//  arrival at a quarter track counts once, for the inspector's Reads marks.
//
////////////////////////////////////////////////////////////////////////////////

Disk2HeadMove Disk2Drive::MoveHead (uint8_t phases)
{
    Disk2HeadMove  move;
    int            target = 0;



    move.from      = m_quarterTrack;
    target         = m_quarterTrack + GetPull (phases, m_quarterTrack);
    move.isBumped  = target < 0 || target > kMaxQuarterTrack;
    move.to        = std::clamp (target, 0, kMaxQuarterTrack);
    m_quarterTrack = move.to;

    m_engine.SetCurrentTrack (m_quarterTrack);

    if (move.IsMoved())
    {
        m_visits[(size_t) m_quarterTrack]++;
    }

    return move;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::SetQuarterTrack
//
//  Puts the head on a quarter track with no move: for a reset or a load.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::SetQuarterTrack (int quarterTrack)
{
    m_quarterTrack = quarterTrack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::GetPull
//
//  How far, in quarter tracks, the energized phases move a head at
//  quarterTrack: the average of each phase's distance to its nearest
//  magnet, rounded to the nearest quarter track. A phase four quarter tracks
//  away, directly opposite, is left out, since it pulls neither way.
//
////////////////////////////////////////////////////////////////////////////////

int Disk2Drive::GetPull (uint8_t phases, int quarterTrack)
{
    constexpr int  kMagnetCycle = kPhaseCount * kQuarterTracksPerHalfTrack;
    constexpr int  kOpposite    = kMagnetCycle / 2;
    int            sum          = 0;
    int            count        = 0;
    int            distance     = 0;
    int            phase        = 0;



    for (phase = 0; phase < kPhaseCount; phase++)
    {
        if ((phases & (1 << phase)) == 0)
        {
            continue;
        }

        distance = ((phase * kQuarterTracksPerHalfTrack - quarterTrack) % kMagnetCycle + kMagnetCycle) % kMagnetCycle;

        if (distance == kOpposite)
        {
            continue;
        }

        sum += (distance > kOpposite) ? distance - kMagnetCycle : distance;
        count++;
    }

    if (count == 0)
    {
        return 0;
    }

    return (2 * sum + ((sum >= 0) ? count : -count)) / (2 * count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::Mount
//
//  Loads a file into the drive's disk and puts that disk in the drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Drive::Mount (const string & path)
{
    HRESULT  hr = S_OK;



    hr = m_disk.Load (path);
    CHR (hr);

    m_active = &m_disk;
    m_engine.SetDiskImage (m_active);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::Eject
//
//  Empties the drive's disk and puts it in the drive.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::Eject()
{
    m_disk.Eject();
    m_active = &m_disk;
    m_engine.SetDiskImage (m_active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::SetExternalDisk
//
//  Puts a disk another object has in the drive, or the drive's empty one
//  with nullptr. Every call is a bay change: another disk, an eject, or the same
//  file reloaded into the same image object at the same address. Each
//  starts the counts over.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::SetExternalDisk (DiskImage * external)
{
    m_visits.fill (0);
    m_access.Clear();
    m_isWriteBlocked = false;

    m_active = (external != nullptr) ? external : &m_disk;
    m_engine.SetDiskImage (m_active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::NoteRead
//
//  A nibble the guest took from the disk, for the access log.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::NoteRead (Byte nibble)
{
    m_access.ObserveRead (m_quarterTrack,
                          nibble,
                          static_cast<uint32_t> (m_engine.GetBitPosition()),
                          static_cast<uint32_t> (m_engine.GetCurrentTrackBits()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::LoadWriteLatch
//
//  The guest loaded the write latch. With the motor running, spin-down
//  included, the nibble goes to the track under the head; with it stopped
//  nothing is written to the disk.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::LoadWriteLatch (Byte nibble, bool isMotorOn)
{
    m_engine.WriteLatch (nibble);

    if (isMotorOn)
    {
        CountLatchLoad();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::CountLatchLoad
//
//  A nibble loaded with the motor running counts on the track under the
//  head; on a disk that cannot be written it goes nowhere, counts nothing,
//  and IsWriteBlocked returns true.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::CountLatchLoad()
{
    if (!m_active->IsLoaded())
    {
        return;
    }

    m_isWriteBlocked = m_active->IsWriteProtected();

    if (!m_isWriteBlocked)
    {
        m_active->CountGuestWrite (m_engine.GetSlot());
        m_access.ObserveWrite (m_quarterTrack, static_cast<uint32_t> (m_engine.GetBitPosition()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::Reset
//
//  The head to track 0 and the engine to rest, with the disk left in.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Drive::Reset()
{
    m_quarterTrack = 0;
    m_engine.Reset();
    m_engine.SetDiskImage (m_active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive::GetVisitCount
//
//  How many times the head arrived at a quarter track since the disk went
//  in.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Disk2Drive::GetVisitCount (int quarterTrack) const
{
    bool  isIn = quarterTrack >= 0 && quarterTrack <= kMaxQuarterTrack;



    return isIn ? m_visits[(size_t) quarterTrack] : 0;
}
