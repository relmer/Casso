#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/BlockDevice/ProFileDrive.h"




//  The drive's special block number high byte; $FFFFFF is the drive's
//  identification block.
static constexpr Byte  s_kSpecialBlockHigh = 0xFF;

//  Status byte 0's bit for a failed operation.
static constexpr Byte  s_kStatusFailed     = 0x01;

//  The 5 MB drive's blocks; the 10 MB drive has more.
static constexpr uint32_t  s_k5MbBlocks = 9728;





////////////////////////////////////////////////////////////////////////////////
//
//  StartHandshake
//
//  The host starts a handshake: the drive asserts BSY and offers its state
//  byte on the bus.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::StartHandshake()
{
    if (m_phase == Phase::Sending)
    {
        m_phase = Phase::Idle;
        m_state = kStateCommand;
    }
    else if (m_phase == Phase::WriteData && m_buffer.size() >= kBlockBytes)
    {
        m_phase = Phase::Idle;
        m_state = kStateWritten;
    }

    m_presenting = true;
    m_busy       = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRealTiming
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::SetRealTiming (bool isOn, uint32_t cpuClockHz, uint64_t powerOnCycle)
{
    constexpr uint64_t  kReady5MbSeconds  = 60;
    constexpr uint64_t  kReady10MbSeconds = 120;
    uint64_t            readySeconds      = (m_capacity > s_k5MbBlocks) ? kReady10MbSeconds : kReady5MbSeconds;



    m_realTiming = isOn;
    m_clockHz    = cpuClockHz;
    m_readyCycle = isOn ? powerOnCycle + readySeconds * cpuClockHz : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSeekDelay
//
//  An average seek and half a revolution at 3,600 RPM, with real timing on.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ProFileDrive::GetSeekDelay() const
{
    constexpr uint64_t  kSeek5MbMs   = 180;
    constexpr uint64_t  kSeek10MbMs  = 85;
    constexpr uint64_t  kHalfTurnMs  = 8;
    constexpr uint64_t  kMsPerSecond = 1000;
    uint64_t            seekMs       = (m_capacity > s_k5MbBlocks) ? kSeek10MbMs : kSeek5MbMs;



    return m_realTiming ? (seekMs + kHalfTurnMs) * m_clockHz / kMsPerSecond : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndHandshake
//
//  The host ends the handshake. With $55 latched from the host, the drive
//  releases BSY and moves to what the state it offered leads to.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::EndHandshake()
{
    if (!m_presenting)
    {
        return;
    }

    m_presenting = false;
    m_busy       = false;

    if (m_hostByte != kAck)
    {
        return;
    }

    switch (m_state)
    {
        case kStateCommand:
            m_phase = Phase::Command;
            m_buffer.clear();
            break;

        case kStateRead:
            m_phase = Phase::Sending;
            m_index = 0;
            break;

        case kStateWrite:
            m_phase = Phase::WriteData;
            m_buffer.clear();
            break;

        case kStateWritten:
            FinishWrite();
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteData
//
//  A byte the host strobes onto the bus: a handshake answer, a command
//  byte, or a byte of a block being written.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::WriteData (Byte value)
{
    m_hostByte = value;

    if (m_presenting)
    {
        return;
    }

    if (m_phase == Phase::Command)
    {
        m_buffer.push_back (value);

        if (m_buffer.size() == kCommandBytes)
        {
            RunCommand();
        }
    }
    else if (m_phase == Phase::WriteData)
    {
        m_buffer.push_back (value);

        if (m_buffer.size() == kBusBlockBytes)
        {
            m_phase = Phase::Idle;
            m_state = kStateWritten;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadData
//
//  The state byte during a handshake; otherwise the next byte of the status
//  and block the drive is sending.
//
////////////////////////////////////////////////////////////////////////////////

Byte ProFileDrive::ReadData()
{
    Byte  value = 0;



    if (m_presenting)
    {
        return m_state;
    }

    if (m_phase == Phase::Sending && m_index < m_buffer.size())
    {
        value = m_buffer[m_index++];

        if (m_index == m_buffer.size())
        {
            m_phase = Phase::Idle;
            m_state = kStateCommand;
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::Reset()
{
    HRESULT  hr = m_media.Flush();



    IGNORE_RETURN_VALUE (hr, S_OK);

    m_phase      = Phase::Idle;
    m_state      = kStateCommand;
    m_presenting = false;
    m_busy       = false;
    m_buffer.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCommand
//
//  A read builds the status and the block now, to send after the next
//  handshake; a write waits for the block. Operation $FF, which the card's
//  ROM sends after each transfer, returns the drive to waiting for a
//  command.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::RunCommand()
{
    constexpr Byte  kOpRead  = 0;
    constexpr Byte  kOpReset = 0xFF;
    Byte            op       = m_buffer[0];
    bool            special  = m_buffer[1] == s_kSpecialBlockHigh;
    std::vector<Byte> block (kBlockBytes, 0);
    HRESULT         hr      = S_OK;



    m_block = ((uint32_t) m_buffer[1] << 16) | ((uint32_t) m_buffer[2] << 8) | m_buffer[3];
    m_phase = Phase::Idle;
    m_lastActivity.store (GetNow() == 0 ? 1 : GetNow(), std::memory_order_relaxed);

    if (op == kOpReset)
    {
        m_state = kStateCommand;
        return;
    }

    m_readyCycle = GetNow() + GetSeekDelay();

    if (op != kOpRead)
    {
        m_state = kStateWrite;
        return;
    }

    if (special)
    {
        BuildIdBlock (block);
    }
    else
    {
        hr = m_media.ReadBlock (m_block, block);
    }

    m_buffer.assign (kStatusBytes, 0);
    m_buffer[0] = FAILED (hr) ? s_kStatusFailed : (Byte) 0;
    m_buffer.insert (m_buffer.end(), block.begin(), block.end());
    m_buffer.insert (m_buffer.end(), kTagBytes, (Byte) 0);
    m_state = kStateRead;

    Emit (BlockDeviceOp::Read, m_block, m_buffer[0]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishWrite
//
//  The block's 512 data bytes go to the image, and the status is sent next.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::FinishWrite()
{
    HRESULT  hr = m_media.WriteBlock (m_block, std::span<const Byte> (m_buffer).first (kBlockBytes));



    m_buffer.assign (kStatusBytes, 0);
    m_buffer[0] = FAILED (hr) ? s_kStatusFailed : (Byte) 0;
    m_phase     = Phase::Sending;
    m_index     = 0;

    hr = m_media.Flush();
    IGNORE_RETURN_VALUE (hr, S_OK);

    Emit (BlockDeviceOp::Write, m_block, m_buffer[0]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildIdBlock
//
//  The identification block: the name "PROFILE" padded to 13 characters, a
//  device number, a firmware revision, the block count in three bytes, and
//  the bytes per block on the bus, 532.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::BuildIdBlock (std::vector<Byte> & outBlock) const
{
    constexpr const char *  kpszName      = "PROFILE      ";
    constexpr size_t        kNameBytes    = 13;
    constexpr size_t        kRevisionAt   = 16;
    constexpr size_t        kBlocksAt     = 18;
    constexpr size_t        kBusBytesAt   = 21;
    constexpr Byte          kRevisionHigh = 0x03;
    constexpr Byte          kRevisionLow  = 0x98;



    std::fill (outBlock.begin(), outBlock.end(), (Byte) 0);
    std::copy_n (kpszName, kNameBytes, outBlock.begin());

    outBlock[kRevisionAt]     = kRevisionHigh;
    outBlock[kRevisionAt + 1] = kRevisionLow;
    outBlock[kBlocksAt]       = (Byte) (m_capacity >> 16);
    outBlock[kBlocksAt + 1]   = (Byte) (m_capacity >> 8);
    outBlock[kBlocksAt + 2]   = (Byte) m_capacity;
    outBlock[kBusBytesAt]     = (Byte) (kBusBlockBytes >> 8);
    outBlock[kBusBytesAt + 1] = (Byte) kBusBlockBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Emit
//
////////////////////////////////////////////////////////////////////////////////

void ProFileDrive::Emit (BlockDeviceOp op, uint32_t block, Byte result)
{
    BlockDeviceEvent  event;



    if (m_sink == nullptr)
    {
        return;
    }

    event.cycle      = GetNow();
    event.kind       = BlockDeviceKind::ProFile;
    event.slot       = (Byte) m_slot;
    event.op         = op;
    event.firstBlock = block;
    event.blockCount = 1;
    event.result     = result;

    m_sink->OnBlockDeviceEvent (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProFileDrive::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProFileDrive::SaveState (StateWriter & writer) const
{
    writer.WriteByte   (static_cast<Byte> (m_phase));
    writer.WriteByte   (m_state);
    writer.WriteBool   (m_presenting);
    writer.WriteBool   (m_busy);
    writer.WriteByte   (m_hostByte);
    writer.WriteUInt32 (m_block);
    writer.WriteUInt32 (static_cast<uint32_t> (m_buffer.size()));
    writer.WriteBytes  (m_buffer.data(), m_buffer.size());
    writer.WriteUInt64 (static_cast<uint64_t> (m_index));
    writer.WriteUInt64 (m_readyCycle);

    m_media.SaveState (writer);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProFileDrive::LoadState
//
//  A phase or a place in the transfer the drive doesn't have is bad data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProFileDrive::LoadState (StateReader & reader)
{
    HRESULT   hr          = S_OK;
    bool      isInBuffer  = false;
    Byte      phase       = 0;
    uint32_t  bufferBytes = 0;
    uint64_t  index       = 0;



    reader.ReadByte   (phase);
    reader.ReadByte   (m_state);
    reader.ReadBool   (m_presenting);
    reader.ReadBool   (m_busy);
    reader.ReadByte   (m_hostByte);
    reader.ReadUInt32 (m_block);
    reader.ReadUInt32 (bufferBytes);

    hr = reader.GetResult();
    CHR (hr);

    CBREx (phase <= static_cast<Byte> (Phase::WriteData) && bufferBytes <= kStatusBytes + kBusBlockBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_phase = static_cast<Phase> (phase);
    m_buffer.assign (bufferBytes, 0);
    reader.ReadBytes  (m_buffer.data(), m_buffer.size());
    reader.ReadUInt64 (index);
    reader.ReadUInt64 (m_readyCycle);

    hr = reader.GetResult();
    CHR (hr);

    isInBuffer = index <= m_buffer.size();
    CBREx (isInBuffer, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    m_index = static_cast<size_t> (index);

    hr = m_media.LoadState (reader);

Error:
    return hr;
}
