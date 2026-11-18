#include "Pch.h"

#include "CassoLink/Audio/WasapiCassetteAudio.h"





//  PKEY_Device_FriendlyName, from functiondiscoverykeys_devpkey.h, which
//  would define it here only with INITGUID ahead of it.
static constexpr PROPERTYKEY  s_kDeviceFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::~WasapiCassetteAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiCassetteAudio::~WasapiCassetteAudio()
{
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::ListDevices
//
//  The active devices of one direction, in the enumerator's order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::ListDevices (bool inputs, std::vector<AudioDeviceChoice> & outDevices)
{
    HRESULT                      hr         = S_OK;
    ComPtr<IMMDeviceEnumerator>  enumerator;
    ComPtr<IMMDeviceCollection>  collection;
    UINT                         count      = 0;



    outDevices.clear();

    hr = CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS (&enumerator));
    CHR (hr);

    hr = enumerator->EnumAudioEndpoints (inputs ? eCapture : eRender, DEVICE_STATE_ACTIVE, &collection);
    CHR (hr);

    hr = collection->GetCount (&count);
    CHR (hr);

    for (UINT at = 0; at < count; at++)
    {
        ComPtr<IMMDevice>       device;
        ComPtr<IPropertyStore>  properties;
        LPWSTR                  id     = nullptr;
        PROPVARIANT             name;
        AudioDeviceChoice       choice;

        PropVariantInit (&name);

        hr = collection->Item (at, &device);
        CHR (hr);

        hr = device->GetId (&id);
        CHR (hr);

        choice.id = id;
        CoTaskMemFree (id);

        hr = device->OpenPropertyStore (STGM_READ, &properties);
        CHR (hr);

        hr = properties->GetValue (s_kDeviceFriendlyName, &name);
        CHR (hr);

        choice.name = (name.vt == VT_LPWSTR && name.pwszVal != nullptr) ? name.pwszVal : choice.id;
        PropVariantClear (&name);

        outDevices.push_back (std::move (choice));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::Open (const std::wstring & outputId, const std::wstring & inputId)
{
    HRESULT                      hr = S_OK;
    ComPtr<IMMDeviceEnumerator>  enumerator;



    Close();

    hr = CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS (&enumerator));
    CHR (hr);

    m_inputEvent = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    CWR (m_inputEvent != nullptr);

    hr = OpenClient (enumerator.Get(), eRender, outputId, nullptr, m_output);
    CHR (hr);

    hr = OpenClient (enumerator.Get(), eCapture, inputId, m_inputEvent, m_input);
    CHR (hr);

    hr = m_output->GetBufferSize (&m_outputFrames);
    CHR (hr);

    hr = m_output->GetService (IID_PPV_ARGS (&m_render));
    CHR (hr);

    hr = m_input->GetService (IID_PPV_ARGS (&m_capture));
    CHR (hr);

    hr = m_input->Start();
    CHR (hr);

    hr = m_output->Start();
    CHR (hr);

    m_started = true;

Error:
    if (FAILED (hr))
    {
        Close();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::OpenClient
//
//  Float mono at kSampleRate, with Windows converting to the device's mix
//  format. The input's client signals `event` as each packet arrives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::OpenClient (IMMDeviceEnumerator * enumerator, EDataFlow flow, const std::wstring & id, HANDLE event, ComPtr<IAudioClient> & outClient)
{
    HRESULT            hr     = S_OK;
    ComPtr<IMMDevice>  device;
    WAVEFORMATEX       format = {};
    DWORD              flags  = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;



    if (id.empty())
    {
        hr = enumerator->GetDefaultAudioEndpoint (flow, eConsole, &device);
        CHR (hr);
    }
    else
    {
        hr = enumerator->GetDevice (id.c_str(), &device);
        CHR (hr);
    }

    hr = device->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr, &outClient);
    CHR (hr);

    format.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels       = 1;
    format.nSamplesPerSec  = kSampleRate;
    format.wBitsPerSample  = 32;
    format.nBlockAlign     = 4;
    format.nAvgBytesPerSec = kSampleRate * 4;

    flags |= (event != nullptr) ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0;

    hr = outClient->Initialize (AUDCLNT_SHAREMODE_SHARED, flags, s_kBufferTime, 0, &format, nullptr);
    CHR (hr);

    if (event != nullptr)
    {
        hr = outClient->SetEventHandle (event);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::Close
//
////////////////////////////////////////////////////////////////////////////////

void WasapiCassetteAudio::Close()
{
    if (m_started)
    {
        m_output->Stop();
        m_input->Stop();
        m_started = false;
    }

    m_render.Reset();
    m_capture.Reset();
    m_output.Reset();
    m_input.Reset();
    m_pending.clear();

    if (m_inputEvent != nullptr)
    {
        CloseHandle (m_inputEvent);
        m_inputEvent = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::Play
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::Play (std::span<const float> samples)
{
    HRESULT  hr = S_OK;



    CBREx (m_started, E_NOT_VALID_STATE);

    m_pending.insert (m_pending.end(), samples.begin(), samples.end());

    hr = Pump();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::Capture
//
//  The output is written each time the input's event is signaled or
//  s_kPollMs passes, so it doesn't run dry while the wait lasts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::Capture (DWORD waitMs, std::vector<float> & outSamples)
{
    HRESULT    hr    = S_OK;
    ULONGLONG  start = GetTickCount64();
    ULONGLONG  spent = 0;



    outSamples.clear();
    CBREx (m_started, E_NOT_VALID_STATE);

    do
    {
        hr = Pump();
        CHR (hr);

        hr = ReadInput (outSamples);
        CHR (hr);

        spent = GetTickCount64() - start;

        if (spent < waitMs)
        {
            WaitForSingleObject (m_inputEvent, std::min<DWORD> (s_kPollMs, (DWORD) (waitMs - spent)));
        }
    }
    while (spent < waitMs);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::GetPlayingMs
//
////////////////////////////////////////////////////////////////////////////////

double WasapiCassetteAudio::GetPlayingMs() const
{
    constexpr double  kMsPerSecond = 1000.0;
    UINT32            padding      = 0;



    if (m_started)
    {
        m_output->GetCurrentPadding (&padding);
    }

    return (double) (m_pending.size() + padding) * kMsPerSecond / kSampleRate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::Pump
//
//  As many pending samples as the output's buffer has room for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::Pump()
{
    HRESULT  hr      = S_OK;
    UINT32   padding = 0;
    UINT32   room    = 0;
    BYTE   * buffer  = nullptr;



    BAIL_OUT_IF (m_pending.empty(), S_OK);

    hr = m_output->GetCurrentPadding (&padding);
    CHR (hr);

    room = std::min<UINT32> (m_outputFrames - padding, (UINT32) m_pending.size());
    BAIL_OUT_IF (room == 0, S_OK);

    hr = m_render->GetBuffer (room, &buffer);
    CHR (hr);

    for (UINT32 at = 0; at < room; at++)
    {
        reinterpret_cast<float *> (buffer)[at] = m_pending.front();
        m_pending.pop_front();
    }

    hr = m_render->ReleaseBuffer (room, 0);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio::ReadInput
//
//  Every packet waiting, a silent one as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiCassetteAudio::ReadInput (std::vector<float> & inOutSamples)
{
    HRESULT  hr     = S_OK;
    UINT32   frames = 0;
    BYTE   * data   = nullptr;
    DWORD    flags  = 0;



    hr = m_capture->GetNextPacketSize (&frames);
    CHR (hr);

    while (frames > 0)
    {
        hr = m_capture->GetBuffer (&data, &frames, &flags, nullptr, nullptr);
        CHR (hr);

        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)
        {
            inOutSamples.insert (inOutSamples.end(), frames, 0.0f);
        }
        else
        {
            inOutSamples.insert (inOutSamples.end(), reinterpret_cast<const float *> (data), reinterpret_cast<const float *> (data) + frames);
        }

        hr = m_capture->ReleaseBuffer (frames);
        CHR (hr);

        hr = m_capture->GetNextPacketSize (&frames);
        CHR (hr);
    }

Error:
    return hr;
}
