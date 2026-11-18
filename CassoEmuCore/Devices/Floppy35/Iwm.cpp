#include "Pch.h"

#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/Iwm.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"




static constexpr int   s_kSwitchEnable = 4;
static constexpr int   s_kSwitchSelect = 5;
static constexpr int   s_kSwitchQ6     = 6;
static constexpr int   s_kSwitchQ7     = 7;
static constexpr Byte  s_kSenseBit     = 0x80;
static constexpr Byte  s_kEnableBit    = 0x20;
static constexpr Byte  s_kEmptyBit     = 0x80;
static constexpr Byte  s_kNoUnderrun   = 0x40;
static constexpr Byte  s_kLowBits      = 0x3F;
static constexpr int   s_kIsmReadBit   = 0x08;
static constexpr int   s_kIsmRegMask   = 0x07;
static constexpr int   s_kIsmParam     = 3;
static constexpr int   s_kIsmPhase     = 4;
static constexpr int   s_kIsmModeRead  = 6;
static constexpr Byte  s_kTopBit       = 0x80;





////////////////////////////////////////////////////////////////////////////////
//
//  Access
//
//  The bits catch up to the access, the switch changes, then the register
//  Q6 and Q7 select is read or written.
//
////////////////////////////////////////////////////////////////////////////////

