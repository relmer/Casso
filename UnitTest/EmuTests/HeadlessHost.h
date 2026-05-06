#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/Prng.h"
#include "FixtureProvider.h"
#include "MockAudioSink.h"
#include "MockHostShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessMachineKind
//
////////////////////////////////////////////////////////////////////////////////

enum class HeadlessMachineKind
{
    AppleII,
    AppleIIPlus,
    AppleIIe,
};





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCore (headless)
//
//  Aggregates the deterministic plumbing a single headless test instance
//  needs: pinned-seed Prng, MockAudioSink (handed out by the host shell),
//  MockHostShell, FixtureProvider rooted at UnitTest/Fixtures/, and the
//  selected machine kind. Later phases extend this struct with the full
//  MemoryBus / EmuCpu / device wiring; F0 ships the deterministic edges
//  only — no production behavior changes.
//
////////////////////////////////////////////////////////////////////////////////

struct EmulatorCore
{
    HeadlessMachineKind      machineKind = HeadlessMachineKind::AppleIIe;
    std::unique_ptr<Prng>    prng;
    std::unique_ptr<MockHostShell>     host;
    std::unique_ptr<FixtureProvider>   fixtures;
    IAudioSink *             audioSink = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHost
//
//  Concrete test host. Wires MockHostShell (constitution §II — no Win32
//  window, no audio device, no host filesystem outside Fixtures, no
//  registry, no network) + MockAudioSink + FixtureProvider rooted at
//  UnitTest/Fixtures/ + a Prng pinned to 0xCA550001 so two builds produce
//  byte-identical output.
//
////////////////////////////////////////////////////////////////////////////////

class HeadlessHost
{
public:
    static constexpr uint64_t   kPinnedSeed = 0xCA550001ULL;

    HRESULT             BuildAppleII     (EmulatorCore & outCore);
    HRESULT             BuildAppleIIPlus (EmulatorCore & outCore);
    HRESULT             BuildAppleIIe    (EmulatorCore & outCore);

private:
    HRESULT             BuildCommon (HeadlessMachineKind kind, EmulatorCore & outCore);
};
