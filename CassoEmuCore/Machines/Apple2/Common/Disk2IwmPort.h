#pragma once

#include "Pch.h"

#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/Iwm.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"

class IDiskPortHook;
class StateReader;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  IDisk2IwmOwner
//
//  The Disk II controller behind a //c's IWM: its clock, its sequencer
//  state, its motor and drives, and where the nibbles go.
//
////////////////////////////////////////////////////////////////////////////////

class IDisk2IwmOwner
{
public:
    virtual ~IDisk2IwmOwner() = default;

    virtual bool  AdvanceIwmClock     (uint64_t & outElapsed)        = 0;
    virtual void  OnIwmSequencer      (int drive, bool q6, bool q7)  = 0;
    virtual void  OnIwmSwitch         (int offset)                   = 0;
    virtual void  OnIwmMotor          (bool isOn)                    = 0;
    virtual void  OnIwmNibbleWritten  (Byte value)                   = 0;
    virtual void  OnIwmNibbleRead     (Byte nibble)                  = 0;
    virtual void  OnIwmPhases         (int drive, uint8_t phases)    = 0;
    virtual void  OnIwmEnable         (int drive, bool isEnabled)    = 0;
    virtual Byte  StepIwmCell         (int drive, bool isWrite, Byte bit) = 0;
    virtual bool  IsIwmWriteProtected (int drive) const              = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2IwmPort
//
//  The //c's disk port, an Integrated Woz Machine in place of a Disk II
//  card: the switches go to an Iwm, whose two drives are the controller's
//  drives. Their phases step the drive's head, their cells are the drive's
//  engine, and their enables run the motor.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2IwmPort
{
public:
    //  The IWM's time is the CPU's cycles at a thousand nanoseconds a cycle,
    //  near enough to real time at the //c's 1.02 MHz for its one-second
    //  motor timer. A cell is four cycles, as a Disk II card's is.
    static constexpr uint64_t  kNsPerCycle = 1000;
    static constexpr uint32_t  kCellNs     = Disk2NibbleEngine::kCyclesPerBit * kNsPerCycle;

    explicit Disk2IwmPort (IDisk2IwmOwner & owner);

    Disk2IwmPort (const Disk2IwmPort &)             = delete;
    Disk2IwmPort & operator= (const Disk2IwmPort &) = delete;

    Byte     Access    (int offset, bool isWrite, Byte value, IDiskPortHook * hook);
    bool     TryPeek   (int offset, const IDiskPortHook * hook, Byte & value) const;
    void     Tick      (uint32_t cpuCycles, bool isCounted);
    void     Reset     ();
    void     SaveState (StateWriter & writer) const;
    HRESULT  LoadState (StateReader & reader);

    Iwm        & GetIwm ()       { return m_iwm; }
    const Iwm  & GetIwm () const { return m_iwm; }

private:
    static constexpr int  kFirstSelectSwitch = 0xA;

    class Drive : public IDiskDriveBus
    {
    public:
        Drive (Disk2IwmPort & port, int drive) : m_port (port), m_drive (drive) {}

        void      SetPhases  (bool ph0, bool ph1, bool ph2, bool ph3) override;
        bool      GetSense   () const override;
        Byte      ReadCell   (uint64_t cellEndNs) override;
        void      WriteCell  (Byte bit, uint64_t cellEndNs) override;
        void      Advance    (uint64_t elapsedNs) override { (void) elapsedNs; }
        uint32_t  GetCellNs  () const override { return kCellNs; }
        void      SetEnabled (bool isEnabled) override;

    private:
        Disk2IwmPort  & m_port;
        int             m_drive = 0;
    };

    uint64_t  GetNow    ();
    void      SyncMotor ();

    IDisk2IwmOwner        & m_owner;
    Iwm                     m_iwm;
    std::array<Drive, 2>    m_drives;
    std::array<bool, 2>     m_enabled = {};
    uint64_t                m_cycles  = 0;
};
