#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"
#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer
//
//  The storage card proxy's side of RawDeviceProtocol: the blocks of each
//  card Casso Explorer opens, read and written on request until the stream
//  closes.
//
//  Every check is made here, in the elevated process, whatever the requests
//  are. A drive that is not a storage card is closed again at once, and each
//  request for it fails with E_ACCESSDENIED. A write fails with
//  E_ACCESSDENIED unless each of its blocks is inside a partition of an
//  Apple layout found on the card when it opened, and none is inside a
//  volume Windows has mounted with a file system.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceServer
{
public:
    using DeviceMaker = std::function<std::unique_ptr<IRawDevice> ()>;

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

    HRESULT  ServeOpen   (int number);
    HRESULT  ServeClose  (int number);
    HRESULT  ServeRead   (int number, uint64_t first, uint32_t count);
    HRESULT  ServeWrite  (int number, uint64_t first, uint32_t count);
    HRESULT  CheckRange  (int number, uint64_t first, uint32_t count, Session * & outSession);
    HRESULT  CheckWrite  (Session & session, uint64_t first, uint32_t count);
    HRESULT  Reply       (HRESULT result, std::span<const Byte> payload);

    DeviceMaker               m_makeDevice;
    IByteStream             & m_stream;
    std::map<int, Session>    m_sessions;
};
