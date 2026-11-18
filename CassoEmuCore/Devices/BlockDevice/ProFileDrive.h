#pragma once

#include "Pch.h"

class StateReader;
class StateWriter;

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProFileDrive
//
//  An Apple ProFile, 5 or 10 MB, at the protocol its interface card's ROM
//  runs. Each exchange starts with a handshake: the host starts it, the drive
//  asserts BSY and puts a state byte on the bus, the host reads it and writes
//  $55, and when the host ends the handshake the drive releases BSY and moves
//  on. The state bytes:
//
//      $01   ready for a command
//      $02   a read was asked for; the status and the block follow
//      $04   a write was asked for; the block is next from the host
//      $06   the block arrived; the status follows the write
//
//  A command is six bytes: the operation (0 read, 1 write, 2 write with
//  verify), the block number high, middle and low, a retry count and a
//  sparing threshold. A block on the bus is the 512 data bytes, then 20 tag
//  bytes; the Apple II card's ROM moves the 512 and stops. Status is four
//  bytes, zero for success.
//
//  A handshake the host ends with anything but $55 results in the same state
//  offered again at the next handshake. A handshake started in the middle of
//  a block ends it: what is left of a read is dropped, and a write with all
//  512 data bytes is complete.
//
////////////////////////////////////////////////////////////////////////////////

class ProFileDrive : public IBlockDeviceEventSource
{
public:
    static constexpr Byte    kStateCommand    = 0x01;
    static constexpr Byte    kStateRead       = 0x02;
    static constexpr Byte    kStateWrite      = 0x04;
    static constexpr Byte    kStateWritten    = 0x06;
    static constexpr Byte    kAck             = 0x55;
    static constexpr size_t  kCommandBytes    = 6;
    static constexpr size_t  kTagBytes        = 20;
    static constexpr size_t  kBlockBytes      = 512;
    static constexpr size_t  kBusBlockBytes   = kTagBytes + kBlockBytes;
    static constexpr size_t  kStatusBytes     = 4;

    ProFileDrive (int slot, uint32_t capacityBlocks) : m_slot (slot), m_capacity (capacityBlocks) {}

    uint32_t  GetCapacity () const { return m_capacity; }
    int       GetSlot     () const { return m_slot; }

    // IBlockDeviceEventSource
    void  SetBlockDeviceEventSink (IBlockDeviceEventSink * sink) override { m_sink = sink; }

    void  StartHandshake ();
    void  EndHandshake   ();
    void  WriteData      (Byte value);
    Byte  ReadData       ();
    Byte  PeekData       () const;
    void  TakeData       ();

    bool  IsBusy         () const { return m_busy && GetNow() >= m_readyCycle; }
    bool  IsPresent      () const { return true; }
    void  Reset          ();

    BlockDeviceMedia        & GetMedia ()       { return m_media; }
    const BlockDeviceMedia  & GetMedia () const { return m_media; }

    void      SetClock             (const uint64_t * clock) { m_clock = clock; }
    uint64_t  GetLastActivityCycle () const { return m_lastActivity.load (std::memory_order_relaxed); }

    // With real timing, BSY is asserted for a handshake only once the drive
    // is ready to operate after power-on, and the state after a read or write
    // command is offered after an average seek and half a turn. Apple's
    // specifications: 60 s and 180 ms for the 5 MB drive, 120 s and 85 ms for
    // the 10 MB drive, at 3,600 RPM.
    void      SetRealTiming        (bool isOn, uint32_t cpuClockHz, uint64_t powerOnCycle);
    bool      IsRealTiming         () const { return m_realTiming; }

    //  The handshake, the transfer under way, the cycle the drive is ready
    //  at and the disk's written blocks, written into and read from the
    //  card's state section.
    HRESULT  SaveState (StateWriter & writer) const;
    HRESULT  LoadState (StateReader & reader);

private:
    enum class Phase : Byte
    {
        Idle,
        Command,
        Sending,
        WriteData,
    };

    bool      IsSendingByte () const;
    void      RunCommand    ();
    void      FinishWrite   ();
    uint64_t  GetSeekDelay  () const;
    void      BuildIdBlock  (std::vector<Byte> & outBlock) const;
    void      Emit          (BlockDeviceOp op, uint32_t block, Byte result);
    uint64_t  GetNow        () const { return m_clock != nullptr ? *m_clock : 0; }

    int                      m_slot         = 0;
    uint32_t                 m_capacity     = 0;
    BlockDeviceMedia         m_media;
    IBlockDeviceEventSink  * m_sink         = nullptr;
    const uint64_t         * m_clock        = nullptr;
    Phase                    m_phase        = Phase::Idle;
    Byte                     m_state        = kStateCommand;
    bool                     m_presenting   = false;
    bool                     m_busy         = false;
    Byte                     m_hostByte     = 0;
    uint32_t                 m_block        = 0;
    std::vector<Byte>        m_buffer;
    size_t                   m_index        = 0;
    bool                     m_realTiming   = false;
    uint32_t                 m_clockHz      = 0;
    uint64_t                 m_readyCycle   = 0;
    std::atomic<uint64_t>    m_lastActivity { 0 };
};
