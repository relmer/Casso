#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHookFilter
//
//  Which instructions MachineHost asks a hook about. An instruction on a page
//  marked in pages is asked about with ShouldStopBefore, and OnInstruction if
//  it runs; any other runs without the hook. While everyInstruction or
//  opcodesStop is set every page is marked, so the test before each
//  instruction is one load; with opcodesStop alone, only an instruction whose
//  opcode is marked in opcodes, or cannot be read without side effects, is
//  then actually asked.
//
//  Without everyInstruction, the hook promises that no stop is raised during
//  an instruction, so HasPendingStop is not asked either.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugHookFilter
{
    static constexpr size_t  kCount = 0x100;

    using Table = std::array<bool, kCount>;

    static constexpr Table  s_kAllMarked = [] { Table all {}; all.fill (true); return all; }();

    bool   everyInstruction = true;
    bool   opcodesStop      = false;
    Table  pages            = s_kAllMarked;
    Table  opcodes          = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHook
//
//  Consulted by MachineHost before the instructions its filter names while a
//  debugger has a stop condition or a run in progress. ShouldStopBefore stops the machine
//  before the instruction at pc executes. HasPendingStop reports a stop raised
//  during the instruction that just ran, such as a watchpoint hit, which takes
//  effect at the next instruction boundary. OnInstruction is told of each
//  instruction that is about to execute, after the stop test let it through.
//
//  A hook that sets no filter is asked about every instruction.
//
////////////////////////////////////////////////////////////////////////////////

class DebugHook
{
public:
    virtual ~DebugHook() = default;

    virtual bool  ShouldStopBefore (Word pc)  = 0;
    virtual bool  HasPendingStop   () const   = 0;
    virtual void  OnInstruction    (Word)     {}

    const DebugHookFilter &  GetFilter () const { return *m_filter; }

protected:
    static constexpr DebugHookFilter  s_kEveryInstruction {};

    void  SetFilter (const DebugHookFilter * filter) { m_filter = filter; }

private:
    const DebugHookFilter  * m_filter = &s_kEveryInstruction;
};
