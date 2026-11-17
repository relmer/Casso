#include "Pch.h"

#include "CassoLink/Audio/DualCassetteByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DualCassetteByteStream::DualCassetteByteStream
//
////////////////////////////////////////////////////////////////////////////////

DualCassetteByteStream::DualCassetteByteStream (std::shared_ptr<ICassetteAudio> audio) :
    CassetteByteStream (audio),
    m_adtProDecoder    (audio->GetSampleRate())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DualCassetteByteStream::Flush
//
//  To ADTPro's client, the bytes written go out as one burst in its
//  encoding; otherwise as Casso's bursts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DualCassetteByteStream::Flush()
{
    HRESULT              hr = S_OK;
    std::vector<double>  halves;
    std::vector<float>   samples;



    if (!m_adtPro)
    {
        hr = CassetteByteStream::Flush();
        CHR (hr);
    }
    else if (!m_pending.empty())
    {
        AdtProAudioCodec::AppendBurst (m_pending, halves);
        CassetteCodec::Render (halves, m_audio->GetSampleRate(), kAmplitude, samples);

        hr = m_audio->Play (samples);
        CHR (hr);

        m_pending.clear();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DualCassetteByteStream::Listen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DualCassetteByteStream::Listen (DWORD waitMs, double & outMs)
{
    constexpr double       kMsPerSecond = 1000.0;
    HRESULT                hr           = S_OK;
    std::vector<float>     samples;
    std::vector<Byte>      burst;
    std::span<const Byte>  packet;



    outMs = 0.0;

    hr = m_audio->Capture (waitMs, samples);
    CHR (hr);

    outMs = (double) samples.size() * kMsPerSecond / (double) m_audio->GetSampleRate();

    m_decoder.Push (samples);
    m_adtProDecoder.Push (samples);

    while (m_decoder.PopBurst (burst))
    {
        m_adtPro = false;
        m_received.insert (m_received.end(), burst.begin(), burst.end());
    }

    while (m_adtProDecoder.PopBurst (burst))
    {
        if (AdtProAudioCodec::HasTrailer (burst))
        {
            m_adtPro = true;
            packet   = AdtProAudioCodec::GetPacket (burst);
            m_received.insert (m_received.end(), packet.begin(), packet.end());
        }
    }

Error:
    return hr;
}
