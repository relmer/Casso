#include "Pch.h"

#include "CassoLink/CassoLinkCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::Encode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::Encode (Byte command, std::span<const Byte> payload, std::vector<Byte> & out)
{
    HRESULT  hr       = S_OK;
    bool     framable = payload.size() <= 0xFFFF;



    CBRAEx (framable, E_INVALIDARG);

    AppendEnvelope (command, (Word) payload.size(), out);
    out.insert (out.end(), payload.begin(), payload.end());

    if (!payload.empty())
    {
        out.push_back (ComputeEor (payload));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::EncodeImageData
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::EncodeImageData (Word startBlock, std::span<const Byte> data, std::vector<Byte> & out)
{
    HRESULT  hr       = S_OK;
    bool     framable = data.size() <= 0xFFFF;
    Word     crc      = 0;



    CBRAEx (framable, E_INVALIDARG);

    AppendEnvelope (kImageData, (Word) data.size(), out);
    out.push_back ((Byte) (startBlock & 0xFF));
    out.push_back ((Byte) (startBlock >> 8));

    EncodePages (data, out);

    crc = ComputeCrc (data);
    out.push_back ((Byte) (crc & 0xFF));
    out.push_back ((Byte) (crc >> 8));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::EncodeSectorData
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::EncodeSectorData (Byte track, Byte sector, std::span<const Byte> data, std::vector<Byte> & out)
{
    HRESULT            hr      = S_OK;
    bool               whole   = data.size() == kSectorSize;
    Word               crc     = 0;
    std::vector<Byte>  payload;



    CBRAEx (whole, E_INVALIDARG);

    crc = ComputeCrc (data);

    payload.reserve (kSectorSize + 4);
    payload.push_back (track);
    payload.push_back (sector);
    payload.insert (payload.end(), data.begin(), data.end());
    payload.push_back ((Byte) (crc & 0xFF));
    payload.push_back ((Byte) (crc >> 8));

    hr = Encode (kSectorData, payload, out);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::EncodeAcknowledge
//
//  The kind, then the number of the next block to send: the form in which
//  a client acknowledges ADTPro's image data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::EncodeAcknowledge (Byte kind, Word block, std::vector<Byte> & out)
{
    const Byte  payload[] = { kind, (Byte) (block & 0xFF), (Byte) (block >> 8) };



    return Encode (kAcknowledge, payload, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::Decode (std::span<const Byte> in, size_t & outConsumed, CassoLinkPacket & outPacket, bool & outComplete)
{
    HRESULT  hr         = S_OK;
    bool     started    = false;
    bool     envelopeOk = false;
    size_t   length     = 0;



    outConsumed = 0;
    outComplete = false;
    outPacket   = CassoLinkPacket();

    BAIL_OUT_IF (in.empty(), S_OK);

    started     = in[0] == kStart;
    outConsumed = 1;
    CBREx (started, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outConsumed = 0;
    BAIL_OUT_IF (in.size() < kEnvelopeSize, S_OK);

    envelopeOk  = ComputeEor (in.first (kEnvelopeSize - 1)) == in[kEnvelopeSize - 1];
    outConsumed = 1;
    CBREx (envelopeOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outConsumed = 0;
    length      = (size_t) in[1] | ((size_t) in[2] << 8);

    if (in[3] == kImageData)
    {
        hr = DecodeImageData (in, length, outConsumed, outPacket, outComplete);
    }
    else
    {
        hr = DecodeFramed (in, length, outConsumed, outPacket, outComplete);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::ParseSectorData
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::ParseSectorData (const CassoLinkPacket & packet, Byte & outTrack, Byte & outSector, std::span<Byte> outData)
{
    HRESULT                hr      = S_OK;
    bool                   room    = outData.size() == kSectorSize;
    bool                   shaped  = packet.command == kSectorData && packet.payload.size() == kSectorSize + 4;
    bool                   crcOk   = false;
    std::span<const Byte>  data;
    Word                   crc     = 0;



    CBRAEx (room, E_INVALIDARG);
    CBREx (shaped, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    data  = std::span<const Byte> (packet.payload).subspan (2, kSectorSize);
    crc   = (Word) (packet.payload[kSectorSize + 2] | (packet.payload[kSectorSize + 3] << 8));
    crcOk = ComputeCrc (data) == crc;
    CBREx (crcOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outTrack  = packet.payload[0];
    outSector = packet.payload[1];
    std::copy (data.begin(), data.end(), outData.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::EncodeSectorNibbles
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::EncodeSectorNibbles (Byte track, Byte sector, std::span<const Byte> data, std::vector<Byte> & out)
{
    HRESULT            hr      = S_OK;
    bool               whole   = data.size() == kSectorSize;
    Word               crc     = 0;
    std::vector<Byte>  nibbles;
    std::vector<Byte>  payload;



    CBRAEx (whole, E_INVALIDARG);

    NibblizationLayer::EncodeDataNibbles (data.data(), nibbles);
    crc = ComputeCrc (nibbles);

    payload.reserve (nibbles.size() + 4);
    payload.push_back (track);
    payload.push_back (sector);
    payload.insert (payload.end(), nibbles.begin(), nibbles.end());
    payload.push_back ((Byte) (crc & 0xFF));
    payload.push_back ((Byte) (crc >> 8));

    hr = Encode (kSectorNibbles, payload, out);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::ParseSectorNibbles
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::ParseSectorNibbles (const CassoLinkPacket & packet, Byte & outTrack, Byte & outSector, std::span<Byte> outData)
{
    HRESULT                hr      = S_OK;
    bool                   room    = outData.size() == kSectorSize;
    bool                   shaped  = packet.command == kSectorNibbles && packet.payload.size() == NibblizationLayer::kDataNibbleCount + 4;
    bool                   crcOk   = false;
    bool                   decoded = false;
    std::span<const Byte>  nibbles;
    std::vector<Byte>      sector;
    Word                   crc     = 0;



    CBRAEx (room, E_INVALIDARG);
    CBREx (shaped, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    nibbles = std::span<const Byte> (packet.payload).subspan (2, NibblizationLayer::kDataNibbleCount);
    crc     = (Word) (packet.payload[NibblizationLayer::kDataNibbleCount + 2] | (packet.payload[NibblizationLayer::kDataNibbleCount + 3] << 8));
    crcOk   = ComputeCrc (nibbles) == crc;
    CBREx (crcOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    decoded = NibblizationLayer::DecodeDataNibbles (nibbles.data(), sector);
    CBREx (decoded, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outTrack  = packet.payload[0];
    outSector = packet.payload[1];
    std::copy (sector.begin(), sector.end(), outData.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::ComputeCrc
//
//  CRC-16/XMODEM: polynomial $1021, initial value 0, most significant bit
//  first, no final EOR.
//
////////////////////////////////////////////////////////////////////////////////

Word CassoLinkCodec::ComputeCrc (std::span<const Byte> data)
{
    uint32_t  crc = 0;



    for (Byte value : data)
    {
        crc ^= (uint32_t) value << 8;

        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }

        crc &= 0xFFFF;
    }

    return (Word) crc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::ComputeEor
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoLinkCodec::ComputeEor (std::span<const Byte> data)
{
    Byte  value = 0;



    for (Byte b : data)
    {
        value ^= b;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::EncodePages
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCodec::EncodePages (std::span<const Byte> data, std::vector<Byte> & out)
{
    Byte    previous = 0;
    size_t  i        = 0;



    while (i < data.size())
    {
        if (data[i] == previous)
        {
            size_t  pageStart = i - i % kPageSize;
            size_t  pageEnd   = pageStart + kPageSize;
            size_t  end       = i;

            while (end < data.size() && end < pageEnd && data[end] == previous)
            {
                end++;
            }

            out.push_back (0);
            out.push_back ((Byte) ((end - pageStart) & 0xFF));
            i = end;
        }
        else
        {
            out.push_back ((Byte) (data[i] - previous));
            previous = data[i];
            i++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::DecodePages
//
//  A repeat must end beyond the bytes already decoded and within `length`;
//  one that does not is damage, not a short buffer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::DecodePages (std::span<const Byte> in, size_t length, size_t & outConsumed, std::vector<Byte> & outData, bool & outComplete)
{
    HRESULT  hr       = S_OK;
    size_t   pos      = 0;
    Byte     previous = 0;
    size_t   runEnd   = 0;
    bool     inRange  = false;



    outConsumed = 0;
    outComplete = false;
    outData.clear();
    outData.reserve (length);

    while (outData.size() < length)
    {
        BAIL_OUT_IF (pos >= in.size(), S_OK);

        if (in[pos] != 0)
        {
            previous = (Byte) (previous + in[pos]);
            outData.push_back (previous);
            pos++;
        }
        else
        {
            BAIL_OUT_IF (pos + 1 >= in.size(), S_OK);

            runEnd  = outData.size() - outData.size() % kPageSize + (in[pos + 1] == 0 ? kPageSize : in[pos + 1]);
            inRange = runEnd > outData.size() && runEnd <= length;
            CBREx (inRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            outData.resize (runEnd, previous);
            pos += 2;
        }
    }

    outConsumed = pos;
    outComplete = true;

Error:
    if (!outComplete)
    {
        outData.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::AppendEnvelope
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCodec::AppendEnvelope (Byte command, Word length, std::vector<Byte> & out)
{
    const Byte  envelope[] = { kStart, (Byte) (length & 0xFF), (Byte) (length >> 8), command };



    out.insert (out.end(), std::begin (envelope), std::end (envelope));
    out.push_back (ComputeEor (envelope));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::DecodeFramed
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::DecodeFramed (std::span<const Byte> in, size_t length, size_t & outConsumed, CassoLinkPacket & outPacket, bool & outComplete)
{
    HRESULT                hr        = S_OK;
    size_t                 total     = kEnvelopeSize + length + (length != 0 ? 1 : 0);
    bool                   payloadOk = false;
    std::span<const Byte>  payload;



    outConsumed = 0;
    outComplete = false;
    BAIL_OUT_IF (in.size() < total, S_OK);

    payload     = in.subspan (kEnvelopeSize, length);
    payloadOk   = length == 0 || ComputeEor (payload) == in[kEnvelopeSize + length];
    outConsumed = total;
    CBREx (payloadOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outPacket.command = in[3];
    outPacket.payload.assign (payload.begin(), payload.end());
    outComplete = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec::DecodeImageData
//
//  With damaged page data the packet's end is unknown, so the caller skips one
//  byte and looks for the next start; a bad CRC comes after a known end, so
//  the whole packet is skipped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCodec::DecodeImageData (std::span<const Byte> in, size_t length, size_t & outConsumed, CassoLinkPacket & outPacket, bool & outComplete)
{
    HRESULT            hr     = S_OK;
    size_t             used   = 0;
    size_t             total  = 0;
    bool               crcOk  = false;
    Word               crc    = 0;
    std::vector<Byte>  data;
    bool               pages  = false;



    outConsumed = 0;
    outComplete = false;
    BAIL_OUT_IF (in.size() < kEnvelopeSize + 2, S_OK);

    hr = DecodePages (in.subspan (kEnvelopeSize + 2), length, used, data, pages);
    outConsumed = FAILED (hr) ? 1 : 0;
    CHR (hr);
    BAIL_OUT_IF (!pages, S_OK);

    total = kEnvelopeSize + 2 + used + 2;
    BAIL_OUT_IF (in.size() < total, S_OK);

    crc         = (Word) (in[total - 2] | (in[total - 1] << 8));
    crcOk       = ComputeCrc (data) == crc;
    outConsumed = total;
    CBREx (crcOk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outPacket.command = kImageData;
    outPacket.payload.reserve (2 + data.size());
    outPacket.payload.push_back (in[kEnvelopeSize]);
    outPacket.payload.push_back (in[kEnvelopeSize + 1]);
    outPacket.payload.insert (outPacket.payload.end(), data.begin(), data.end());
    outComplete = true;

Error:
    return hr;
}
