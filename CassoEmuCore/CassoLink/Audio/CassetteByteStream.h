#pragma once

#include "Pch.h"

#include "CassoLink/Audio/CassetteCodec.h"
#include "CassoLink/Audio/ICassetteAudio.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteByteStream
//
//  The link's bytes over the cassette cables, in Casso's audio encoding
//  (CassetteCodec). The link is half duplex, as the Apple's side is: bytes
//  written are queued until the next read, which sends them as bursts of at
//  most CassetteCodec::kMaxBurst bytes and then listens. A read's timeout
//  starts once the bursts are sent, and a burst under way at its end is
//  received whole. Time is counted in the input's samples, so it is the machine's
//  over an emulated one.
//
////////////////////////////////////////////////////////////////////////////////

class CassetteByteStream : public IByteStream
{
public:
    static constexpr float   kAmplitude  = 0.7f;
    static constexpr DWORD   kStepMs     = 20;
    static constexpr double  kBurstGapMs = 300.0;

    explicit CassetteByteStream (std::shared_ptr<ICassetteAudio> audio);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override { return m_closed; }
    void     Close            () override       { m_closed = true; }

    //  Half cycles sent through the output as they are, after the bytes
    //  written, as the monitor's tape record that starts the link.
    HRESULT  PlayHalves       (std::span<const double> halvesUs);

    //  The input's peak level, 0 to 1.
    float    GetInputLevel    () const { return m_decoder.GetLevel(); }

protected:
    virtual HRESULT  Flush       ();
    virtual HRESULT  Listen      (DWORD waitMs, double & outMs);
    virtual bool     IsReceiving () const { return m_decoder.IsInBurst(); }
    HRESULT          Receive     (DWORD timeoutMs);

    std::shared_ptr<ICassetteAudio>  m_audio;
    CassetteDecoder                  m_decoder;
    std::vector<Byte>                m_pending;
    std::deque<Byte>                 m_received;
    bool                             m_closed  = false;
};
