#include "Pch.h"

#include "Debugger/Reverse/HeldInputWatch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputLines::IsEmpty
//
////////////////////////////////////////////////////////////////////////////////

bool HeldInputLines::IsEmpty() const
{
    HeldInputLines  none;



    return *this == none;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::Publish
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::Publish (const HeldInputLines & lines)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    m_published = lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::Refresh
//
////////////////////////////////////////////////////////////////////////////////

bool HeldInputWatch::Refresh()
{
    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_lines = m_published;
    }

    m_hasHit      = false;
    m_hitPosition = 0;

    return !m_lines.IsEmpty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::ReportDifference
//
//  The position read is the instruction making the read: the machine counts
//  an instruction only once it has finished.
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::ReportDifference()
{
    if (m_hasHit)
    {
        return;
    }

    m_hasHit      = true;
    m_hitPosition = (m_positionSource != nullptr) ? *m_positionSource : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::CheckLatch
//
//  A $C000 read. A held press leaves its key waiting, strobe set, so the read
//  differs unless the recording has that very key waiting there already.
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::CheckLatch (Byte latch)
{
    if (m_lines.keyLatch.has_value() && *m_lines.keyLatch != latch)
    {
        ReportDifference();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::CheckKeyDown
//
//  A $C010 read, whose bit 7 is any-key-down: it differs while the host holds
//  a key down that the recording has up.
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::CheckKeyDown (bool isDown)
{
    if (m_lines.isKeyDown && !isDown)
    {
        ReportDifference();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::CheckButton
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::CheckButton (
    size_t  index,
    bool    isRecordedDown)
{
    bool  isHeld = index < m_lines.buttons.size() && m_lines.buttons[index].has_value();



    if (isHeld && *m_lines.buttons[index] != isRecordedDown)
    {
        ReportDifference();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::CheckPaddle
//
//  The read's bit 7 is whether the axis's one-shot still runs: elapsed cycles
//  since the trigger against the position's span. Two positions read alike
//  while both one-shots run and once both have run out; the read differs
//  between the two spans.
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::CheckPaddle (
    size_t    axis,
    Byte      recordedPosition,
    uint64_t  elapsedCycles,
    uint64_t  cyclesPerUnit)
{
    bool  isHeld        = axis < m_lines.paddles.size() && m_lines.paddles[axis].has_value();
    bool  isRecordedOn  = false;
    bool  isHeldOn      = false;



    if (!isHeld)
    {
        return;
    }

    isRecordedOn = elapsedCycles < static_cast<uint64_t> (recordedPosition) * cyclesPerUnit;
    isHeldOn     = elapsedCycles < static_cast<uint64_t> (*m_lines.paddles[axis]) * cyclesPerUnit;

    if (isRecordedOn != isHeldOn)
    {
        ReportDifference();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::CheckJackSwitch
//
//  The Joyport answers a button read with one switch of the jack its
//  annunciators select; only that switch of the held jack is compared.
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::CheckJackSwitch (
    size_t  jack,
    size_t  switchIndex,
    bool    isRecordedClosed)
{
    bool  isHeld = jack < m_lines.jacks.size() && m_lines.jacks[jack].has_value() && switchIndex < m_lines.jacks[jack]->size();



    if (isHeld && m_lines.jacks[jack]->test (switchIndex) != isRecordedClosed)
    {
        ReportDifference();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeldInputWatch::CheckMouseButton
//
////////////////////////////////////////////////////////////////////////////////

void HeldInputWatch::CheckMouseButton (bool isRecordedDown)
{
    if (m_lines.mouseButton.has_value() && *m_lines.mouseButton != isRecordedDown)
    {
        ReportDifference();
    }
}




