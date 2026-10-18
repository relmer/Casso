#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITapeControl
//
//  The cassette deck's keys for the TAPE command in the emulator, pressed
//  through the shell's tape manager, so a recording made before a stop or an
//  eject is saved and an inserted tape is the one the recorder widget draws.
//  CPU thread. A batch run has none, and TAPE calls the machine's deck
//  directly.
//
////////////////////////////////////////////////////////////////////////////////

class ITapeControl
{
public:
    virtual ~ITapeControl() = default;

    // Read and decoded in the background, and inserted once decoded; a file
    // that cannot be read is reported by the emulator's notice.
    virtual void  InsertTapeFile (const std::wstring & path) = 0;

    virtual void  PlayTape       () = 0;
    virtual void  StopTape       () = 0;
    virtual void  RewindTape     () = 0;
    virtual void  EjectTape      () = 0;
};
