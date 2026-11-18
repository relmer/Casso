#pragma once

#include "Pch.h"

#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"

class Disk2Drive;
class Disk2Lines;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmDriveBus
//
//  A Disk II drive on the //c's IWM: its phase lines step the drive's head
//  through the controller's lines, its cells are the drive's engine, and its
//  enable runs the engine. A cell is four CPU cycles, as on a Disk II card.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2IwmDriveBus : public IDiskDriveBus
{
public:
    //  The IWM's time is the CPU's cycles at a thousand nanoseconds a cycle,
    //  near enough to real time at the //c's 1.02 MHz for its one-second
    //  motor timer.
    static constexpr uint64_t  kNsPerCycle = 1000;
    static constexpr uint32_t  kCellNs     = Disk2NibbleEngine::kCyclesPerBit * kNsPerCycle;

    Disk2IwmDriveBus (Disk2Lines & lines, Disk2Drive & drive, int index);

    void      SetPhases  (bool ph0, bool ph1, bool ph2, bool ph3) override;
    bool      GetSense   () const override;
    Byte      ReadCell   (uint64_t cellEndNs) override;
    void      WriteCell  (Byte bit, uint64_t cellEndNs) override;
    void      Advance    (uint64_t elapsedNs) override { (void) elapsedNs; }
    uint32_t  GetCellNs  () const override { return kCellNs; }
    void      SetEnabled (bool isEnabled) override;

    bool      IsEnabled      () const { return m_isEnabled; }
    void      RestoreEnabled (bool isEnabled) { m_isEnabled = isEnabled; }

private:
    Disk2Lines  & m_lines;
    Disk2Drive  & m_drive;
    int           m_index     = 0;
    bool          m_isEnabled = false;
};
