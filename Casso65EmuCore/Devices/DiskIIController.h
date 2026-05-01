#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImage
//
////////////////////////////////////////////////////////////////////////////////

class DiskImage
{
public:
    DiskImage ();

    HRESULT Load (const std::string & filePath);
    void    Eject ();
    bool    IsLoaded () const { return m_loaded; }

    void ReadSector  (int track, int logicalSector, Byte * outData) const;
    void WriteSector (int track, int logicalSector, const Byte * data);

    bool IsWriteProtected () const { return m_writeProtected; }
    void SetWriteProtected (bool wp) { m_writeProtected = wp; }

    HRESULT Flush ();

    const std::string & GetFilePath () const { return m_filePath; }

private:
    std::string             m_filePath;
    std::array<Byte, 143360> m_data;
    bool                    m_loaded         = false;
    bool                    m_dirty          = false;
    bool                    m_writeProtected = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIController
//
//  I/O at $C0s0-$C0sF where s = slot.
//  TODO: Slot ROM at $Cs00-$CsFF needs a separate device or dual-range
//  registration for boot firmware (disk2.rom).
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIController : public MemoryDevice
{
public:
    DiskIIController (int slot);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return m_ioStart; }
    Word GetEnd   () const override { return m_ioEnd; }
    void Reset    () override;

    // Disk image management
    HRESULT MountDisk (int drive, const std::string & path);
    void    EjectDisk (int drive);
    DiskImage * GetDisk (int drive);

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void NibblizeTrack (int drive);
    void HandlePhase   (int phase, bool on);
    Byte ReadDataLatch ();

    int     m_slot;
    Word    m_ioStart;
    Word    m_ioEnd;

    // Stepper motor
    uint8_t m_phases       = 0;
    int     m_quarterTrack = 0;

    // Controller state
    bool    m_motorOn      = false;
    int     m_activeDrive  = 0;
    bool    m_q6           = false;
    bool    m_q7           = false;

    // Data path
    Byte    m_shiftRegister = 0;

    // Disk images
    DiskImage m_disks[2];

    // Pre-nibblized current track
    std::vector<Byte>   m_nibbleBuffer;
    size_t              m_nibblePos = 0;
};
