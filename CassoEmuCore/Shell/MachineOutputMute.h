#pragma once

#include "Pch.h"

class IDriveAudioSink;
class MachineConnectors;
struct MachineParts;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineOutputMute
//
//  Silences what the machine sends to the host while it runs: speaker
//  clicks and the Disk II drive sounds, and printer bytes, together or
//  apart. Reverse execution mutes a replay that runs to a point, since its
//  sound and printing happened live; a replay that runs on at the machine's
//  pace has its sound again but doesn't print again.
//
////////////////////////////////////////////////////////////////////////////////

class MachineOutputMute
{
public:
    MachineOutputMute (MachineParts & parts, MachineConnectors & connectors);

    MachineOutputMute (const MachineOutputMute &)             = delete;
    MachineOutputMute & operator= (const MachineOutputMute &) = delete;

    void  SetMuted (bool isMuted) { SetMuted (isMuted, isMuted); }
    void  SetMuted (bool isSoundMuted, bool isPrinterMuted);

    bool  IsMuted        () const noexcept { return m_isSoundMuted || m_isPrinterMuted; }
    bool  IsSoundMuted   () const noexcept { return m_isSoundMuted; }
    bool  IsPrinterMuted () const noexcept { return m_isPrinterMuted; }

private:
    MachineParts       & m_parts;
    MachineConnectors  & m_connectors;
    IDriveAudioSink    * m_mutedDiskAudio   = nullptr;   // the Disk II's sound sink, stored here while muted
    bool                 m_isSoundMuted     = false;
    bool                 m_isPrinterMuted   = false;
    bool                 m_wasMotorOnAtMute = false;
};
