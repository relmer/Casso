#pragma once

#include "Ui/Debugger/CodeViews.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/MemoryWindowViews.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OpenViewsText
//
//  Which optional views are open, as the text the preferences keep, so a
//  restart brings them back: `code2=E000 follow=2 memory3=0300 panel=mmu`.
//  A disassembly view keeps the address at its top, and a memory window its
//  first byte. The first disassembly view and the first memory window are
//  always open and are not written.
//
////////////////////////////////////////////////////////////////////////////////

class OpenViewsText
{
public:
    struct OpenViews
    {
        std::array<std::optional<Word>, CodeViews::kMaxCodeViews>              code;
        int                                                                    follow  = 0;
        std::array<std::optional<Word>, MemoryWindowViews::kMaxMemoryWindows>  memory;
        std::vector<std::string>                                               panels;
    };

    static std::string  FormatOpenViews (const DebuggerViewSnapshot & snapshot);
    static OpenViews    ParseOpenViews  (const std::string & text);

private:
    static constexpr int  kMaxCodeViews     = CodeViews::kMaxCodeViews;
    static constexpr int  kMaxMemoryWindows = MemoryWindowViews::kMaxMemoryWindows;
};
