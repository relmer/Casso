#include "../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildCommon
//
//  Stamps out a fully-mocked EmulatorCore: pinned Prng, MockHostShell,
//  FixtureProvider rooted at UnitTest/Fixtures/, and the IAudioSink the
//  host shell hands out. Constitution §II — no Win32, no audio device,
//  no host filesystem outside fixtures, no registry, no network.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildCommon (HeadlessMachineKind kind, EmulatorCore & outCore)
{
    HRESULT     hr = S_OK;

    outCore.machineKind = kind;
    outCore.prng        = std::make_unique<Prng> (kPinnedSeed);
    outCore.host        = std::make_unique<MockHostShell> ();
    outCore.fixtures    = std::make_unique<FixtureProvider> ();
    outCore.audioSink   = nullptr;

    hr = outCore.host->OpenAudioDevice (outCore.audioSink);
    if (FAILED (hr))
    {
        goto Error;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleII
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleII (EmulatorCore & outCore)
{
    return BuildCommon (HeadlessMachineKind::AppleII, outCore);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleIIPlus
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleIIPlus (EmulatorCore & outCore)
{
    return BuildCommon (HeadlessMachineKind::AppleIIPlus, outCore);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost::BuildAppleIIe
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HeadlessHost::BuildAppleIIe (EmulatorCore & outCore)
{
    return BuildCommon (HeadlessMachineKind::AppleIIe, outCore);
}
