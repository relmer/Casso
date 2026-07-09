#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/Prng.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Devices/RamDevice.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/Apple2eKeyboard.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Devices/AppleSpeaker.h"
#include "Devices/LanguageCard.h"
#include "Devices/Apple2cRomBank.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/AppleMouse.h"
#include "Core/InterruptController.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Video/VideoTiming.h"
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
    Apple2e,
    Apple2c,
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
    HeadlessMachineKind      machineKind = HeadlessMachineKind::Apple2e;
    std::unique_ptr<Prng>    prng;
    std::unique_ptr<MockHostShell>     host;
    std::unique_ptr<FixtureProvider>   fixtures;
    IAudioSink *             audioSink = nullptr;

    // Phase 7 (T067/T069): full //e machine wiring is populated by
    // HeadlessHost::BuildApple2e so integration tests can drive a real
    // cold boot through `Apple2e.rom`. ][/][+ kinds leave these unset.
    std::unique_ptr<MemoryBus>                 bus;
    std::unique_ptr<RamDevice>                 mainRam;
    std::unique_ptr<VideoTiming>               videoTiming;
    std::unique_ptr<Apple2eMmu>               mmu;
    std::unique_ptr<Apple2eKeyboard>          keyboard;
    std::unique_ptr<Apple2eSoftSwitchBank>    softSwitches;
    std::unique_ptr<AppleSpeaker>              speaker;
    std::unique_ptr<LanguageCard>              languageCard;
    std::unique_ptr<LanguageCardBank>          lcBank;
    std::unique_ptr<EmuCpu>                    cpu;

    // Apple //c (65C02 + 32K two-bank firmware ROM). Set by
    // HeadlessHost::BuildApple2c; null for every other machine kind.
    std::unique_ptr<Apple2cRomBank>            romBank;

    // Phase 11 (T097/T099-T104). Optional Disk II wiring. Set by
    // HeadlessHost::BuildApple2eWithDisk2 so US2 integration tests can
    // mount synthetic disks through the store and pump the controller's
    // nibble engine in lock-step with the CPU.
    std::unique_ptr<Disk2Controller>           diskController;
    std::unique_ptr<DiskImageStore>            diskStore;

    // Apple //c dual 6551 ACIA serial ports (phantom slots 1 & 2). Set by
    // HeadlessHost::BuildApple2c; null for every other machine kind. v1
    // endpoints are loopback (comms self-test) so a guest write to the data
    // register echoes back into the receiver.
    std::unique_ptr<Acia6551>                  serial1;
    std::unique_ptr<Acia6551>                  serial2;
    std::unique_ptr<AciaLoopbackEndpoint>      serial1Loopback;
    std::unique_ptr<AciaLoopbackEndpoint>      serial2Loopback;

    // Apple //c IOU mouse (US4) + the shared interrupt controller its VBL /
    // movement IRQ lines aggregate through. Set by HeadlessHost::BuildApple2c;
    // null for every other machine kind.
    std::unique_ptr<InterruptController>       interruptController;
    std::unique_ptr<AppleMouse>                mouse;

    // Cycle-pumped helpers used by Phase 7 integration tests.
    void   PowerCycle    ();
    void   SoftReset     ();
    void   RunCycles     (uint64_t cycleBudget);
    bool   HasApple2e   () const { return cpu != nullptr && mmu != nullptr; }
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

    HRESULT             BuildAppleII             (EmulatorCore & outCore);
    HRESULT             BuildAppleIIPlus         (EmulatorCore & outCore);
    HRESULT             BuildApple2e             (EmulatorCore & outCore);
    HRESULT             BuildApple2eWithDisk2    (EmulatorCore & outCore);
    HRESULT             BuildApple2c             (EmulatorCore & outCore);

private:
    HRESULT             BuildCommon (HeadlessMachineKind kind, EmulatorCore & outCore);
};
