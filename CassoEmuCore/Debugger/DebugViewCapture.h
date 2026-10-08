#pragma once

#include "Debugger/CallStack.h"
#include "Debugger/DebugMemoryImage.h"
#include "Debugger/IDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewCapture
//
//  What the debugger's panes read from the machine, copied at one moment on
//  the machine's thread so the panes can be built on any other: the
//  registers, the clocks and beam, the 64K the CPU sees, the soft switches,
//  the window of the instruction trace a pane shows, and the call record.
//  Take does only copying, so the machine's thread spends microseconds on it.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugViewCapture
{
    Cpu6502Registers          registers      = {};
    uint64_t                  cycleCount     = 0;
    VideoPosition             video;
    Byte                      lastPenalties  = 0;
    std::optional<Word>       lastBranch;
    DebugCpuKind              cpuKind        = DebugCpuKind::M6502;
    const Microcode         * instructionSet = nullptr;
    DebugMachineInfo          machineInfo;
    bool                      isKeyPending   = false;
    std::vector<SoftSwitch>   softSwitches;
    DebugMemoryImage          memory;

    bool                      isTraceOn      = false;
    size_t                    traceSize      = 0;
    size_t                    traceFirst     = 0;
    std::vector<TraceRecord>  trace;

    CallRecord                callRecord;

    //  Copies everything above from target, and count trace entries from
    //  traceFirst, cut at the end of the trace.
    static void  Take (const IDebugTarget  & target,
                       const CallRecord    & callRecord,
                       size_t                traceFirst,
                       size_t                traceCount,
                       DebugViewCapture    & out);
};
