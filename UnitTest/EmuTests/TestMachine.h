#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/MachineConfig.h"
#include "Core/Prng.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"

//  MachineHost forward-declares the devices it holds, because production
//  code reaches most of them through MachineRefs. A test reaches THROUGH
//  them -- it asks the MMU which bank is paged in and the controller which
//  track the head is on -- so the machine a test builds arrives with its
//  parts defined rather than named.
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Common/VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TestMachine
//
//  A real machine, built for a test by the code that builds the real one.
//
//  Name a shipped machine and you get it: its configuration read from the
//  JSON the executable carries, its ROMs resolved out of UnitTest/Fixtures,
//  and every device wired by MachineBuilder in the production order. Nothing
//  is faked and nothing is reimplemented -- the difference between this and
//  a running Casso is that no mixer is attached to the speaker and no thread
//  drains the printer, because MachineBuildServices leaves those out.
//
//  It replaces HeadlessHost, which built the same machines a second time
//  from a hand-copied wiring order. That copy could only ever be as right as
//  someone remembered to keep it, and it had already drifted: its power
//  cycle skipped the disk flush the real one does, and it composed no
//  machine at all for the ][ and ][+.
//
//  It IS a MachineHost rather than holding one, so a test reads the machine
//  it built without a hop through it and hands it to anything taking a
//  MachineHost. Nothing derives further and nothing deletes one through a
//  base pointer -- tests hold them by value.
//
//  The Prng is pinned, so two runs of the same test see the same power-on
//  memory.
//
////////////////////////////////////////////////////////////////////////////////

class TestMachine : public MachineHost
{
public:

    //  Pinned so a power cycle produces the same DRAM twice running.
    static constexpr uint64_t  kSeed = 0xCA550001ULL;

    //  What goes in the machine's card slots.
    //
    //  AsShipped is the machine a user gets, Disk ][ in slot 6 and all. A
    //  machine with a drive and no disk in it does what the real one does:
    //  the autostart ROM hands over to slot 6 and the drive spins, so it
    //  never reaches BASIC. A test that wants a prompt without mounting
    //  anything wants a machine with no disk card -- which is a machine a
    //  user can configure, not a fiction for testing.
    enum class Slots
    {
        AsShipped,
        Empty,
        DiskOnly,     // slot 6 and nothing else
    };

    //  `machineId` is a shipped machine's directory name: "Apple2",
    //  "Apple2Plus", "Apple2e", "Apple2eEnhanced" or "Apple2c". Asserts
    //  rather than failing softly -- a test whose machine did not build has
    //  nothing left to say.
    explicit TestMachine (const std::string & machineId, Slots slots = Slots::AsShipped);

private:

    static void  LoadConfig (const std::string & machineId, MachineConfig & outConfig);

    MachineBuildServices  m_nothingListening;
    MachineBuilder        m_builder;
};
