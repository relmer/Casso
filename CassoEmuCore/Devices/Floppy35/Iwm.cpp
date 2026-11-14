#include "Pch.h"

#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/Iwm.h"




static constexpr int   s_kSwitchEnable = 4;
static constexpr int   s_kSwitchSelect = 5;
static constexpr int   s_kSwitchQ6     = 6;
static constexpr int   s_kSwitchQ7     = 7;
static constexpr Byte  s_kSenseBit     = 0x80;
static constexpr Byte  s_kEnableBit    = 0x20;
static constexpr Byte  s_kEmptyBit     = 0x80;
static constexpr Byte  s_kNoUnderrun   = 0x40;
static constexpr Byte  s_kLowBits      = 0x3F;
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

    SetSwitch ((offset >> 1) & 7, (offset & 1) != 0, nowNs);

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
//  as zero and ignore writes.
//
////////////////////////////////////////////////////////////////////////////////

Byte Iwm::AccessIsm (int offset, bool isWrite, Byte value)
{
    constexpr int   kReadBit     = 0x08;
    constexpr int   kRegMask     = 0x07;
    constexpr int   kParam       = 3;
    constexpr int   kPhase       = 4;
    constexpr int   kModeZeros   = 6;
    constexpr int   kModeOnes    = 7;
    constexpr int   kModeRead    = 6;
    constexpr Byte  kLevelMask   = 0x0F;
    constexpr int   kOutputShift = 4;
    int             reg          = offset & kRegMask;
    bool            isRead       = (offset & kReadBit) != 0;
    Byte            result       = 0;
    Byte            levels       = 0;



    if (isRead && !isWrite)
    {
        if (reg == kParam)
        {
            result = m_param[m_paramIndex];
            m_paramIndex = (m_paramIndex + 1) % kParamBytes;
        }
        else if (reg == kPhase)
        {
            result = m_ismPhase;
        }
        else if (reg == kModeRead)
        {
            result = m_ismMode;
        }

        return result;
    }

    if (isRead)
    {
        return result;
    }

    if (reg == kParam)
    {
        m_param[m_paramIndex] = value;
        m_paramIndex          = (m_paramIndex + 1) % kParamBytes;
    }
    else if (reg == kPhase)
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
//  AdvanceTo
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::AdvanceTo (uint64_t nowNs)
{
    uint64_t         elapsed = nowNs > m_lastNs ? nowNs - m_lastNs : 0;
    IDiskDriveBus  * drive   = GetSelected();



    m_lastNs = nowNs;

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
    }

    if (drive == nullptr || !m_enabled)
    {
        m_cellCarryNs = 0;
        return;
    }

    m_cellCarryNs += elapsed;

    while (m_cellCarryNs >= drive->GetCellNs())
    {
        m_cellCarryNs -= drive->GetCellNs();
        MoveCell();
    }
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
    m_bufferFull      = false;
    m_writeBits       = 0;
    m_underrun        = false;
    m_ism             = false;
    m_ismHistory      = 0;
    m_paramIndex      = 0;

    UpdatePhases();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSwitch
//
//  Turning the enable off waits a second unless the mode's timer bit is
//  set. Setting Q7 with the drive enabled starts a write.
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::SetSwitch (int sw, bool on, uint64_t nowNs)
{
    if (sw < s_kSwitchEnable)
    {
        m_phases[(size_t) sw] = on;
        UpdatePhases();
    }
    else if (sw == s_kSwitchEnable)
    {
        if (on)
        {
            m_enabled         = true;
            m_motorOffPending = false;
        }
        else if (m_enabled && (m_mode & kModeNoTimer) == 0)
        {
            m_motorOffPending = true;
            m_motorOffAt      = nowNs + kMotorOffNs;
        }
        else
        {
            m_enabled = false;
        }
    }
    else if (sw == s_kSwitchSelect)
    {
        m_driveSelect = on ? 1 : 0;
        UpdatePhases();
    }
    else if (sw == s_kSwitchQ6)
    {
        m_q6 = on;
    }
    else
    {
        if (on && !m_q7)
        {
            m_bufferFull = false;
            m_writeBits  = 0;
            m_underrun   = false;
        }

        m_q7 = on;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
////////////////////////////////////////////////////////////////////////////////

Byte Iwm::ReadRegister()
{
    IDiskDriveBus  * drive = GetSelected();
    Byte             value = 0xFF;



    if (!m_q7 && !m_q6)
    {
        if (m_enabled)
        {
            value = m_data;

            if ((m_mode & kModeAsync) != 0 && (m_data & s_kTopBit) != 0)
            {
                m_data = 0;
            }
        }
    }
    else if (!m_q7 && m_q6)
    {
        value = (Byte) (((drive == nullptr || drive->GetSense()) ? s_kSenseBit : 0) | (m_enabled ? s_kEnableBit : 0) | (m_mode & kModeMask));
    }
    else if (m_q7 && !m_q6)
    {
        value = (Byte) ((m_bufferFull ? 0 : s_kEmptyBit) | (m_underrun ? 0 : s_kNoUnderrun) | s_kLowBits);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::WriteRegister (Byte value)
{
    if (!m_enabled)
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
//  reloaded from the buffer every eight.
//
////////////////////////////////////////////////////////////////////////////////

void Iwm::MoveCell()
{
    IDiskDriveBus  * drive = GetSelected();



    if (!m_q7)
    {
        m_shift = (Byte) ((m_shift << 1) | drive->ReadCell());

        if ((m_shift & s_kTopBit) != 0)
        {
            m_data  = m_shift;
            m_shift = 0;
        }

        return;
    }

    if (m_writeBits == 0)
    {
        if (!m_bufferFull)
        {
            m_underrun = m_underrun || m_writeBits == 0;
            drive->WriteCell (0);
            return;
        }

        m_shift      = m_writeBuffer;
        m_bufferFull = false;
        m_writeBits  = 8;
    }

    drive->WriteCell ((Byte) ((m_shift >> 7) & 1));
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
