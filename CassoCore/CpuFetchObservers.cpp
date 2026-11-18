#include "Pch.h"

#include "CpuFetchObservers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuFetchObservers::SetOpcodeWatch
//
////////////////////////////////////////////////////////////////////////////////

void CpuFetchObservers::SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher)
{
    bool  isOn = opcodes != nullptr && watcher != nullptr;



    m_watchOpcodes = isOn ? opcodes : nullptr;
    m_watcher      = isOn ? watcher : nullptr;
    m_watchFetch   = isOn ? opcodes : s_kNoOpcodes.data();

    Update();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuFetchObservers::ReportWatchedFetch
//
//  The fetch after a watched opcode is looked up in a table that has every
//  opcode, so the watcher gets the registers that opcode left.
//
////////////////////////////////////////////////////////////////////////////////

void CpuFetchObservers::ReportWatchedFetch (Byte opcode, Word pc, Byte sp)
{
    m_watchFetch = m_watchOpcodes[opcode] ? s_kAllOpcodes.data() : m_watchOpcodes;
    m_watcher->OnWatchedFetch (pc, sp, opcode);
}
