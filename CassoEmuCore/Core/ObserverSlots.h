#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ObserverSlot
//
//  The two listeners a device's event feed can have at once: Panel, the
//  emulator's own debug windows, and History, the debugger's event cache on
//  the running machine or a rebuild's probe on a machine replaying history
//  (FR-211). A History listener gets what the machine itself does: guest
//  reads and device events, on the thread that runs the machine. Host
//  input (keys, axes, buttons, a key's auto-repeat) goes to Panel alone,
//  since a replay takes it from the input journal instead.
//
////////////////////////////////////////////////////////////////////////////////

enum class ObserverSlot
{
    Panel,
    History,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ObserverSlots
//
//  A device's listeners, one per ObserverSlot. IsAny is one load, so a
//  device with no listener pays a test and nothing more; ForEach calls each
//  listener present, Panel first.
//
////////////////////////////////////////////////////////////////////////////////

template <typename Sink>
class ObserverSlots
{
public:
    void  Set (ObserverSlot slot, Sink * sink) noexcept
    {
        m_sinks[(size_t) slot] = sink;
        m_isAny                = m_sinks[0] != nullptr || m_sinks[1] != nullptr;
    }

    Sink  * Get   (ObserverSlot slot) const noexcept { return m_sinks[(size_t) slot]; }
    bool    IsAny () const noexcept                  { return m_isAny; }

    template <typename Fn>
    void  ForEach (Fn && fn) const
    {
        for (Sink * sink : m_sinks)
        {
            if (sink != nullptr)
            {
                fn (*sink);
            }
        }
    }

private:
    std::array<Sink *, 2>  m_sinks = {};
    bool                   m_isAny = false;
};