Byte Iwm::Access (int offset, bool isWrite, Byte value, uint64_t nowNs)
{
    Byte  result = 0xFF;



    AdvanceTo (nowNs);

    if (m_ism)
    {
        return AccessIsm (offset, isWrite, value);
    }

    SetSwitch (offset, nowNs);

    if (isWrite)
    {
        if (m_q6 && m_q7)
        {
            WriteRegister (value);
        }
    }
    else
    {
        result = ReadRegister();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessIsm
//
//  The SWIM's ISM registers. Registers this model doesn't implement read
//  as zero and ignore writes. A read of the parameter RAM advances its
//  counter.
//
////////////////////////////////////////////////////////////////////////////////

Byte Iwm::AccessIsm (int offset, bool isWrite, Byte value)
{
    constexpr int   kModeZeros   = 6;
    constexpr int   kModeOnes    = 7;
    constexpr Byte  kLevelMask   = 0x0F;
    constexpr int   kOutputShift = 4;
    int             reg          = offset & s_kIsmRegMask;
    bool            isRead       = (offset & s_kIsmReadBit) != 0;
    Byte            result       = 0;
    Byte            levels       = 0;



    if (isRead && !isWrite)
    {
        result = ReadIsmRegister (reg);

        if (reg == s_kIsmParam)
        {
            m_paramIndex = (m_paramIndex + 1) % kParamBytes;
        }

        return result;
    }

    if (isRead)
    {
        return result;
    }

    if (reg == s_kIsmParam)
    {
        m_param[m_paramIndex] = value;
        m_paramIndex          = (m_paramIndex + 1) % kParamBytes;
    }
    else if (reg == s_kIsmPhase)
    {
        m_ismPhase = value;
        levels     = (Byte) (value & (value >> kOutputShift) & kLevelMask);

        for (size_t i = 0; i < m_phases.size(); i++)
        {
            m_phases[i] = ((levels >> i) & 1) != 0;
        }

        UpdatePhases();
    }
    else if (reg == kModeZeros || reg == kModeOnes)
    {
        m_ismMode    = (reg == kModeOnes) ? (Byte) (m_ismMode | value) : (Byte) (m_ismMode & ~value);
        m_paramIndex = 0;
        m_ism        = (m_ismMode & kIsmBit) != 0;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadIsmRegister
//
//  An ISM register's byte, without the read's change to the parameter
//  counter.
//
////////////////////////////////////////////////////////////////////////////////

Byte Iwm::ReadIsmRegister (int reg) const
{
    return (reg == s_kIsmParam)    ? m_param[m_paramIndex]
         : (reg == s_kIsmPhase)    ? m_ismPhase
         : (reg == s_kIsmModeRead) ? m_ismMode
         :                           (Byte) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSwitches
//
////////////////////////////////////////////////////////////////////////////////

Iwm::SwitchState Iwm::GetSwitches() const
{
    SwitchState  state;



    state.q6           = m_q6;
    state.q7           = m_q7;
    state.isEnabled    = m_enabled;
    state.isOffPending = m_motorOffPending;
    state.drive        = m_driveSelect;
    state.phases       = m_phases;

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSwitchesAfter
//
//  The switch at an offset applied to a copy of the switches. Turning the
//  enable off waits a second unless the mode's timer bit is set. In the
//  SWIM's ISM mode no switch moves.
//
////////////////////////////////////////////////////////////////////////////////

Iwm::SwitchState Iwm::GetSwitchesAfter (int offset) const
{
    int          sw     = (offset >> 1) & 7;
    bool         on     = (offset & 1) != 0;
    SwitchState  state  = GetSwitches();



    if (m_ism)
    {
        return state;
    }

    if (sw < s_kSwitchEnable)
    {
        state.phases[(size_t) sw] = on;
        state.isDriveMoved        = m_phases[(size_t) sw] != on;
    }
    else if (sw == s_kSwitchEnable)
    {
        if (on)
        {
            state.isEnabled    = true;
            state.isOffPending = false;
        }
        else if (m_enabled && (m_mode & kModeNoTimer) == 0)
        {
            state.isOffPending   = true;
            state.isTimerStarted = true;
        }
        else
        {
            state.isEnabled = false;
        }
    }
    else if (sw == s_kSwitchSelect)
    {
        state.drive = on ? 1 : 0;
    }
    else if (sw == s_kSwitchQ6)
    {
        state.q6 = on;
    }
    else
    {
        state.q7 = on;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekAccess
//
//  The register Q6 and Q7 select after the switch, from the same reads an
//  access makes, with their changes discarded.
//
////////////////////////////////////////////////////////////////////////////////

bool Iwm::TryPeekAccess (int offset, Byte & value) const
{
    RegisterRead  read;



    if (m_ism)
    {
        value = ((offset & s_kIsmReadBit) != 0) ? ReadIsmRegister (offset & s_kIsmRegMask) : (Byte) 0;
        return true;
    }

    read  = EvaluateRegister (GetSwitchesAfter (offset));
    value = read.value;

    return read.isKnown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceTo
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::AdvanceTo (uint64_t nowNs)
{
    uint64_t         elapsed  = nowNs > m_lastNs ? nowNs - m_lastNs : 0;
    IDiskDriveBus  * drive    = GetSelected();
    uint64_t         boundary = 0;



    m_lastNs = std::max (m_lastNs, nowNs);

    for (IDiskDriveBus * each : m_drives)
    {
        if (each != nullptr)
        {
            each->Advance (elapsed);
        }
    }

    if (m_motorOffPending && nowNs >= m_motorOffAt)
    {
        m_motorOffPending = false;
        m_enabled         = false;
        UpdateEnables();
    }

    if (drive == nullptr || !m_enabled)
    {
        m_cellCarryNs     = 0;
        m_isUpdatePending = false;
        return;
    }

    m_cellCarryNs += elapsed;
    boundary       = m_lastNs - m_cellCarryNs;

    while (m_cellCarryNs >= drive->GetCellNs())
    {
        uint64_t  cellNs = drive->GetCellNs();
        uint64_t  quiet  = 0;
        uint64_t  cells  = 1;



        //  Reading, with nothing shifted in: cells that read 0 shift in only
        //  zeros, so the shift register is 0 after them and no byte completes.
        if (!m_q7 && m_shift == 0)
        {
            quiet = drive->GetQuietUntil();
            cells = (quiet > boundary + cellNs) ? std::min (m_cellCarryNs / cellNs, (quiet - boundary) / cellNs) : 1;
        }

        if (cells > 1)
        {
            m_cellCarryNs -= cells * cellNs;
            boundary      += cells * cellNs;
            continue;
        }

        m_cellCarryNs -= cellNs;
        boundary      += cellNs;
        MoveCell (boundary);
    }

    if (m_isUpdatePending && m_lastNs >= m_updateAt)
    {
        m_data            = m_shift;
        m_isUpdatePending = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disable
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::Disable()
{
    m_enabled         = false;
    m_motorOffPending = false;
    UpdateEnables();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeFreshByte
//
////////////////////////////////////////////////////////////////////////////////

bool Iwm::ConsumeFreshByte (Byte & outByte)
{
    bool  hasByte = m_hasFreshByte;



    outByte        = m_freshByte;
    m_hasFreshByte = false;

    return hasByte;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::Reset()
{
    m_phases.fill (false);
    m_enabled         = false;
    m_motorOffPending = false;
    m_driveSelect     = 0;
    m_q6              = false;
    m_q7              = false;
    m_mode            = 0;
    m_data            = 0;
    m_shift           = 0;
    m_isUpdatePending = false;
    m_hasFreshByte    = false;
    m_bufferFull      = false;
    m_writeBits       = 0;
    m_underrun        = false;
    m_ism             = false;
    m_ismHistory      = 0;
    m_paramIndex      = 0;

    UpdatePhases();
    UpdateEnables();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSwitch
//
//  Moves the switches as GetSwitchesAfter works them out. Setting Q7 with
//  the drive enabled starts a write.
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::SetSwitch (int offset, uint64_t nowNs)
{
    int          sw    = (offset >> 1) & 7;
    SwitchState  after = GetSwitchesAfter (offset);



    if (after.q7 && !m_q7)
    {
        m_bufferFull = false;
        m_writeBits  = 0;
        m_underrun   = false;
    }

    if (after.isTimerStarted)
    {
        m_motorOffAt = nowNs + kMotorOffNs;
    }

    m_phases          = after.phases;
    m_enabled         = after.isEnabled;
    m_motorOffPending = after.isOffPending;
    m_driveSelect     = after.drive;
    m_q6              = after.q6;
    m_q7              = after.q7;

    if (sw < s_kSwitchEnable || sw == s_kSwitchSelect)
    {
        UpdatePhases();
    }

    if (sw == s_kSwitchEnable || sw == s_kSwitchSelect)
    {
        UpdateEnables();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
//  EvaluateRegister's byte, with its change made.
//
////////////////////////////////////////////////////////////////////////////////

Byte Iwm::ReadRegister()
{
    RegisterRead  read = EvaluateRegister (GetSwitches());



    if (read.isDataTaken)
    {
        m_data = 0;
    }

    return read.value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateRegister
//
//  A read of the register Q6 and Q7 select, with the switches as they are
//  after an access. An asynchronous read of a finished byte takes it out of
//  the data register. Starting a write empties the handshake register's
//  buffer and underrun. A status read is unknown when the access moves a
//  phase or selects another drive, since the drive's sense line follows
//  both.
//
////////////////////////////////////////////////////////////////////////////////

Iwm::RegisterRead Iwm::EvaluateRegister (const SwitchState & after) const
{
    IDiskDriveBus  * drive    = m_drives[(size_t) after.drive];
    bool             isSame   = after.drive == m_driveSelect || m_drives[(size_t) m_driveSelect] == drive;
    bool             isFresh  = after.q7 && !m_q7;
    bool             isFull   = m_bufferFull && !isFresh;
    bool             isUnder  = m_underrun && !isFresh;
    RegisterRead     read;



    if (!after.q7 && !after.q6)
    {
        if (after.isEnabled)
        {
            read.value       = m_data;
            read.isDataTaken = (m_mode & kModeAsync) != 0 && (m_data & s_kTopBit) != 0;
        }
    }
    else if (!after.q7 && after.q6)
    {
        read.isKnown = !after.isDriveMoved && isSame;

        if (read.isKnown)
        {
            read.value = (Byte) (((drive == nullptr || drive->GetSense()) ? s_kSenseBit : 0) |
                                 ((after.isEnabled && !after.isOffPending) ? s_kEnableBit : 0) | (m_mode & kModeMask));
        }
    }
    else if (after.q7 && !after.q6)
    {
        read.value = (Byte) ((isFull ? 0 : s_kEmptyBit) | (isUnder ? 0 : s_kNoUnderrun) | s_kLowBits);
    }

    return read;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::WriteRegister (Byte value)
{
    //  The motor switch counts here, not the one-second delay after it: the
    //  //c writes the mode right after turning the motor off.
    if (!m_enabled || m_motorOffPending)
    {
        m_mode       = (Byte) (value & kModeMask);
        m_ismHistory = (Byte) (((m_ismHistory << 1) | ((value & kIsmBit) != 0 ? 1 : 0)) & kHistoryMask);

        if (m_ismHistory == kIsmEntry)
        {
            m_ism        = true;
            m_ismHistory = 0;
            m_ismMode    = kIsmBit;
            m_ismPhase   = kIsmPhaseReset;
            m_paramIndex = 0;
        }

        return;
    }

    m_writeBuffer = value;
    m_bufferFull  = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveCell
//
//  One cell: read, a bit into the shift register; written, a bit out of it,
//  reloaded from the buffer every eight. In sync mode a byte shifting in
//  replaces the finished one in the data register from its third bit, or
//  eight IWM clocks after its second.
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::MoveCell (uint64_t cellEndNs)
{
    IDiskDriveBus  * drive = GetSelected();



    constexpr Byte  kThirdBit  = 0x04;
    constexpr Byte  kSecondBit = 0x02;
    bool            isSync     = (m_mode & kModeAsync) == 0;



    if (!m_q7)
    {
        m_shift = (Byte) ((m_shift << 1) | drive->ReadCell (cellEndNs));

        if ((m_shift & s_kTopBit) != 0)
        {
            m_data            = m_shift;
            m_freshByte       = m_shift;
            m_hasFreshByte    = true;
            m_shift           = 0;
            m_isUpdatePending = false;
        }
        else if (isSync && m_shift >= kThirdBit)
        {
            m_data            = m_shift;
            m_isUpdatePending = false;
        }
        else if (isSync && m_shift >= kSecondBit)
        {
            m_isUpdatePending = true;
            m_updateAt        = cellEndNs + (((m_mode & kModeFast) != 0) ? kSyncUpdateNs / 2 : kSyncUpdateNs);
        }

        return;
    }

    //  With the motor switch off, a write stops: the disk turns through the
    //  second's delay, and nothing is written to it.
    if (m_motorOffPending)
    {
        (void) drive->ReadCell (cellEndNs);
        return;
    }

    if (m_writeBits == 0)
    {
        if (!m_bufferFull)
        {
            m_underrun = m_underrun || m_writeBits == 0;
            drive->WriteCell (0, cellEndNs);
            return;
        }

        m_shift      = m_writeBuffer;
        m_bufferFull = false;
        m_writeBits  = 8;
    }

    drive->WriteCell ((Byte) ((m_shift >> 7) & 1), cellEndNs);
    m_shift = (Byte) (m_shift << 1);
    m_writeBits--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePhases
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::UpdatePhases()
{
    IDiskDriveBus  * drive = GetSelected();



    if (drive != nullptr)
    {
        drive->SetPhases (m_phases[0], m_phases[1], m_phases[2], m_phases[3]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateEnables
//
//  The drive that isn't selected is disabled first, so a drive attached as
//  both of the IWM's drives ends with the selected drive's enable.
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::UpdateEnables()
{
    for (size_t index = 0; index < m_drives.size(); index++)
    {
        if (m_drives[index] != nullptr && (int) index != m_driveSelect)
        {
            m_drives[index]->SetEnabled (false);
        }
    }

    if (GetSelected() != nullptr)
    {
        GetSelected()->SetEnabled (m_enabled);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Iwm::SaveState
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::SaveState (StateWriter & writer) const
{
    writer.WriteBool   (m_phases[0]);
    writer.WriteBool   (m_phases[1]);
    writer.WriteBool   (m_phases[2]);
    writer.WriteBool   (m_phases[3]);
    writer.WriteBool   (m_enabled);
    writer.WriteBool   (m_motorOffPending);
    writer.WriteUInt64 (m_motorOffAt);
    writer.WriteUInt32 (static_cast<uint32_t> (m_driveSelect));
    writer.WriteBool   (m_q6);
    writer.WriteBool   (m_q7);
    writer.WriteByte   (m_mode);
    writer.WriteByte   (m_data);
    writer.WriteByte   (m_shift);
    writer.WriteBool   (m_isUpdatePending);
    writer.WriteUInt64 (m_updateAt);
    writer.WriteBool   (m_hasFreshByte);
    writer.WriteByte   (m_freshByte);
    writer.WriteByte   (m_writeBuffer);
    writer.WriteBool   (m_bufferFull);
    writer.WriteUInt32 (static_cast<uint32_t> (m_writeBits));
    writer.WriteBool   (m_underrun);
    writer.WriteUInt64 (m_lastNs);
    writer.WriteUInt64 (m_cellCarryNs);
    writer.WriteBool   (m_ism);
    writer.WriteByte   (m_ismHistory);
    writer.WriteByte   (m_ismMode);
    writer.WriteByte   (m_ismPhase);
    writer.WriteUInt64 (static_cast<uint64_t> (m_paramIndex));
    writer.WriteBytes  (m_param.data(), m_param.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Iwm::LoadState
//
//  A drive select or parameter index past the chip's is bad data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Iwm::LoadState (StateReader & reader)
{
    HRESULT   hr          = S_OK;
    uint32_t  driveSelect = 0;
    uint32_t  writeBits   = 0;
    uint64_t  paramIndex  = 0;



    reader.ReadBool   (m_phases[0]);
    reader.ReadBool   (m_phases[1]);
    reader.ReadBool   (m_phases[2]);
    reader.ReadBool   (m_phases[3]);
    reader.ReadBool   (m_enabled);
    reader.ReadBool   (m_motorOffPending);
    reader.ReadUInt64 (m_motorOffAt);
    reader.ReadUInt32 (driveSelect);
    reader.ReadBool   (m_q6);
    reader.ReadBool   (m_q7);
    reader.ReadByte   (m_mode);
    reader.ReadByte   (m_data);
    reader.ReadByte   (m_shift);
    reader.ReadBool   (m_isUpdatePending);
    reader.ReadUInt64 (m_updateAt);
    reader.ReadBool   (m_hasFreshByte);
    reader.ReadByte   (m_freshByte);
    reader.ReadByte   (m_writeBuffer);
    reader.ReadBool   (m_bufferFull);
    reader.ReadUInt32 (writeBits);
    reader.ReadBool   (m_underrun);
    reader.ReadUInt64 (m_lastNs);
    reader.ReadUInt64 (m_cellCarryNs);
    reader.ReadBool   (m_ism);
    reader.ReadByte   (m_ismHistory);
    reader.ReadByte   (m_ismMode);
    reader.ReadByte   (m_ismPhase);
    reader.ReadUInt64 (paramIndex);
    reader.ReadBytes  (m_param.data(), m_param.size());

    hr = reader.GetResult();
    CHR (hr);

    CBREx (driveSelect < kDrives && paramIndex < kParamBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_driveSelect = static_cast<int> (driveSelect);
    m_writeBits   = static_cast<int> (writeBits);
    m_paramIndex  = static_cast<size_t> (paramIndex);

Error:
    return hr;
}
