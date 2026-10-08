#include "Pch.h"

#include "Debugger/DebugViewCapture.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewCapture::Take
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewCapture::Take (
    const IDebugTarget  & target,
    const CallRecord    & callRecord,
    size_t                traceFirst,
    size_t                traceCount,
    DebugViewCapture    & out)
{
    Word  branchFrom = 0;



    out.registers      = target.GetRegisters();
    out.cycleCount     = target.GetCycleCount();
    out.video          = target.GetVideoPosition();
    out.lastPenalties  = target.GetLastPenalties();
    out.lastBranch     = target.TryGetLastBranch (branchFrom) ? std::optional<Word> (branchFrom) : std::nullopt;
    out.cpuKind        = target.GetCpuKind();
    out.instructionSet = target.GetInstructionSet();
    out.machineInfo    = target.GetMachineInfo();
    out.isKeyPending   = target.IsKeyPending();

    out.softSwitches.clear();
    target.GetSoftSwitches (out.softSwitches);
    target.TakeMemoryImage (out.memory);

    out.isTraceOn  = target.IsTraceOn();
    out.traceSize  = target.GetTraceSize();
    out.traceFirst = (std::min) (traceFirst, out.traceSize);

    out.trace.clear();
    target.GetTraceWindow (out.traceFirst, traceCount, out.trace);

    out.callRecord = callRecord;
}
