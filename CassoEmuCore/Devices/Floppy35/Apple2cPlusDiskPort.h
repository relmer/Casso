#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Floppy35/ICatchUpDevice.h"
#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/SmartPortHost.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Machines/Apple2/Common/IDiskPortHook.h"

class MigChip;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusDiskPort
//
//  The //c Plus's IWM at $C0E0-$C0EF, its internal Apple 3.5 Drive, and the
//  disk port's chain. With drive 2 selected and the MIG's internal drive
//  on, the IWM drives the internal drive. With the MIG's 3.5 select on, IWM
//  drive 1 is the first external Apple 3.5 Drive at the start of the chain
//  and drive 2 the second, when the chain has them. Otherwise the IWM
//  drives the SmartPort bus. Each 3.5-inch drive's head is the MIG's head
//  select. The phases go only to the one the IWM drives, so a 3.5-inch
//  drive's register selects are never on the bus.
//
//  With 5.25-inch drives at the end of the chain, a Disk II controller in
//  IWM mode is at $C0E0 in place of this class, and this class is its port
//  hook: the 5.25-inch drives are enabled while the selected drive is one of
//  them and not the internal drive, and the SmartPort bus is neither enabled
//  nor in reset.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cPlusDiskPort : public MemoryDevice, public IDiskPortHook, public IFloppy35EventSource, public IMachineState, public IReplayMediaSource, public ICatchUpDevice
{
public:
    static constexpr Word  kIoStart     = 0xC0E0;
    static constexpr Word  kIoEnd       = 0xC0EF;
    static constexpr int   kMaxExternal = 2;

    Apple2cPlusDiskPort();
    ~Apple2cPlusDiskPort() override;

    // MemoryDevice
    Byte  Read      (Word address) override;
    void  Write     (Word address, Byte value) override;
    bool  TryPeekIo (Word address, Byte & value) override;
    Word  GetStart  () const override { return kIoStart; }
    Word  GetEnd    () const override { return kIoEnd; }
    void  Reset     () override { m_port.Reset(); }

    // IDiskPortHook
    bool  OnIwmAccess         (int offset, bool isWrite, Byte value, Byte & outValue) override;
    bool  AreFloppiesEnabled  () override;
    bool  PeekFloppiesEnabled () const override;
    bool  TryPeekIwmAccess    (int offset, bool & outIsPorts, Byte & outValue) const override;

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override;

    //  The MIG that switches the internal drive, which this class deletes.
    void             AdoptMig         (std::unique_ptr<MigChip> mig);
    MigChip        * GetMig           () const { return m_mig; }
    void             ResetIwm         ()       { m_port.GetIwm().Reset(); }
    Sony35Drive    & GetInternalDrive ()       { return m_internal; }
    SmartPortHost  & GetPort          ()       { return m_port; }

    void  CatchUp (CatchUpKind kind) override { m_port.CatchUp (kind); }

    //  The 5.25-inch drives at the end of the chain, drive 1 first.
    void             SetFloppyCount   (int count) { m_floppyCount = count; }
    int              GetFloppyCount   () const    { return m_floppyCount; }

    //  The Apple 3.5 Drives at the start of the chain, drive 1 first.
    void             AddExternalDrive ()          { m_externalCount = std::min (m_externalCount + 1, kMaxExternal); }
    int              GetExternalCount () const    { return m_externalCount; }
    Sony35Drive    & GetExternalDrive (int index) { return m_externals[(size_t) index]; }

    //  IMachineState: the MIG, the SmartPort chain, the internal drive and
    //  the external 3.5-inch drives, the phases and the drive enables.
    HRESULT  SaveState      (StateWriter & writer) const override;
    HRESULT  LoadState      (StateReader & reader) override;
    void     GetReplayMedia (std::vector<IReplayMedia *> & outDrives) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('C', '+', 'D', 'P');
    static constexpr uint16_t  kStateVersion = 1;

private:
    static constexpr Word  kSwitchMask    = 0x0F;
    static constexpr int   kInternalDrive = 1;

    //  One of the IWM's two drives. After a switch between the 3.5-inch
    //  drive and the bus, the newly driven one is set to the IWM's phases.
    class PortDrive : public IDiskDriveBus
    {
    public:
        PortDrive (Apple2cPlusDiskPort & diskPort, int drive) : m_diskPort (diskPort), m_drive (drive) {}

        void      SetPhases     (bool ph0, bool ph1, bool ph2, bool ph3) override;
        bool      GetSense      () const override;
        Byte      ReadCell      (uint64_t cellEndNs) override;
        void      WriteCell     (Byte bit, uint64_t cellEndNs) override;
        void      Advance       (uint64_t elapsedNs) override;
        uint32_t  GetCellNs     () const override;
        uint64_t  GetQuietUntil () const override;
        void      SetEnabled    (bool isEnabled) override;

        //  The drive the phases were last set on, as an ID in the state
        //  section: none, the internal drive, an external drive or the chain.
        Byte     GetDrivenId () const;
        HRESULT  SetDrivenId (Byte id);

    private:
        IDiskDriveBus  & Sync ();

        static constexpr Byte  kDrivenNone     = 0;
        static constexpr Byte  kDrivenInternal = 1;
        static constexpr Byte  kDrivenExternal = 2;
        static constexpr Byte  kDrivenChain    = 3;

        Apple2cPlusDiskPort  & m_diskPort;
        int                    m_drive  = 0;
        IDiskDriveBus        * m_driven = nullptr;
    };

    bool             IsInternal   () const;
    bool             IsDrivingBus (int drive) const;
    IDiskDriveBus  & GetDriven    (int drive);
    void             SyncHeads    ();
    void             SyncEnables  ();

    SmartPortHost                          m_port;
    Sony35Drive                            m_internal      { 0 };
    std::array<Sony35Drive, kMaxExternal>  m_externals     { Sony35Drive { 1 }, Sony35Drive { 2 } };
    std::array<PortDrive, Iwm::kDrives>    m_drives        { PortDrive { *this, 0 }, PortDrive { *this, 1 } };
    std::unique_ptr<MigChip>               m_ownedMig;
    MigChip                              * m_mig           = nullptr;
    std::array<bool, 4>                    m_phases        = {};
    std::array<bool, Iwm::kDrives>         m_iwmEnabled    = {};
    int                                    m_externalCount = 0;
    int                                    m_floppyCount   = 0;
    bool                                   m_floppies      = false;
};
