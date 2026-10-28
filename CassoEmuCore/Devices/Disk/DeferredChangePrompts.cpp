#include "Pch.h"

#include "DeferredChangePrompts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeferredChangePrompts::Defer
//
////////////////////////////////////////////////////////////////////////////////

void DeferredChangePrompts::Defer (
    Kind                  kind,
    int                   slot,
    int                   drive,
    const ChangePrompt  & prompt)
{
    std::erase_if (m_entries, [&] (const Entry & entry)
    {
        return entry.kind == kind && entry.slot == slot && entry.drive == drive;
    });

    m_entries.push_back (Entry { kind, slot, drive, prompt });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeferredChangePrompts::Take
//
//  The prompts in the order they arrived, leaving none.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DeferredChangePrompts::Entry> DeferredChangePrompts::Take()
{
    std::vector<Entry>  taken;



    taken.swap (m_entries);

    return taken;
}
