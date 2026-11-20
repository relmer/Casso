#include "Pch.h"

#include "CassoLink/Audio/AdtProAudioByteStream.h"
#include "CassoLink/Audio/CassetteCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::AdtProAudioByteStream
//
////////////////////////////////////////////////////////////////////////////////

AdtProAudioByteStream::AdtProAudioByteStream (std::shared_ptr<ICassetteAudio> audio) :
    m_audio   (std::move (audio)),
    m_decoder (m_audio->GetSampleRate())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AdtProAudioByteStream::Write (const Byte * data, size_t count)
{
    HRESULT  hr = S_OK;



    CBREx (!m_closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

    m_pending.insert (m_pending.end(), data, data + count);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AdtProAudioByteStream::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
{
    HRESULT  hr = S_OK;



    outRead = 0;
    CBREx (!m_closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

    if (m_received.empty())
    {
        hr = Flush();
        CHR (hr);

        hr = Receive (timeoutMs);
        CHR (hr);
    }

    while (outRead < capacity && !m_received.empty())
    {
        buffer[outRead++] = m_received.front();
        m_received.pop_front();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::GetReadableCount
//
//  The input is decoded as far as it has arrived, without waiting.
//
////////////////////////////////////////////////////////////////////////////////

size_t AdtProAudioByteStream::GetReadableCount()
{
    HRESULT  hr      = S_OK;
    double   heardMs = 0.0;



    if (!m_closed && m_pending.empty())
    {
        hr = Listen (0, heardMs);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    return m_received.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::Flush
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AdtProAudioByteStream::Flush()
{
    HRESULT              hr = S_OK;
    std::vector<double>  halves;
    std::vector<float>   samples;



    BAIL_OUT_IF (m_pending.empty(), S_OK);

    AdtProAudioCodec::AppendBurst (m_pending, halves);
    CassetteCodec::Render (halves, m_audio->GetSampleRate(), kAmplitude, samples);

    hr = m_audio->Play (samples);
    CHR (hr);

    m_pending.clear();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::Receive
//
//  The wait is the queued output's time and then timeoutMs; a burst under
//  way then is waited for to its end, up to the longest a burst can be.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AdtProAudioByteStream::Receive (DWORD timeoutMs)
{
    constexpr double  kUsPerMs = 1000.0;
    HRESULT           hr       = S_OK;
    double            budgetMs = m_audio->GetPlayingMs() + (double) timeoutMs;
    double            limitMs  = budgetMs + AdtProAudioCodec::GetBurstUs (kLongestBurst) / kUsPerMs;
    double            spentMs  = 0.0;
    double            heardMs  = 0.0;



    while (m_received.empty() && !m_closed && spentMs < limitMs)
    {
        if (spentMs >= budgetMs && !m_decoder.IsInBurst())
        {
            break;
        }

        hr = Listen (kStepMs, heardMs);
        CHR (hr);

        spentMs += std::max (heardMs, 1.0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioByteStream::Listen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AdtProAudioByteStream::Listen (DWORD waitMs, double & outMs)
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

    while (m_decoder.PopBurst (burst))
    {
        packet = AdtProAudioCodec::GetPacket (burst);
        m_received.insert (m_received.end(), packet.begin(), packet.end());
    }

Error:
    return hr;
}
