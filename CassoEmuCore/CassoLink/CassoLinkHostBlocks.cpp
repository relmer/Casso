#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Host/CassoLinkVirtualDrives.h"





static constexpr size_t  s_kEntryBytes   = 23;     // slot, unit, SmartPort, blocks (3), kind, volume name (16)
static constexpr size_t  s_kNameBytes    = 16;
static constexpr size_t  s_kBlockPayload = 3 + CassoLinkCodec::kBlockSize + 2;   // the block's number, its bytes and their CRC
static constexpr Byte    s_kNameLength   = 0x0F;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::BuildBlockModule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::BuildBlockModule (std::vector<Byte> & outModule) const
{
    return LinkProgram::BuildBlockModule (outModule, m_programChip, m_faultProgram);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ListBlockDevices
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ListBlockDevices (std::vector<CassoLinkBlockDevice> & outDevices)
{
    HRESULT          hr    = S_OK;
    CassoLinkPacket  reply;
    size_t           count = 0;
    bool             valid = false;



    outDevices.clear();

    hr = RequestModule (CassoLinkCodec::kListBlocks, {}, reply);
    CHR (hr);

    valid = reply.command == CassoLinkCodec::kBlockDevices && !reply.payload.empty();
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    count = reply.payload[0];
    valid = reply.payload.size() == 1 + count * s_kEntryBytes;
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (size_t each = 0; each < count; each++)
    {
        const Byte            * entry  = reply.payload.data() + 1 + each * s_kEntryBytes;
        CassoLinkBlockDevice    device;
        size_t                  length = entry[7] & s_kNameLength;

        device.slot        = entry[0];
        device.unit        = entry[1];
        device.isSmartPort = entry[2] != 0;
        device.blocks      = (uint32_t) entry[3] | ((uint32_t) entry[4] << 8) | ((uint32_t) entry[5] << 16);
        device.kind        = entry[6];
        device.volume.assign ((const char *) entry + 8, std::min (length, s_kNameBytes - 1));

        outDevices.push_back (device);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::MakeBlockRequest
//
//  The slot, the unit, the SmartPort flag, the first block, the count, the
//  verify flag and the progress's percentages.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkHost::MakeBlockRequest (const CassoLinkBlockDevice & device, uint32_t first, Byte count, bool verify, std::vector<Byte> & outRequest) const
{
    outRequest = { device.slot, device.unit, (Byte) (device.isSmartPort ? 1 : 0),
                   (Byte) (first & 0xFF), (Byte) ((first >> 8) & 0xFF), (Byte) ((first >> 16) & 0xFF),
                   count, (Byte) (verify ? 1 : 0), m_progressStart, m_progressEnd };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ReadBlocks
//
//  Each Block data packet must be the next block, whole and with its CRC.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ReadBlocks (const CassoLinkBlockDevice & device, uint32_t first, Byte count, bool verify, std::vector<Byte> & outData, CassoLinkStatus & outStatus)
{
    HRESULT            hr      = S_OK;
    CassoLinkPacket    packet;
    std::vector<Byte>  request;
    uint32_t           next    = first;



    outData.clear();
    outStatus = {};

    hr = AwaitReady();
    CHR (hr);

    MakeBlockRequest (device, first, count, verify, request);

    hr = SendPacket (CassoLinkCodec::kReadBlocks, request);
    CHR (hr);

    for (;;)
    {
        std::span<const Byte>  data;
        uint32_t               block = 0;
        Word                   crc   = 0;
        bool                   valid = false;

        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == CassoLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        valid = packet.command == CassoLinkCodec::kBlockData && packet.payload.size() == s_kBlockPayload;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        block = (uint32_t) packet.payload[0] | ((uint32_t) packet.payload[1] << 8) | ((uint32_t) packet.payload[2] << 16);
        data  = std::span<const Byte> (packet.payload).subspan (3, CassoLinkCodec::kBlockSize);
        crc   = (Word) (packet.payload[3 + CassoLinkCodec::kBlockSize] | (packet.payload[4 + CassoLinkCodec::kBlockSize] << 8));

        valid = block == next;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        valid = CassoLinkCodec::ComputeCrc (data) == crc;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_CRC));

        outData.insert (outData.end(), data.begin(), data.end());
        next++;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::WriteBlocks
//
//  The Apple requests each block with an Acknowledge of its index, and
//  requests it again with a NAK when it arrived damaged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::WriteBlocks (const CassoLinkBlockDevice & device, uint32_t first, std::span<const Byte> data, bool verify, CassoLinkStatus & outStatus)
{
    HRESULT            hr      = S_OK;
    CassoLinkPacket    packet;
    std::vector<Byte>  request;
    size_t             count   = data.size() / CassoLinkCodec::kBlockSize;
    bool               valid   = data.size() % CassoLinkCodec::kBlockSize == 0 && count > 0 && count <= kMaxBlocksARequest;



    outStatus = {};
    CBRAEx (valid, E_INVALIDARG);

    hr = AwaitReady();
    CHR (hr);

    MakeBlockRequest (device, first, (Byte) count, verify, request);

    hr = SendPacket (CassoLinkCodec::kWriteBlocks, request);
    CHR (hr);

    for (;;)
    {
        size_t  index = 0;

        hr = ReadReply (packet);
        CHR (hr);

        if (packet.command == CassoLinkCodec::kStatus)
        {
            hr = ReadStatus (packet, outStatus);
            CHR (hr);
            break;
        }

        valid = packet.command == CassoLinkCodec::kAcknowledge && packet.payload.size() == 3 && packet.payload[1] < count;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        index = packet.payload[1];

        hr = SendBlock (first + (uint32_t) index, data.subspan (index * CassoLinkCodec::kBlockSize, CassoLinkCodec::kBlockSize));
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::SendBlock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::SendBlock (uint32_t block, std::span<const Byte> data)
{
    Word               crc     = CassoLinkCodec::ComputeCrc (data);
    std::vector<Byte>  payload = { (Byte) (block & 0xFF), (Byte) ((block >> 8) & 0xFF), (Byte) ((block >> 16) & 0xFF) };



    payload.insert (payload.end(), data.begin(), data.end());
    payload.push_back ((Byte) (crc & 0xFF));
    payload.push_back ((Byte) (crc >> 8));

    return SendPacket (CassoLinkCodec::kBlockData, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::DescribeBlockError
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkHost::DescribeBlockError (Byte error, uint32_t block)
{
    std::string  text;



    switch (error)
    {
        case kErrorNone:         text = "";                                                                              break;
        case kBlockOffline:      text = std::format ("There's no disk in the drive (block {}).", block);                break;
        case kBlockWriteProtect: text = std::format ("The disk is write-protected (block {}).", block);                 break;
        case kBlockNoDevice:     text = std::format ("The device is offline (block {}).", block);                       break;
        case kBlockIoError:      text = std::format ("I/O error at block {}.", block);                                  break;
        case kErrorVerify:       text = std::format ("Block {} read back differently from what was written.", block);   break;
        case kErrorLink:         text = std::format ("Block {} didn't arrive whole over the link.", block);              break;
        case kErrorDrive:        text = "There's no block device in that slot.";                                         break;
        default:                 text = std::format ("Error ${:02X} at block {}.", error, block);                       break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHost::ServeVirtualBlock
//
//  A request whose size doesn't match its operation, or a write whose block
//  fails its CRC, has no reply, so the driver sends it again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkHost::ServeVirtualBlock (const CassoLinkPacket & packet)
{
    HRESULT                hr        = S_OK;
    std::span<const Byte>  payload   = packet.payload;
    std::vector<Byte>      reply;
    std::vector<Byte>      data (CassoLinkVirtualDrives::kBlockSize, 0);
    int                    drive     = 0;
    Byte                   operation = 0;
    uint32_t               block     = 0;
    uint32_t               blocks    = 0;
    Byte                   error     = CassoLinkVirtualDrives::kNoDevice;
    bool                   sized     = false;
    Word                   crc       = 0;



    sized = payload.size() >= 4;
    BAIL_OUT_IF (!sized, S_OK);

    drive     = payload[0];
    operation = payload[1];
    block     = (uint32_t) payload[2] | ((uint32_t) payload[3] << 8);
    sized     = payload.size() == ((operation == CassoLinkCodec::kVirtualWrite) ? 4 + CassoLinkVirtualDrives::kBlockSize + 2 : 4);
    BAIL_OUT_IF (!sized, S_OK);

    if (operation == CassoLinkCodec::kVirtualWrite)
    {
        std::span<const Byte>  written = payload.subspan (4, CassoLinkVirtualDrives::kBlockSize);
        bool                   intact  = false;

        crc    = (Word) (payload[4 + CassoLinkVirtualDrives::kBlockSize] | (payload[5 + CassoLinkVirtualDrives::kBlockSize] << 8));
        intact = CassoLinkCodec::ComputeCrc (written) == crc;
        BAIL_OUT_IF (!intact, S_OK);

        if (m_virtualDrives != nullptr)
        {
            error = m_virtualDrives->WriteBlock (drive, block, written);
        }

        reply.push_back (error);
    }
    else if (operation == CassoLinkCodec::kVirtualRead)
    {
        if (m_virtualDrives != nullptr)
        {
            error = m_virtualDrives->ReadBlock (drive, block, data);
        }

        crc = CassoLinkCodec::ComputeCrc (data);

        reply.push_back (error);
        reply.insert (reply.end(), data.begin(), data.end());
        reply.push_back ((Byte) (crc & 0xFF));
        reply.push_back ((Byte) (crc >> 8));
    }
    else
    {
        if (m_virtualDrives != nullptr)
        {
            error = m_virtualDrives->GetBlocks (drive, blocks);
        }

        reply.push_back (error);
        reply.push_back ((Byte) (blocks & 0xFF));
        reply.push_back ((Byte) (blocks >> 8));
    }

    hr = SendPacket (CassoLinkCodec::kVirtualBlock, reply);
    CHR (hr);

Error:
    return hr;
}
