#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"
#include "Devices/BlockDevice/IScsiTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiHardDisk
//
//  An Apple Hard Disk 20SC, or any SCSI hard disk, over an image. It answers
//  TEST UNIT READY, REQUEST SENSE, INQUIRY, READ and WRITE in their 6- and
//  10-byte forms, READ CAPACITY, MODE SENSE, MODE SELECT, START STOP UNIT,
//  VERIFY, SEEK, REZERO UNIT, FORMAT UNIT and PREVENT ALLOW MEDIUM REMOVAL. A
//  drive with no image does not answer selection.
//
//  With real timing off, the drive is ready at power-on and each command
//  completes at once. With it on, the drive reports NOT READY until it has
//  spun up, and each read or write waits for an average seek and half a
//  revolution.
//
////////////////////////////////////////////////////////////////////////////////

class ScsiHardDisk : public IScsiTarget, public IBlockDeviceEventSource
{
public:
    static constexpr Byte  kSenseNoSense        = 0x00;
    static constexpr Byte  kSenseNotReady       = 0x02;
    static constexpr Byte  kSenseMediumError    = 0x03;
    static constexpr Byte  kSenseIllegalRequest = 0x05;
    static constexpr Byte  kSenseUnitAttention  = 0x06;
    static constexpr Byte  kSenseDataProtect    = 0x07;

    static constexpr Byte  kAscInvalidOpcode    = 0x20;
    static constexpr Byte  kAscLbaOutOfRange    = 0x21;
    static constexpr Byte  kAscWriteProtected   = 0x27;
    static constexpr Byte  kAscNotReady         = 0x04;
    static constexpr Byte  kAscReadError        = 0x11;

    ScsiHardDisk (int scsiId, int slot);

    // IScsiTarget
    int         GetScsiId     () const override { return m_scsiId; }
    bool        IsSelectable  () const override { return m_media.IsAttached(); }
    void        BeginCommand  (std::span<const Byte> cdb, uint64_t now, ScsiCommandResult & outResult) override;
    ScsiStatus  FinishDataOut (std::span<const Byte> data, uint64_t now, uint64_t & outDelayCycles) override;
    void        ResetTarget   () override;

    // IBlockDeviceEventSource
    void  SetBlockDeviceEventSink (IBlockDeviceEventSink * sink) override { m_sink = sink; }

    BlockDeviceMedia        & GetMedia ()       { return m_media; }
    const BlockDeviceMedia  & GetMedia () const { return m_media; }

    //  Real timing at the CPU's clock rate. `powerOnCycle` is when the drive
    //  starts spinning up.
    void  SetRealTiming (bool isOn, uint32_t cpuClockHz, uint64_t powerOnCycle);
    bool  IsRealTiming  () const { return m_realTiming; }

    //  "20SC" for an empty drive; otherwise the image's size in MB, rounded,
    //  then "SC".
    std::wstring  GetModelLabel () const;

    Byte  GetSenseKey () const { return m_senseKey; }

    //  The CPU cycle of the last command, for the drive's lamp; read on the
    //  UI thread.
    uint64_t  GetLastActivityCycle () const { return m_lastActivity.load (std::memory_order_relaxed); }

    //  The commands received since the drive was made, for a log.
    const std::vector<Byte> & GetCommandLog () const { return m_commandLog; }

private:
    static constexpr size_t  kBlock = BlockDeviceMedia::kBlockBytes;

    void      SetSense        (Byte key, Byte asc);
    void      Fail            (ScsiCommandResult & outResult, Byte key, Byte asc);
    bool      IsSpunUp        (uint64_t now) const;
    uint64_t  GetAccessDelay  () const;
    void      Emit            (BlockDeviceOp op, uint32_t first, uint32_t count, Byte result, uint64_t now);
    void      Read            (uint32_t lba, uint32_t count, uint64_t now, ScsiCommandResult & outResult);
    void      BeginWrite      (uint32_t lba, uint32_t count, ScsiCommandResult & outResult);
    void      Inquiry         (std::span<const Byte> cdb, ScsiCommandResult & outResult);
    void      RequestSense    (std::span<const Byte> cdb, ScsiCommandResult & outResult);
    void      ModeSense       (std::span<const Byte> cdb, ScsiCommandResult & outResult);
    void      ReadCapacity    (ScsiCommandResult & outResult);
    void      FlushWrites     ();

    int                       m_scsiId        = 0;
    int                       m_slot          = 0;
    BlockDeviceMedia          m_media;
    IBlockDeviceEventSink   * m_sink          = nullptr;
    bool                      m_realTiming    = false;
    uint32_t                  m_clockHz       = 0;
    uint64_t                  m_powerOnCycle  = 0;
    Byte                      m_senseKey      = 0;
    Byte                      m_senseAsc      = 0;
    uint32_t                  m_writeLba      = 0;
    uint32_t                  m_writeCount    = 0;
    std::vector<Byte>         m_commandLog;
    std::atomic<uint64_t>     m_lastActivity  { 0 };
};
