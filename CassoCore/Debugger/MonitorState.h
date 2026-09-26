#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorState
//
//  What the Apple II Monitor remembers between lines, and what its line scan
//  accumulates within one.
//
//  The Monitor is a stateful line scanner rather than a command parser: hex
//  digits accumulate into A2 and shift into A1 at a delimiter, `.` opens a
//  range, `:` puts it in store mode, `<` records a destination, and a bare
//  Return or space continues examining from wherever the last one stopped.
//  A command character acts on whatever the scan has accumulated so far, so
//  the same character means different things depending on what preceded it:
//  `S` alone steps, and `41<300.3FFS` searches.
//
//  The register-edit flag is a place where one line changes what the NEXT
//  line means, which is why it lives here rather than in the scan. The
//  assembler, the other such place, is the session's.
//
////////////////////////////////////////////////////////////////////////////////

struct MonitorState
{
    // Where an empty line or a space continues examining from.
    Word  lastExamined = 0;

    // Where `: bytes` with no address in front of it stores.
    Word  storeAddress = 0;

    // Set by `^E`: the next `: bytes` sets the registers rather than memory.
    bool  registerEditPending = false;
};
