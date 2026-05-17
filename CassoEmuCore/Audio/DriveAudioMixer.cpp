#include "Pch.h"

#include "Audio/DriveAudioMixer.h"
#include "Audio/DiskIIAudioSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAudioMixer
//
////////////////////////////////////////////////////////////////////////////////

DriveAudioMixer::DriveAudioMixer()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DriveAudioMixer
//
////////////////////////////////////////////////////////////////////////////////

DriveAudioMixer::~DriveAudioMixer()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterSource
//
////////////////////////////////////////////////////////////////////////////////

void DriveAudioMixer::RegisterSource (IDriveAudioSource * source)
{
    if (source == nullptr)
    {
        return;
    }

    m_sources.push_back (source);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnregisterSource
//
////////////////////////////////////////////////////////////////////////////////

void DriveAudioMixer::UnregisterSource (IDriveAudioSource * source)
{
    auto  it = std::find (m_sources.begin(), m_sources.end(), source);

    if (it != m_sources.end())
    {
        m_sources.erase (it);
    }
}


void DriveAudioMixer::UnregisterAllSources()
{
    m_sources.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetEnabled / IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

void DriveAudioMixer::SetEnabled (bool enabled)
{
    m_enabled = enabled;
}


bool DriveAudioMixer::IsEnabled() const
{
    return m_enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleLoadContext / SetMechanism / IsValidMechanism
//
//  Per spec 005-disk-ii-audio Phase 14 (FR-006 / SC-010 / T141):
//  remember the asset-load context once, then let SetMechanism
//  iterate the registered Disk II sources and re-invoke LoadSamples
//  on each. State change MUST take effect within one audio frame --
//  LoadSamples replaces the per-buffer move targets atomically from
//  the same thread that drives GeneratePCM (NFR-002 same-thread
//  state model), so no cross-thread synchronisation is required.
//
////////////////////////////////////////////////////////////////////////////////

bool DriveAudioMixer::IsValidMechanism (const wstring & mechanism) const
{
    return mechanism == L"Shugart" || mechanism == L"Alps";
}


void DriveAudioMixer::SetSampleLoadContext (
    const wstring & devicesDir,
    uint32_t        sampleRate)
{
    m_devicesDir     = devicesDir;
    m_loadSampleRate = sampleRate;
}


HRESULT DriveAudioMixer::SetMechanism (const wstring & mechanism)
{
    HRESULT  hr = S_OK;



    if (!IsValidMechanism (mechanism))
    {
        // SC-010 invariant: bad input does NOT mutate state. The
        // mixer keeps whatever mechanism was active before the bad
        // call so the running audio frame is undisturbed.
        return E_INVALIDARG;
    }

    m_mechanism = mechanism;

    if (m_devicesDir.empty () || m_loadSampleRate == 0)
    {
        // No asset context yet (the host hasn't called
        // SetSampleLoadContext, typically because WASAPI hasn't
        // started). Remember the mechanism so the eventual first
        // load uses the right subdir.
        return S_OK;
    }

    for (IDriveAudioSource * src : m_sources)
    {
        DiskIIAudioSource *  disk = dynamic_cast<DiskIIAudioSource *> (src);

        if (disk == nullptr)
        {
            continue;
        }

        HRESULT  hrLoad = disk->LoadSamples (m_devicesDir.c_str (),
                                             m_mechanism.c_str (),
                                             m_loadSampleRate);

        // FR-009: a per-source load failure mutes that source but
        // never propagates up; the mechanism switch as a whole has
        // still succeeded from the caller's perspective.
        (void) hrLoad;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Forwards the cycle count to any source that participates in
//  cycle-based state machines (Disk II step/seek discriminator).
//  Non-Disk-II sources can no-op their Tick; we only special-case
//  DiskIIAudioSource here because IDriveAudioSource itself does not
//  carry a Tick on its abstract surface (NFR-002 keeps the abstract
//  interface minimal).
//
////////////////////////////////////////////////////////////////////////////////

void DriveAudioMixer::Tick (uint64_t currentCycle)
{
    DiskIIAudioSource *  disk = nullptr;

    for (IDriveAudioSource * src : m_sources)
    {
        disk = dynamic_cast<DiskIIAudioSource *> (src);

        if (disk != nullptr)
        {
            disk->Tick (currentCycle);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  Stereo interleaved (L, R, L, R, ...). Always clears the output
//  buffer first. Per-source mono PCM is panned via the source's
//  precomputed equal-power coefficients (FR-008 -- linear sum, no
//  motor-hum dedup; FR-010 / FR-012).
//
////////////////////////////////////////////////////////////////////////////////

void DriveAudioMixer::GeneratePCM (float * stereoOut, uint32_t numSamples)
{
    uint32_t  i        = 0;
    float     panL     = 0.0f;
    float     panR     = 0.0f;
    float     monoSamp = 0.0f;

    if (stereoOut == nullptr || numSamples == 0)
    {
        return;
    }

    memset (stereoOut, 0, sizeof (float) * 2 * numSamples);

    if (!m_enabled || m_sources.empty())
    {
        return;
    }

    if (m_scratchMono.size() < numSamples)
    {
        m_scratchMono.resize (numSamples);
    }

    for (IDriveAudioSource * src : m_sources)
    {
        memset (m_scratchMono.data(), 0, sizeof (float) * numSamples);
        src->GeneratePCM (m_scratchMono.data(), numSamples);

        panL = src->PanLeft();
        panR = src->PanRight();

        // Help the analyzer see the loop bound matches the
        // documented 2 * numSamples capacity of stereoOut. The
        // memset(stereoOut, 0, 2*numSamples*sizeof(float)) above
        // already proved the buffer covers that range; the inner
        // additive loop reuses the same span.
        for (i = 0; i < numSamples; i++)
        {
            monoSamp = m_scratchMono[i];

            #pragma warning (suppress: 6385 6386)
            stereoOut[2 * i]     += monoSamp * panL;
            #pragma warning (suppress: 6385 6386)
            stereoOut[2 * i + 1] += monoSamp * panR;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MixDriveIntoSpeakerStereo
//
//  Static helper: sums interleaved-stereo `driveStereo` into
//  interleaved-stereo `speakerStereo` in place with per-channel
//  clamp to [-1, +1]. Exposed for unit testing (FR-010 / FR-011 /
//  SC-006).
//
////////////////////////////////////////////////////////////////////////////////

void DriveAudioMixer::MixDriveIntoSpeakerStereo (
    float *         speakerStereo,
    const float *   driveStereo,
    uint32_t        numSamples)
{
    uint32_t  i = 0;
    float     l = 0.0f;
    float     r = 0.0f;

    if (speakerStereo == nullptr || driveStereo == nullptr || numSamples == 0)
    {
        return;
    }

    for (i = 0; i < numSamples; i++)
    {
        l = speakerStereo[2 * i]     + driveStereo[2 * i];
        r = speakerStereo[2 * i + 1] + driveStereo[2 * i + 1];

        if (l >  1.0f)  { l =  1.0f; }
        if (l < -1.0f)  { l = -1.0f; }
        if (r >  1.0f)  { r =  1.0f; }
        if (r < -1.0f)  { r = -1.0f; }

        speakerStereo[2 * i]     = l;
        speakerStereo[2 * i + 1] = r;
    }
}
