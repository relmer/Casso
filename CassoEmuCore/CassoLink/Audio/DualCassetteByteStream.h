#pragma once

#include "Pch.h"

#include "CassoLink/Audio/AdtProAudioCodec.h"
#include "CassoLink/Audio/CassetteByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DualCassetteByteStream
//
//  A cassette link for both Casso's program and ADTPro's audio client, as a
//  serial line is for both. The input is decoded in Casso's encoding and in
//  ADTPro's, and bytes written go out in the encoding of the last burst
//  received, Casso's until an ADTPro burst arrives. A burst decoded as
//  ADTPro's counts only when it ends in the client's trailer, so a run of
//  Casso's 1 bits isn't taken for ADTPro's tone.
//
////////////////////////////////////////////////////////////////////////////////

class DualCassetteByteStream : public CassetteByteStream
{
public:
    explicit DualCassetteByteStream (std::shared_ptr<ICassetteAudio> audio);

    bool  IsAdtPro () const { return m_adtPro; }

protected:
    HRESULT  Flush       () override;
    HRESULT  Listen      (DWORD waitMs, double & outMs) override;
    bool     IsReceiving () const override { return m_decoder.IsInBurst() || m_adtProDecoder.IsInBurst(); }

private:
    AdtProAudioDecoder  m_adtProDecoder;
    bool                m_adtPro = false;
};
