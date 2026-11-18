#include "Pch.h"

#include "Ui/Debugger/LatestSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LatestSnapshot::UpdateStopChanges
//
////////////////////////////////////////////////////////////////////////////////

void LatestSnapshot::UpdateStopChanges()
{
    StopChanges::Values  values;



    for (const DebuggerViewSnapshot::RegisterRow & reg : m_snapshot->registers)
    {
        values["R:" + reg.name] = reg.value;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
    {
        values[std::format ("W:{}", watch.id)] = watch.value;
    }

    for (const DebuggerViewSnapshot::AutoWatchLine & line : m_snapshot->autoWatches)
    {
        values["A:" + line.key] = line.value;
    }

    for (const auto & entry : m_snapshot->stack)
    {
        values[std::format ("S:{:04X}", entry.address)] = std::format ("{:02X}", entry.value);
    }

    m_stopChanges.Update (m_snapshot->isPaused, m_snapshot->cycles, std::move (values));
}
