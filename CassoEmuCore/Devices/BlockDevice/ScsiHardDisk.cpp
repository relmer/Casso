#include "Pch.h"

#include "Devices/BlockDevice/ScsiHardDisk.h"




//  Command opcodes.
static constexpr Byte  s_kTestUnitReady  = 0x00;
static constexpr Byte  s_kRezeroUnit     = 0x01;
static constexpr Byte  s_kRequestSense   = 0x03;
static constexpr Byte  s_kFormatUnit     = 0x04;
static constexpr Byte  s_kRead6          = 0x08;
static constexpr Byte  s_kWrite6         = 0x0A;
static constexpr Byte  s_kSeek6          = 0x0B;
static constexpr Byte  s_kInquiry        = 0x12;
static constexpr Byte  s_kModeSelect6    = 0x15;
static constexpr Byte  s_kModeSense6     = 0x1A;
static constexpr Byte  s_kStartStopUnit  = 0x1B;
static constexpr Byte  s_kPreventAllow   = 0x1E;
static constexpr Byte  s_kReadCapacity   = 0x25;
static constexpr Byte  s_kRead10         = 0x28;
static constexpr Byte  s_kWrite10        = 0x2A;
static constexpr Byte  s_kSeek10         = 0x2B;
static constexpr Byte  s_kVerify10       = 0x2F;

