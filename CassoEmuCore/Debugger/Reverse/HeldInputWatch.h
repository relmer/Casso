#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputLines
//
//  What the host input held back behind live would put on the lines the
//  guest reads, line by line. A line with no value is not held: the guest
//  reads it as the recording has it, and nothing about it is in question.
//
//  The key latch keeps its strobe bit set, since a held press leaves a key
//  waiting until the guest reads it, however long ago the key went up. The
//  mouse target is the host pointer's viewport fraction, packed as the
//  guest mouse holds it (x in the high half).
//
////////////////////////////////////////////////////////////////////////////////

struct HeldInputLines
{
    static constexpr size_t  kButtonCount = GamePortContribution::kButtonCount;
    static constexpr size_t  kAxisCount   = GamePortContribution::kAxisCount;
    static constexpr size_t  kJackCount   = JoyportJacks::kJackCount;

    std::optional<Byte>                                       keyLatch;
    bool                                                      isKeyDown = false;
    std::array<std::optional<bool>, kButtonCount>             buttons;
    std::array<std::optional<Byte>, kAxisCount>               paddles;
    std::array<std::optional<JoystickSwitches>, kJackCount>   jacks;
    std::optional<bool>                                       mouseButton;
    std::optional<uint32_t>                                   mouseTarget;

    bool  IsEmpty() const;

    bool operator== (const HeldInputLines &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch
//
//  Behind live, host input is held back rather than asked about at once, and
//  the question waits until the guest actually reads a line the held input
//  would change. The UI thread publishes what is held; the CPU thread takes
//  it at the start of each slice it runs behind live, attaches the watch to
//  the devices for that slice only, and the devices report a read whose
//  result the held input would change.
//
//  The first such read is kept, with the position of the instruction that
//  made it; the run ends after that instruction, and the machine is put back
//  where the instruction began before the user is asked. Later reads in the
//  same slice are ignored.
//
//  Publish and Refresh cross threads under a lock; everything else is the
//  CPU thread's.
//
////////////////////////////////////////////////////////////////////////////////

class HeldInputWatch
{
public:
    //  UI thread: what is held now. Empty lines take the watch down.
    void  Publish (const HeldInputLines & lines);

    //  CPU thread, before a slice: takes the lines last published and forgets
    //  any read seen in an earlier slice. True when anything is held.
    bool  Refresh ();

    void  SetPositionSource (const uint64_t * position) { m_positionSource = position; }

    //  CPU thread, devices: the lines as taken, and a read the held input
    //  would change. Each check compares what the read returned from the
    //  recording with what it would return with the held line in its place,
    //  and reports a difference; a line not held is never one.
    const HeldInputLines  & GetLines         () const { return m_lines; }
    void                    ReportDifference ();
    void                    CheckLatch       (Byte latch);
    void                    CheckKeyDown     (bool isDown);
    void                    CheckButton      (size_t index, bool isRecordedDown);
    void                    CheckPaddle      (size_t axis, Byte recordedPosition, uint64_t elapsedCycles, uint64_t cyclesPerUnit);
    void                    CheckJackSwitch  (size_t jack, size_t switchIndex, bool isRecordedClosed);
    void                    CheckMouseButton (bool isRecordedDown);

    bool      HasHit         () const { return m_hasHit; }
    uint64_t  GetHitPosition () const { return m_hitPosition; }
    void      ClearHit       ()       { m_hasHit = false; }

private:
    std::mutex          m_mutex;
    HeldInputLines      m_published;
    HeldInputLines      m_lines;
    const uint64_t    * m_positionSource = nullptr;
    bool                m_hasHit         = false;
    uint64_t            m_hitPosition    = 0;
};
