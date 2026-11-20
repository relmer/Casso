#pragma once

#include "Pch.h"

#include "CassoLink/Audio/AdtProAudioCodec.h"
#include "CassoLink/Audio/ICassetteAudio.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream
//
//  The link's bytes over the cassette cables in ADTPro's audio encoding
//  (AdtProAudioCodec), so CassoLinkHost serves ADTPro's audio client as it
//  serves its serial one. Bytes written are queued until the next read,
//  which sends them as one burst and then reads the input. ADTPro's client
//  reads each reply from a separate burst. A burst received is read as the
//  packet in it, without the client's trailer.
//
////////////////////////////////////////////////////////////////////////////////

class AdtProAudioByteStream : public IByteStream
{
public:
    static constexpr float   kAmplitude    = 0.7f;
    static constexpr DWORD   kStepMs       = 20;
    static constexpr size_t  kLongestBurst = 0x1200;

    explicit AdtProAudioByteStream (std::shared_ptr<ICassetteAudio> audio);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override { return m_closed; }
    void     Close            () override       { m_closed = true; }

    float    GetInputLevel    () const { return m_decoder.GetLevel(); }

private:
    HRESULT  Flush   ();
    HRESULT  Receive (DWORD timeoutMs);
    HRESULT  Listen  (DWORD waitMs, double & outMs);

    std::shared_ptr<ICassetteAudio>  m_audio;
    AdtProAudioDecoder               m_decoder;
    std::vector<Byte>                m_pending;
    std::deque<Byte>                 m_received;
    bool                             m_closed  = false;
};
