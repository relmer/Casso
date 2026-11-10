#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"
#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceServer
//
//  The elevated process's side of RawDeviceProtocol: one drive's blocks,
//  read and written on request until the stream closes.
//
//  The drive is checked here, in the process that has administrator access:
//  a drive that is not removable is never read or written, whatever the
//  requests are. The reply to each request for it is E_ACCESSDENIED.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceServer
{
public:
    RawDeviceServer (IRawDevice & device, IByteStream & stream) : m_device (device), m_stream (stream) {}

    //  Returns S_OK once the stream has closed, and ERROR_INVALID_DATA after
    //  a request that is not one of the protocol's, which also closes the
    //  stream. The drive is closed either way.
    HRESULT  Run (int driveNumber);

private:
    HRESULT  ServeInfo  ();
    HRESULT  ServeRead  (uint64_t first, uint32_t count);
    HRESULT  ServeWrite (uint64_t first, uint32_t count);
    HRESULT  CheckRange (uint64_t first, uint32_t count) const;
    HRESULT  Reply      (HRESULT result, std::span<const Byte> payload);

    IRawDevice     & m_device;
    IByteStream    & m_stream;
    PhysicalDrive    m_drive;
    HRESULT          m_opened = E_FAIL;
};
