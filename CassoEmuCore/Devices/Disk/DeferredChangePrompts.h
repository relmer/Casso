#pragma once

#include "Pch.h"

#include "ChangePrompt.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeferredChangePrompts
//
//  The reports and questions about a disk changed outside Casso that arrive
//  while the machine is behind live in history, put off until the machine is
//  live again. Behind live, the emulated past is drawn with each disk as it
//  was then, so news about the files as they are now is put off until the
//  present.
//
//  A later prompt of the same kind for the same bay replaces the earlier one
//  and moves to the end, as a later change rewords the banner already on
//  screen.
//
////////////////////////////////////////////////////////////////////////////////

class DeferredChangePrompts
{
public:
    enum class Kind
    {
        Report,
        Question,
    };

    struct Entry
    {
        Kind          kind  = Kind::Report;
        int           slot  = 0;
        int           drive = 0;
        ChangePrompt  prompt;
    };

    void                Defer   (Kind kind, int slot, int drive, const ChangePrompt & prompt);
    std::vector<Entry>  Take    ();
    bool                IsEmpty () const { return m_entries.empty(); }

private:
    std::vector<Entry>  m_entries;
};
