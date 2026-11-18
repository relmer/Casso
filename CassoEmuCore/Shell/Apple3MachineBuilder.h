#pragma once

#include "Pch.h"

class MachineBuilder;
class MachineHost;
struct MachineBuildServices;
struct MachineConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3MachineBuilder
//
//  Builds an Apple /// on a MachineHost: its system ROM, the Apple3System
//  with its memory map over the bus, the CPU with extended addressing, the
//  keyboard and the built-in drive in the host's references, and the video
//  output. The Apple II video modes are built too, for the frame renderers
//  and for Apple II emulation mode.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3MachineBuilder
{
public:
    static constexpr size_t  kRomBytes = 4096;

    Apple3MachineBuilder (MachineHost & host, const MachineBuildServices & services, MachineBuilder & builder);

    HRESULT  Build (const MachineConfig & config);

private:
    HRESULT  CreateSystem   (const MachineConfig & config);
    void     CreateCpu      ();
    void     WireDiskDrive  ();
    void     WireSound      (const MachineConfig & config);
    void     CreateVideo    ();

    MachineHost                 & m_host;
    const MachineBuildServices  & m_services;
    MachineBuilder              & m_builder;
};
