#pragma once

#include "Pch.h"

#include "CassoLink/Audio/ICassetteAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AudioDeviceChoice
//
//  A sound device the user can pick: its endpoint id, and its name as
//  Windows' Sound settings has it.
//
////////////////////////////////////////////////////////////////////////////////

struct AudioDeviceChoice
{
    std::wstring  id;
    std::wstring  name;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiCassetteAudio
//
//  The cassette cables on this PC's sound devices, through WASAPI in shared
//  mode: an output device to the Apple's cassette input and an input device
//  from its cassette output, each as 32-bit float mono at kSampleRate,
//  which Windows converts to and from each device's format. An empty id is
//  the default device.
//
//  Queued output is written to the device in each call to Play and Capture,
//  as far as the device's buffer has room, on the thread that reads the
//  link. No other thread is started.
//
////////////////////////////////////////////////////////////////////////////////

class WasapiCassetteAudio : public ICassetteAudio
{
public:
    static constexpr uint32_t  kSampleRate = 48000;

    ~WasapiCassetteAudio() override;

    static HRESULT  ListDevices (bool inputs, std::vector<AudioDeviceChoice> & outDevices);

    HRESULT   Open          (const std::wstring & outputId, const std::wstring & inputId);
    void      Close         ();

    uint32_t  GetSampleRate () const override { return kSampleRate; }
    HRESULT   Play          (std::span<const float> samples) override;
    HRESULT   Capture       (DWORD waitMs, std::vector<float> & outSamples) override;
    double    GetPlayingMs  () const override;

private:
    static HRESULT  OpenClient (IMMDeviceEnumerator * enumerator, EDataFlow flow, const std::wstring & id, HANDLE event, ComPtr<IAudioClient> & outClient);

    HRESULT  Pump       ();
    HRESULT  ReadInput  (std::vector<float> & inOutSamples);

    static constexpr REFERENCE_TIME  s_kBufferTime  = 2000000;   // 200 ms
    static constexpr DWORD           s_kPollMs      = 10;

    ComPtr<IAudioClient>         m_output;
    ComPtr<IAudioClient>         m_input;
    ComPtr<IAudioRenderClient>   m_render;
    ComPtr<IAudioCaptureClient>  m_capture;
    HANDLE                       m_inputEvent   = nullptr;
    UINT32                       m_outputFrames = 0;
    std::deque<float>            m_pending;
    bool                         m_started      = false;
};