//  INQUIRY's fixed data: a direct-access device, not removable, SCSI-1,
//  then the vendor, product and revision.
static constexpr Byte  s_kInquiryData[] =
{
    0x00, 0x00, 0x01, 0x01, 31, 0x00, 0x00, 0x00,
    'A', 'P', 'P', 'L', 'E', ' ', ' ', ' ',
    'H', 'D', '2', '0', 'S', 'C', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '1', '.', '0', ' ',
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiHardDisk
//
////////////////////////////////////////////////////////////////////////////////

ScsiHardDisk::ScsiHardDisk (int scsiId, int slot) :
    m_scsiId (scsiId),
    m_slot   (slot)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginCommand
//
//  A failed command sets the sense data and returns CHECK CONDITION; the
//  next REQUEST SENSE returns the sense and clears it.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::BeginCommand (std::span<const Byte> cdb, uint64_t now, ScsiCommandResult & outResult)
{
    Byte      opcode   = cdb.empty() ? (Byte) 0xFF : cdb[0];
    bool      isSense  = opcode == s_kRequestSense || opcode == s_kInquiry;
    uint32_t  lba      = 0;
    uint32_t  count    = 0;



    outResult = {};
    m_commandLog.push_back (opcode);
    m_lastActivity.store (now == 0 ? 1 : now, std::memory_order_relaxed);

    if (!isSense && !IsSpunUp (now))
    {
        Fail (outResult, kSenseNotReady, kAscNotReady);
        Emit (BlockDeviceOp::Status, 0, 0, kSenseNotReady, now);
        return;
    }

    if (opcode != s_kWrite6 && opcode != s_kWrite10)
    {
        FlushWrites();
    }

    switch (opcode)
    {
        case s_kTestUnitReady:
        case s_kRezeroUnit:
        case s_kFormatUnit:
        case s_kSeek6:
        case s_kSeek10:
        case s_kStartStopUnit:
        case s_kPreventAllow:
        case s_kVerify10:
            Emit (BlockDeviceOp::Status, 0, 0, 0, now);
            break;

        case s_kRequestSense:
            RequestSense (cdb, outResult);
            break;

        case s_kInquiry:
            Inquiry (cdb, outResult);
            break;

        case s_kModeSense6:
            ModeSense (cdb, outResult);
            break;

        case s_kModeSelect6:
            outResult.dataOutLength = cdb[4];
            m_writeCount            = 0;
            break;

        case s_kReadCapacity:
            ReadCapacity (outResult);
            break;

        case s_kRead6:
        case s_kWrite6:
            lba   = ((uint32_t) (cdb[1] & 0x1F) << 16) | ((uint32_t) cdb[2] << 8) | cdb[3];
            count = (cdb[4] == 0) ? 256u : cdb[4];

            if (opcode == s_kRead6)
            {
                Read (lba, count, now, outResult);
            }
            else
            {
                BeginWrite (lba, count, outResult);
            }

            break;

        case s_kRead10:
        case s_kWrite10:
            lba   = ((uint32_t) cdb[2] << 24) | ((uint32_t) cdb[3] << 16) | ((uint32_t) cdb[4] << 8) | cdb[5];
            count = ((uint32_t) cdb[7] << 8) | cdb[8];

            if (opcode == s_kRead10)
            {
                Read (lba, count, now, outResult);
            }
            else
            {
                BeginWrite (lba, count, outResult);
            }

            break;

        default:
            Fail (outResult, kSenseIllegalRequest, kAscInvalidOpcode);
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishDataOut
//
//  The data of a WRITE: each block goes to the image. MODE SELECT's
//  parameters are accepted and have no effect.
//
////////////////////////////////////////////////////////////////////////////////

ScsiStatus ScsiHardDisk::FinishDataOut (std::span<const Byte> data, uint64_t now, uint64_t & outDelayCycles)
{
    HRESULT  hr     = S_OK;
    Byte     result = 0;



    outDelayCycles = 0;

    if (m_writeCount == 0)
    {
        return ScsiStatus::Good;
    }

    for (uint32_t i = 0; i < m_writeCount; i++)
    {
        hr = m_media.WriteBlock (m_writeLba + i, data.subspan ((size_t) i * kBlock, kBlock));

        if (FAILED (hr))
        {
            break;
        }
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT))
    {
        SetSense (kSenseDataProtect, kAscWriteProtected);
        result = kSenseDataProtect;
    }
    else if (FAILED (hr))
    {
        SetSense (kSenseMediumError, 0);
        result = kSenseMediumError;
    }

    outDelayCycles = GetAccessDelay();
    Emit (BlockDeviceOp::Write, m_writeLba, m_writeCount, result, now);

    m_writeCount = 0;

    return (result == 0) ? ScsiStatus::Good : ScsiStatus::CheckCondition;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetTarget
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::ResetTarget()
{
    FlushWrites();
    m_senseKey   = kSenseNoSense;
    m_senseAsc   = 0;
    m_writeCount = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRealTiming
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::SetRealTiming (bool isOn, uint32_t cpuClockHz, uint64_t powerOnCycle)
{
    m_realTiming   = isOn;
    m_clockHz      = cpuClockHz;
    m_powerOnCycle = powerOnCycle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModelLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ScsiHardDisk::GetModelLabel() const
{
    constexpr uint32_t  kBlocksPerMb = 2048;
    constexpr uint32_t  kEmptyMb     = 20;
    uint32_t            blocks       = m_media.GetBlockCount();
    uint32_t            megabytes    = m_media.IsAttached() ? (blocks + kBlocksPerMb / 2) / kBlocksPerMb : kEmptyMb;



    return std::format (L"{}SC", std::max (megabytes, 1u));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSense
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::SetSense (Byte key, Byte asc)
{
    m_senseKey = key;
    m_senseAsc = asc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Fail
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::Fail (ScsiCommandResult & outResult, Byte key, Byte asc)
{
    SetSense (key, asc);
    outResult.status = ScsiStatus::CheckCondition;
    outResult.dataIn.clear();
    outResult.dataOutLength = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSpunUp
//
//  Always, with real timing off; otherwise once the spin-up time has passed
//  since power-on.
//
////////////////////////////////////////////////////////////////////////////////

bool ScsiHardDisk::IsSpunUp (uint64_t now) const
{
    constexpr uint64_t  kSpinUpSeconds = 10;



    return !m_realTiming || now >= m_powerOnCycle + kSpinUpSeconds * m_clockHz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAccessDelay
//
//  An average seek and half a revolution at 3,600 RPM, with real timing on.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ScsiHardDisk::GetAccessDelay() const
{
    constexpr uint64_t  kSeekMs        = 65;
    constexpr uint64_t  kHalfTurnMs    = 8;
    constexpr uint64_t  kMsPerSecond   = 1000;



    return m_realTiming ? (kSeekMs + kHalfTurnMs) * m_clockHz / kMsPerSecond : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Emit
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::Emit (BlockDeviceOp op, uint32_t first, uint32_t count, Byte result, uint64_t now)
{
    BlockDeviceEvent  event;



    if (m_sink == nullptr)
    {
        return;
    }

    event.cycle      = now;
    event.kind       = BlockDeviceKind::Scsi;
    event.slot       = (Byte) m_slot;
    event.unit       = (Byte) m_scsiId;
    event.op         = op;
    event.firstBlock = first;
    event.blockCount = count;
    event.result     = result;

    m_sink->OnBlockDeviceEvent (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::Read (uint32_t lba, uint32_t count, uint64_t now, ScsiCommandResult & outResult)
{
    HRESULT   hr      = S_OK;
    bool      inRange = (uint64_t) lba + count <= m_media.GetBlockCount();



    if (!inRange)
    {
        Fail (outResult, kSenseIllegalRequest, kAscLbaOutOfRange);
        Emit (BlockDeviceOp::Read, lba, count, kSenseIllegalRequest, now);
        return;
    }

    outResult.dataIn.resize ((size_t) count * kBlock);

    for (uint32_t i = 0; i < count; i++)
    {
        hr = m_media.ReadBlock (lba + i, std::span<Byte> (outResult.dataIn).subspan ((size_t) i * kBlock, kBlock));

        if (FAILED (hr))
        {
            Fail (outResult, kSenseMediumError, kAscReadError);
            Emit (BlockDeviceOp::Read, lba, count, kSenseMediumError, now);
            return;
        }
    }

    outResult.delayCycles = GetAccessDelay();
    Emit (BlockDeviceOp::Read, lba, count, 0, now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginWrite
//
//  The range is checked before the data arrives; a write-protected drive
//  takes the data and then fails, as a drive does.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::BeginWrite (uint32_t lba, uint32_t count, ScsiCommandResult & outResult)
{
    bool  inRange = (uint64_t) lba + count <= m_media.GetBlockCount();



    if (!inRange)
    {
        Fail (outResult, kSenseIllegalRequest, kAscLbaOutOfRange);
        return;
    }

    m_writeLba              = lba;
    m_writeCount            = count;
    outResult.dataOutLength = (size_t) count * kBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Inquiry
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::Inquiry (std::span<const Byte> cdb, ScsiCommandResult & outResult)
{
    size_t  length = std::min<size_t> (cdb[4], sizeof (s_kInquiryData));



    outResult.dataIn.assign (s_kInquiryData, s_kInquiryData + length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestSense
//
//  Extended sense data, cut to the allocation length; an allocation length
//  of 0 returns four bytes, as SCSI-1 specifies. The sense is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::RequestSense (std::span<const Byte> cdb, ScsiCommandResult & outResult)
{
    constexpr size_t  kSenseBytes      = 18;
    constexpr size_t  kNonExtended     = 4;
    constexpr Byte    kExtendedFormat  = 0x70;
    constexpr Byte    kAdditionalBytes = 10;
    constexpr size_t  kKeyAt           = 2;
    constexpr size_t  kAdditionalAt    = 7;
    constexpr size_t  kAscAt           = 12;
    size_t            length           = (cdb[4] == 0) ? kNonExtended : std::min<size_t> (cdb[4], kSenseBytes);
    std::vector<Byte> sense (kSenseBytes, 0);



    sense[0]             = kExtendedFormat;
    sense[kKeyAt]        = m_senseKey;
    sense[kAdditionalAt] = kAdditionalBytes;
    sense[kAscAt]        = m_senseAsc;

    sense.resize (length);
    outResult.dataIn = std::move (sense);

    SetSense (kSenseNoSense, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ModeSense
//
//  The header and one block descriptor: no pages, the write-protect bit, the
//  block count and the 512-byte block length.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::ModeSense (std::span<const Byte> cdb, ScsiCommandResult & outResult)
{
    constexpr Byte    kWriteProtectBit = 0x80;
    constexpr Byte    kDescriptorBytes = 8;
    uint32_t          blocks           = m_media.GetBlockCount();
    std::vector<Byte> data =
    {
        11, 0x00, (Byte) (m_media.IsWriteProtected() ? kWriteProtectBit : 0), kDescriptorBytes,
        0x00, (Byte) (blocks >> 16), (Byte) (blocks >> 8), (Byte) blocks,
        0x00, 0x00, (Byte) (kBlock >> 8), (Byte) kBlock,
    };



    data.resize (std::min<size_t> (cdb[4], data.size()));
    outResult.dataIn = std::move (data);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCapacity
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::ReadCapacity (ScsiCommandResult & outResult)
{
    uint32_t  last = m_media.GetBlockCount() - 1;



    outResult.dataIn =
    {
        (Byte) (last >> 24), (Byte) (last >> 16), (Byte) (last >> 8), (Byte) last,
        0x00, 0x00, (Byte) (kBlock >> 8), (Byte) kBlock,
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushWrites
//
//  Written blocks go to the file when the next command that is not a write
//  arrives, and at reset, so a run of writes is one flush.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiHardDisk::FlushWrites()
{
    HRESULT  hr = m_media.Flush();



    IGNORE_RETURN_VALUE (hr, S_OK);
}
