#pragma once

#include "Debugger/IDebugCommandHandler.h"

class IDebugTarget;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers
//
//  View and enter: D, MDB, ME, MEB, MEW, ME8, ME16 and the addr:bytes form.
//  Move, compare, fill: M, MM, MC, F. Search: S, MS, SH, @. Files: BLOAD,
//  BSAVE, TSAVE. I/O: IN, INPUT, OUT. And SWITCHES.
//
//  D with no range continues from where the last dump ended.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    // Rows of eight from first to last, with each row's region.
    static MemoryData  MakeRows (IDebugTarget & target, Word first, Word last);

private:
    static constexpr int   kBytesPerRow     = 8;
    static constexpr int   kDefaultDump     = 64;
    static constexpr Word  kAddressSpace    = 0xFFFF;
    static constexpr Word  kTextPage        = 0x0400;
    static constexpr int   kTextRows        = 24;
    static constexpr int   kTextColumns     = 40;
    static constexpr int   kTextRowGroups   = 8;
    static constexpr Word  kTextGroupStride = 0x0080;
    static constexpr Word  kTextRowOffset   = 0x0028;

    void         Dump         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Enter        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Move         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Compare      (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Fill         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Search       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ShowResults  (DebugSession & session, Reply & reply);
    static void  LoadBinary   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SaveBinary   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SaveText     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ReadIo       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  WriteIo      (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ShowSwitches (DebugSession & session, Reply & reply);
    static void  Patch        (DebugSession & session, const DebugCommand & command, Reply & reply);

    static bool  TryPokeRange (IDebugTarget & target, Word first, std::span<const Byte> bytes, Reply & reply);
    static bool  TryGetFiles  (DebugSession & session, Reply & reply, IFileSystem *& files);
    static Word  GetLast      (const DebugCommand & command);
    static Byte  Peek         (IDebugTarget & target, Word address);

    Word  m_nextDump = 0;
};
