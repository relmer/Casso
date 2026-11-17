#include "Pch.h"

#include "CassoLink/Audio/CassetteByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteByteStream::CassetteByteStream
//
////////////////////////////////////////////////////////////////////////////////

CassetteByteStream::CassetteByteStream (std::shared_ptr<ICassetteAudio> audio) :
    m_audio   (std::move (audio)),
    m_decoder (m_audio->GetSampleRate())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteByteStream::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassetteByteStream::Write (const Byte * data, size_t count)
{
    HRESULT  hr = S_OK;



    CBREx (!m_closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

    m_pending.insert (m_pending.end(), data, data + count);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteByteStream::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassetteByteStream::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
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
//  CassetteByteStream::GetReadableCount
//
//  The input is decoded as far as it has arrived, without waiting.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassetteByteStream::GetReadableCount()
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
//  CassetteByteStream::PlayHalves
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassetteByteStream::PlayHalves (std::span<const double> halvesUs)
{
    HRESULT             hr = S_OK;
    std::vector<float>  samples;



    CBREx (!m_closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

    hr = Flush();
    CHR (hr);

    CassetteCodec::Render (halvesUs, m_audio->GetSampleRate(), kAmplitude, samples);

    hr = m_audio->Play (samples);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteByteStream::Flush
//
//  Each burst after the first follows a gap, so the Apple has stored the
//  one before it and is listening again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassetteByteStream::Flush()
{
    constexpr double     kUsPerMs = 1000.0;
    HRESULT              hr       = S_OK;
    std::vector<double>  halves;
    std::vector<float>   samples;
    std::vector<float>   gap ((size_t) (kBurstGapMs * m_audio->GetSampleRate() / kUsPerMs), 0.0f);



    BAIL_OUT_IF (m_pending.empty(), S_OK);

    for (size_t at = 0; at < m_pending.size(); at += CassetteCodec::kMaxBurst)
    {
        size_t  count = std::min (CassetteCodec::kMaxBurst, m_pending.size() - at);

        halves.clear();
        CassetteCodec::AppendBurst (std::span<const Byte> (m_pending).subspan (at, count), halves);
        CassetteCodec::Render (halves, m_audio->GetSampleRate(), kAmplitude, samples);

        if (at > 0)
        {
            hr = m_audio->Play (gap);
            CHR (hr);
        }

        hr = m_audio->Play (samples);
        CHR (hr);
    }

    m_pending.clear();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteByteStream::Receive
//
//  The wait is the queued output's time and then timeoutMs; a burst under
//  way then is waited for to its end, up to the longest a burst can be.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassetteByteStream::Receive (DWORD timeoutMs)
{
    constexpr double  kUsPerMs = 1000.0;
    HRESULT           hr       = S_OK;
    double            budgetMs = m_audio->GetPlayingMs() + (double) timeoutMs;
    double            limitMs  = budgetMs + CassetteCodec::GetBurstUs (CassetteCodec::kMaxBurst) / kUsPerMs;
    double            spentMs  = 0.0;
    double            heardMs  = 0.0;



    while (m_received.empty() && !m_closed && spentMs < limitMs)
    {
        if (spentMs >= budgetMs && !IsReceiving())
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
//  CassetteByteStream::Listen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassetteByteStream::Listen (DWORD waitMs, double & outMs)
{
    constexpr double    kMsPerSecond = 1000.0;
    HRESULT             hr           = S_OK;
    std::vector<float>  samples;
    std::vector<Byte>   burst;



    outMs = 0.0;

    hr = m_audio->Capture (waitMs, samples);
    CHR (hr);

    outMs = (double) samples.size() * kMsPerSecond / (double) m_audio->GetSampleRate();

    m_decoder.Push (samples);

    while (m_decoder.PopBurst (burst))
    {
        m_received.insert (m_received.end(), burst.begin(), burst.end());
    }

Error:
    return hr;
}
