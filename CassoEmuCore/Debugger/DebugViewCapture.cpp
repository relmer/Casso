#include "Pch.h"

#include "Debugger/DebugViewCapture.h"

#include "Microcode.h"





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
    Word            branchFrom = 0;
    Word            branchTo   = 0;
    DebugDiskState  disk;



    out.registers      = target.GetRegisters();
    out.cycleCount     = target.GetCycleCount();
    out.video          = target.GetVideoPosition();
    out.lastPenalties  = target.GetLastPenalties();
    out.lastBranch     = target.TryGetLastBranch (branchFrom) ? std::optional<Word> (branchFrom) : std::nullopt;
    out.lastBranchTo   = target.TryGetLastBranchTarget (branchTo) ? std::optional<Word> (branchTo) : std::nullopt;
    out.cpuKind        = target.GetCpuKind();
    out.instructionTable = ShareInstructionSet (target.GetInstructionSet());
    out.instructionSet   = (out.instructionTable != nullptr) ? out.instructionTable->data() : nullptr;
    out.machineInfo    = target.GetMachineInfo();
    out.isKeyPending   = target.IsKeyPending();
    out.diskState      = target.TryGetDiskState (disk) ? std::optional<DebugDiskState> (disk) : std::nullopt;

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





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewCapture::ShareInstructionSet
//
//  The captures share one copy until the live set differs from it, as after
//  a machine switch, so the copy costs a compare, and a cache keyed by the
//  copy's address hits for every capture of the same set.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const std::vector<Microcode>> DebugViewCapture::ShareInstructionSet (const Microcode * live)
{
    static constexpr size_t                                kOpcodeCount = 256;
    static std::mutex                                      tableLock;
    static std::shared_ptr<const std::vector<Microcode>>   last;
    std::lock_guard<std::mutex>                            held (tableLock);



    if (live == nullptr)
    {
        return nullptr;
    }

    if (last == nullptr || memcmp (last->data(), live, kOpcodeCount * sizeof (Microcode)) != 0)
    {
        last = std::make_shared<const std::vector<Microcode>> (live, live + kOpcodeCount);
    }

    return last;
}
