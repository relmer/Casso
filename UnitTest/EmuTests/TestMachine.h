#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/MachineConfig.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"





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
//  The Prng is pinned, so two runs of the same test see the same power-on
//  memory.
//
////////////////////////////////////////////////////////////////////////////////

class TestMachine
{
public:

    //  Pinned so a power cycle produces the same DRAM twice running.
    static constexpr uint64_t  kSeed = 0xCA550001ULL;

    //  `machineId` is a shipped machine's directory name: "Apple2",
    //  "Apple2Plus", "Apple2e", "Apple2eEnhanced" or "Apple2c". Asserts
    //  rather than failing softly -- a test whose machine did not build has
    //  nothing left to say.
    explicit TestMachine (const std::string & machineId);

    MachineHost        &  GetHost   ()       noexcept { return m_host; }
    const MachineConfig &  GetConfig () const noexcept { return m_config; }

    //  Reads as the machine it is: machine->RunCycles (n), machine->GetCpu().
    MachineHost *  operator-> () noexcept { return &m_host; }

private:

    static void  LoadConfig (const std::string & machineId, MachineConfig & outConfig);

    MachineHost           m_host;
    MachineBuildServices  m_nothingListening;
    MachineBuilder        m_builder;
    MachineConfig         m_config;
};
