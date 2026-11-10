#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"
#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer
//
//  The storage card broker's side of RawDeviceProtocol: the blocks of each
//  card Casso Explorer opens, read and written on request until the stream
//  closes.
//
//  Every check is made here, in the elevated process, whatever the requests
//  are. A drive that is not a storage card is closed again at once, and each
//  request for it fails with E_ACCESSDENIED. A write fails with
//  E_ACCESSDENIED unless each of its blocks is inside a partition of an
//  Apple layout found on the card when it opened, and none is inside a
//  volume Windows has mounted with a file system. An initialize is the one
//  request that writes outside them, and the blocks it writes are built here.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceServer
{
public:
    //  A new layout is CFFA 2.0's: up to 13 ProDOS volumes of 32 MB from block
    //  0, the most its firmware maps. The card must be larger than one. The
    //  partition count in the firmware's menu sets how many are mapped.
    static constexpr int       kInitialVolumes  = 13;
    static constexpr uint32_t  kSliceBlocks     = 65536;
    static constexpr uint32_t  kVolumeBlocks    = 65535;
    static constexpr uint32_t  kGptBlocks       = 34;
    static constexpr uint32_t  kMinVolumeBlocks = 280;

    //  The 1 GB volumes of the CFFA 1.x GS/OS driver, after the first four
    //  slices, and the blocks cleared at each place a volume could be.
    static constexpr uint64_t  kLargeVolumeBlocks = 2097152;
    static constexpr uint64_t  kLargeVolumes      = 2;
    static constexpr uint32_t  kHeaderBlocks      = 4;

    using DeviceMaker = std::function<std::unique_ptr<IRawDevice> ()>;
    using LogFn       = std::function<void (const std::wstring & line)>;

    //  Each open and each step of an initialize, with its result.
    void  SetLog (LogFn log) { m_log = std::move (log); }

    //  The volumes an initialize writes on a drive of `blocks`: 0 for a drive
    //  too small.
    static int  CountInitialVolumes (uint64_t blocks);

    RawDeviceServer (DeviceMaker makeDevice, IByteStream & stream) : m_makeDevice (std::move (makeDevice)), m_stream (stream) {}

    //  Returns S_OK once the stream has closed, and ERROR_INVALID_DATA after
    //  a request that is not one of the protocol's, which also closes the
    //  stream. Every drive is closed either way.
    HRESULT  Run ();

private:
    struct Session
    {
        std::unique_ptr<IRawDevice>  device;
        PhysicalDrive                drive;
        std::vector<BlockExtent>     writable;
    };

    static void  FindApplePartitions (IRawDevice & device, uint64_t blocks, std::vector<BlockExtent> & outExtents);

    HRESULT  ServeOpen       (int number);
    HRESULT  ServeClose      (int number);
    HRESULT  ServeRead       (int number, uint64_t first, uint32_t count);
    HRESULT  ServeWrite      (int number, uint64_t first, uint32_t count);
    HRESULT  ServeInitialize (int number, uint64_t layout);
    HRESULT  WriteCffaLayout (Session & session, int volumes);
    HRESULT  ClearHeader     (Session & session, uint64_t at, uint64_t written, uint64_t total, std::span<const Byte> header);
    HRESULT  CheckRange      (int number, uint64_t first, uint32_t count, Session * & outSession);
    HRESULT  CheckWrite      (Session & session, uint64_t first, uint32_t count);
    HRESULT  Reply           (HRESULT result, std::span<const Byte> payload);

    void  Log (const std::wstring & step, HRESULT result) const { if (m_log) { m_log (std::format (L"{}: 0x{:08X}", step, (uint32_t) result)); } }

    DeviceMaker               m_makeDevice;
    LogFn                     m_log;
    IByteStream             & m_stream;
    std::map<int, Session>    m_sessions;
};
